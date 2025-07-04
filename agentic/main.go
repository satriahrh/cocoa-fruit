package main

import (
	"log"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/http"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/llm"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/tts"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
	"github.com/subosito/gotenv"
)

func main() {
	gotenv.Load()

	geminiLlm := llm.NewGeminiClient()
	svc := usecase.NewChatService(geminiLlm)
	googleTTS := tts.NewGoogleTTS()
	googleSpeech := speech.NewGoogleSpeech()

	server := websocket.NewServer(svc, googleTTS, googleSpeech)
	go server.RunWebsocketHub()

	// Create HTTP audio handler with WebSocket hub
	audioHandler := http.NewAudioHandler(svc, googleSpeech, server.GetHub())

	e := echo.New()

	// Security middleware
	e.Use(middleware.Logger())
	e.Use(middleware.Recover())
	e.Use(middleware.Secure())
	e.Use(middleware.RateLimiter(middleware.NewRateLimiterMemoryStore(20))) // 20 requests per minute

	// CORS configuration for ESP32
	e.Use(middleware.CORSWithConfig(middleware.CORSConfig{
		AllowOrigins: []string{"*"}, // In production, specify exact origins
		AllowMethods: []string{echo.GET, echo.POST, echo.PUT, echo.DELETE, echo.OPTIONS},
		AllowHeaders: []string{
			echo.HeaderOrigin,
			echo.HeaderContentType,
			echo.HeaderAccept,
			echo.HeaderAuthorization,
			"X-API-Key",
			"X-API-Secret",
			"Content-Length",
		},
		AllowCredentials: true,
		MaxAge:           86400, // 24 hours
	}))

	// Request size limit
	e.Use(middleware.BodyLimit("10MB"))

	// JWT auth for WebSocket (same as HTTP)
	wsGroup := e.Group("/ws")
	wsGroup.Use(server.JWTMiddleware)
	wsGroup.GET("", server.Handler)

	// HTTP API routes
	api := e.Group("/api/v1")

	// Public endpoints (no auth required)
	api.GET("/health", audioHandler.HealthCheck)
	api.POST("/auth/token", audioHandler.GenerateJWT)

	// Audio endpoints (JWT auth required)
	audio := api.Group("/audio")
	audio.Use(audioHandler.JWTMiddleware)
	audio.Use(audioHandler.RateLimitMiddleware)

	// Audio streaming endpoints
	audio.POST("/stream", audioHandler.StreamAudio) // Chunked streaming

	log.Println("Starting server on :8080")
	log.Println("Available endpoints:")
	log.Println("  GET  /api/v1/health              - Health check")
	log.Println("  POST /api/v1/auth/token          - Get JWT token")
	log.Println("  POST /api/v1/audio/stream        - Stream audio (JWT required)")
	log.Println("  GET  /ws                         - WebSocket (JWT required)")
	log.Fatal(e.Start(":8080"))
}
