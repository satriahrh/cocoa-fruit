package main

import (
	"flag"
	"log"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/http"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/llm"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/message_broker"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/tts"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
	"github.com/subosito/gotenv"
)

func main() {
	gotenv.Load()

	voiceName := flag.String("voice", "id-ID-Wavenet-B", "Google TTS voice name")
	languageCode := flag.String("lang", "id-ID", "Google TTS language code")
	audioEncoding := flag.String("encoding", "LINEAR16", "Audio encoding (LINEAR16, MULAW, ALAW)")
	sampleRate := flag.Int("samplerate", 16000, "Sample rate in Hz")
	flag.Parse()

	// Initialize message broker (channel-based for now)
	messageBroker := message_broker.NewChannelMessageBroker()
	defer messageBroker.Close()

	// Initialize services
	geminiLlm := llm.NewGeminiClient()
	svc := usecase.NewChatService(geminiLlm)
	googleTTS := tts.NewGoogleTTS(
		tts.GoogleTTSConfig{
			VoiceName:     *voiceName,
			LanguageCode:  *languageCode,
			AudioEncoding: *audioEncoding,
			SampleRate:    int32(*sampleRate),
		},
	)
	googleSpeech := speech.NewGoogleSpeech()

	// Initialize WebSocket server with message broker
	server := websocket.NewServer(svc, googleTTS, googleSpeech, messageBroker)
	go server.RunWebsocketHub()

	// Create HTTP audio handler with message broker
	audioHandler := http.NewAudioHandler(svc, googleSpeech, messageBroker, server.GetHub())

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
	log.Println("Message broker: Channel-based (single pod)")
	log.Println("Flow: HTTP -> Message Broker -> WebSocket")
	log.Fatal(e.Start(":8080"))
}
