package websocket

import (
	"fmt"
	"strings"

	"github.com/golang-jwt/jwt/v5"
	"github.com/labstack/echo/v4"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

// JWTClaims matches the claims structure used in HTTP handler
type JWTClaims struct {
	UserID        int    `json:"user_id"`
	DeviceID      string `json:"device_id"`
	DeviceVersion string `json:"device_version"`
	jwt.RegisteredClaims
}

// JWTMiddleware validates JWT tokens for WebSocket connections
func (s *Server) JWTMiddleware(next echo.HandlerFunc) echo.HandlerFunc {
	return func(c echo.Context) error {
		// Check for JWT token in query parameters (common for WebSocket)
		tokenString := c.QueryParam("token")

		// If not in query, check Authorization header
		if tokenString == "" {
			authHeader := c.Request().Header.Get("Authorization")
			if authHeader != "" {
				tokenString = strings.TrimPrefix(authHeader, "Bearer ")
			}
		}

		if tokenString == "" {
			log.WithCtx(c.Request().Context()).Error("❌ Missing JWT token for WebSocket connection")
			return echo.NewHTTPError(401, "Missing JWT token")
		}

		// Parse and validate token
		token, err := jwt.ParseWithClaims(tokenString, &JWTClaims{}, func(token *jwt.Token) (interface{}, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
			}
			// Use the same secret as HTTP handler
			return []byte("your-super-secret-jwt-key-change-in-production"), nil
		})

		if err != nil {
			log.WithCtx(c.Request().Context()).Error("❌ JWT validation error for WebSocket", zap.Error(err))
			return echo.NewHTTPError(401, "Invalid JWT token")
		}

		if claims, ok := token.Claims.(*JWTClaims); ok && token.Valid {
			// Set claims in context
			c.Set("user_id", claims.UserID)
			c.Set("device_id", claims.DeviceID)
			c.Set("device_version", claims.DeviceVersion)
			return next(c)
		}

		log.WithCtx(c.Request().Context()).Error("❌ Invalid JWT claims for WebSocket")
		return echo.NewHTTPError(401, "Invalid JWT claims")
	}
}

// Handler returns an http.Handler for your "/ws" endpoint.
func (s *Server) Handler(c echo.Context) error {
	conn, err := s.upgrader.Upgrade(c.Response(), c.Request(), nil)
	if err != nil {
		log.WithCtx(c.Request().Context()).Error("❌ Failed to upgrade connection to WebSocket", zap.Error(err))
		return err
	}

	userID := c.Get("user_id").(int)
	deviceID := c.Get("device_id").(string)
	deviceVersion := c.Get("device_version").(string)

	// Check if device is already connected
	if s.hub.IsDeviceConnected(deviceID) {
		log.WithCtx(c.Request().Context()).Warn("⚠️ Device already connected, rejecting duplicate connection",
			zap.String("device_id", deviceID),
			zap.String("remote_addr", c.Request().RemoteAddr))
		conn.Close()
		return echo.NewHTTPError(409, "Device already connected")
	}

	log.WithCtx(c.Request().Context()).Info("🔗 New doll connected",
		zap.Int("user_id", userID),
		zap.String("device_id", deviceID),
		zap.String("device_version", deviceVersion),
		zap.String("remote_addr", c.Request().RemoteAddr))

	client := NewClient(conn, userID, deviceID, deviceVersion, s.svc, s.googleSpeech)
	s.hub.Register(client)

	// Start the client goroutines
	client.Run()

	// Register cleanup when client is done
	defer s.hub.Unregister(client)

	// Wait for the client context to be done (connection closed)
	<-client.Context().Done()

	return nil
}
