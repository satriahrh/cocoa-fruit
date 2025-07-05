package http

import (
	"context"
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/labstack/echo/v4"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/domain"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
)

const (
	// JWT settings
	JWTSecretKey = "your-super-secret-jwt-key-change-in-production"
	JWTExpiry    = 24 * time.Hour

	// Rate limiting
	MaxRequestSize = 10 * 1024 * 1024 // 10MB
	MaxConcurrent  = 10

	// Audio settings
	MaxAudioDuration = 60 * time.Second
	SupportedFormats = "wav,mp3,flac,m4a"

	// Message broker settings
	TranscriptionTopic = "transcription.results"
)

type AudioHandler struct {
	chatService   *usecase.ChatService
	speechService *speech.GoogleSpeech
	messageBroker domain.MessageBroker
	wsHub         *websocket.Hub
	jwtSecret     []byte
}

type AudioUploadRequest struct {
	DeviceID      string `json:"device_id"`
	DeviceVersion string `json:"device_version"`
	AudioFormat   string `json:"audio_format"`
	SampleRate    int    `json:"sample_rate"`
	Channels      int    `json:"channels"`
}

type StreamAudioResponse struct {
	Success   bool   `json:"success"`
	Message   string `json:"message"`
	SessionID string `json:"session_id,omitempty"`
	Text      string `json:"text,omitempty"`
}

type JWTClaims struct {
	UserID        int    `json:"user_id"`
	DeviceID      string `json:"device_id"`
	DeviceVersion string `json:"device_version"`
	jwt.RegisteredClaims
}

func NewAudioHandler(chatService *usecase.ChatService, speechService *speech.GoogleSpeech, messageBroker domain.MessageBroker, wsHub *websocket.Hub) *AudioHandler {
	return &AudioHandler{
		chatService:   chatService,
		speechService: speechService,
		messageBroker: messageBroker,
		wsHub:         wsHub,
		jwtSecret:     []byte(JWTSecretKey),
	}
}

// GenerateJWT creates a JWT token for authenticated clients
func (h *AudioHandler) GenerateJWT(c echo.Context) error {
	// Basic auth for token generation
	username := c.Request().Header.Get("X-API-Key")
	password := c.Request().Header.Get("X-API-Secret")

	if username != "John" || password != "Doe" {
		return echo.NewHTTPError(http.StatusUnauthorized, "Invalid credentials")
	}

	// Generate claims
	claims := &JWTClaims{
		UserID:        99,
		DeviceID:      "12312",
		DeviceVersion: "0.1.0",
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(JWTExpiry)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
			NotBefore: jwt.NewNumericDate(time.Now()),
			Issuer:    "cocoa-fruit-agentic",
			Subject:   "audio-streaming",
		},
	}

	// Create token
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString(h.jwtSecret)
	if err != nil {
		log.Printf("Error signing JWT: %v", err)
		return echo.NewHTTPError(http.StatusInternalServerError, "Failed to generate token")
	}

	return c.JSON(http.StatusOK, map[string]string{
		"token": tokenString,
		"type":  "Bearer",
	})
}

// JWT middleware for authentication
func (h *AudioHandler) JWTMiddleware(next echo.HandlerFunc) echo.HandlerFunc {
	return func(c echo.Context) error {
		authHeader := c.Request().Header.Get("Authorization")
		if authHeader == "" {
			return echo.NewHTTPError(http.StatusUnauthorized, "Missing authorization header")
		}

		// Extract token from "Bearer <token>"
		tokenString := strings.TrimPrefix(authHeader, "Bearer ")
		if tokenString == authHeader {
			return echo.NewHTTPError(http.StatusUnauthorized, "Invalid authorization format")
		}

		// Parse and validate token
		token, err := jwt.ParseWithClaims(tokenString, &JWTClaims{}, func(token *jwt.Token) (interface{}, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
			}
			return h.jwtSecret, nil
		})

		if err != nil {
			log.Printf("JWT validation error: %v", err)
			return echo.NewHTTPError(http.StatusUnauthorized, "Invalid token")
		}

		if claims, ok := token.Claims.(*JWTClaims); ok && token.Valid {
			// Set claims in context
			c.Set("user_id", claims.UserID)
			c.Set("device_id", claims.DeviceID)
			c.Set("device_version", claims.DeviceVersion)
			return next(c)
		}

		return echo.NewHTTPError(http.StatusUnauthorized, "Invalid token claims")
	}
}

// Rate limiting middleware
func (h *AudioHandler) RateLimitMiddleware(next echo.HandlerFunc) echo.HandlerFunc {
	semaphore := make(chan struct{}, MaxConcurrent)
	return func(c echo.Context) error {
		select {
		case semaphore <- struct{}{}:
			defer func() { <-semaphore }()
			return next(c)
		default:
			return echo.NewHTTPError(http.StatusTooManyRequests, "Too many concurrent requests")
		}
	}
}

// StreamAudio handles chunked audio streaming
func (h *AudioHandler) StreamAudio(c echo.Context) error {
	startTime := time.Now()

	// Validate content type
	contentType := c.Request().Header.Get("Content-Type")
	if !strings.HasPrefix(contentType, "audio/") && !strings.HasPrefix(contentType, "application/octet-stream") {
		return echo.NewHTTPError(http.StatusBadRequest, "Invalid content type. Expected audio/* or application/octet-stream")
	}

	// Get device info from JWT context
	deviceID := c.Get("device_id").(string)
	userID := c.Get("user_id").(int)

	// Generate session ID
	sessionID, err := h.generateSessionID()
	if err != nil {
		log.Printf("Error generating session ID: %v", err)
		return echo.NewHTTPError(http.StatusInternalServerError, "Failed to create session")
	}

	log.Printf("Starting streaming audio session %s for device %s (user %d)", sessionID, deviceID, userID)

	// Create channels for streaming transcription
	audioChunkChan := make(chan []byte, 100) // Buffered channel for audio chunks

	// Create context with timeout
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	type TranscriptionResult struct {
		text string
		err  error
	}
	resultChan := make(chan TranscriptionResult)
	go func() {
		text, err := h.speechService.TranscribeStreaming(ctx, audioChunkChan)
		resultChan <- TranscriptionResult{text, err}
	}()

	// Start goroutine to send audio chunks
	go func() {
		defer close(audioChunkChan)
		startTime := time.Now()

		for {
			// Read chunk with timeout
			chunk := make([]byte, 4096) // 4KB chunks
			n, err := c.Request().Body.Read(chunk)

			if err != nil {
				if err == io.EOF {
					log.Printf("End of audio stream for session %s", sessionID)
					break
				}
				log.Printf("Error reading audio chunk: %v", err)
				break
			}

			if n > 0 {
				// Copy chunk and send to transcription
				chunkCopy := make([]byte, n)
				copy(chunkCopy, chunk[:n])

				select {
				case audioChunkChan <- chunkCopy:
					log.Printf("Sent %d bytes chunk for session %s", n, sessionID)
				case <-ctx.Done():
					log.Printf("Context cancelled while sending chunks for session %s", sessionID)
					return
				}
			}

			// Check if we've exceeded max duration
			if time.Since(startTime) > MaxAudioDuration {
				log.Printf("Audio stream exceeded max duration for session %s", sessionID)
				break
			}
		}
	}()

	// Wait for transcription to complete
	var transcription string
	select {
	case result := <-resultChan:
		if result.err != nil {
			log.Printf("Streaming transcription error for session %s: %v", sessionID, result.err)
			return echo.NewHTTPError(http.StatusInternalServerError, "Failed to transcribe audio")
		}
		transcription = result.text
		// Transcription completed successfully
		log.Printf("Streaming transcription completed for session %s", sessionID)

	case <-ctx.Done():
		log.Printf("Context timeout for session %s", sessionID)
		return echo.NewHTTPError(http.StatusRequestTimeout, "Transcription timeout")
	}

	log.Printf("Completed audio stream for session %s over %v", sessionID, time.Since(time.Now()))

	// Publish transcription result to message broker
	transcriptionMsg := domain.TranscriptionMessage{
		SessionID: sessionID,
		UserID:    userID,
		DeviceID:  deviceID,
		Text:      transcription,
		Success:   true,
		Timestamp: startTime,
	}

	// Convert to JSON
	payload, err := json.Marshal(transcriptionMsg)
	if err != nil {
		log.Printf("Error marshaling transcription message: %v", err)
	} else {
		log.Printf("📤 Publishing to message broker: topic=%s, routing_key=%s, device_id=%s",
			TranscriptionTopic, sessionID, deviceID)

		// Publish to message broker with sessionID as routing key
		err = h.messageBroker.Publish(ctx, TranscriptionTopic, sessionID, payload)
		if err != nil {
			log.Printf("Error publishing transcription result: %v", err)
			// Don't fail the request, just log the error
		} else {
			log.Printf("✅ Successfully published to message broker")
		}
	}
	if err != nil {
		log.Printf("Error publishing transcription result: %v", err)
		// Don't fail the request, just log the error
	}

	// Return the transcribed text
	return c.JSON(http.StatusOK, StreamAudioResponse{
		Success:   true,
		Message:   "Audio processed successfully",
		SessionID: sessionID,
		Text:      transcription,
	})
}

// Health check endpoint
func (h *AudioHandler) HealthCheck(c echo.Context) error {
	return c.JSON(http.StatusOK, map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now().UTC(),
		"service":   "audio-streaming",
	})
}

// generateSessionID creates a unique session identifier
func (h *AudioHandler) generateSessionID() (string, error) {
	bytes := make([]byte, 16)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return fmt.Sprintf("%x", bytes), nil
}
