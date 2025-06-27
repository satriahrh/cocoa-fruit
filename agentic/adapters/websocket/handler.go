package websocket

import (
	"github.com/labstack/echo/v4"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

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

	log.WithCtx(c.Request().Context()).Info("🔗 New doll connected",
		zap.Int("user_id", userID),
		zap.String("device_id", deviceID),
		zap.String("device_version", deviceVersion),
		zap.String("remote_addr", c.Request().RemoteAddr))

	client := NewClient(conn, userID, deviceID, deviceVersion, s.svc)
	s.hub.Register(client)

	// Start the client goroutines
	client.Run()

	// Register cleanup when client is done
	defer s.hub.Unregister(client)

	// Wait for the client context to be done (connection closed)
	<-client.Context().Done()

	return nil
}
