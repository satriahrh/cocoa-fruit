package websocket

import (
	"context"
	"encoding/json"
	"net/http"

	"github.com/gorilla/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/tts"
	"github.com/satriahrh/cocoa-fruit/agentic/domain"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

const (
	TranscriptionTopic = "transcription.results"
)

type Server struct {
	upgrader      websocket.Upgrader
	svc           *usecase.ChatService
	googleTTS     *tts.GoogleTTS
	googleSpeech  *speech.GoogleSpeech
	messageBroker domain.MessageBroker
	hub           *Hub
}

func NewServer(svc *usecase.ChatService, googleTTS *tts.GoogleTTS, googleSpeech *speech.GoogleSpeech, messageBroker domain.MessageBroker) *Server {
	hub := NewHub()

	server := &Server{
		upgrader:      websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }},
		svc:           svc,
		googleTTS:     googleTTS,
		googleSpeech:  googleSpeech,
		messageBroker: messageBroker,
		hub:           hub,
	}

	// Start listening to transcription messages from the broker
	go server.startTranscriptionListener()

	return server
}

func (s *Server) RunWebsocketHub() {
	s.hub.Run()
}

func (s *Server) GetHub() *Hub {
	return s.hub
}

// startTranscriptionListener listens for transcription messages and broadcasts them to WebSocket clients
func (s *Server) startTranscriptionListener() {
	ctx := context.Background()

	// Subscribe to all transcription messages (using wildcard or empty routing key for now)
	messageChan, err := s.messageBroker.Subscribe(ctx, TranscriptionTopic, "")
	if err != nil {
		log.WithCtx(ctx).Error("❌ Failed to subscribe to transcription topic", zap.Error(err))
		return
	}

	log.WithCtx(ctx).Info("🎧 WebSocket server listening to transcription messages")

	for {
		select {
		case msg := <-messageChan:
			// Process the transcription message
			var transcriptionMsg domain.TranscriptionMessage
			err := json.Unmarshal(msg.Payload, &transcriptionMsg)
			if err != nil {
				log.WithCtx(ctx).Error("❌ Failed to unmarshal transcription message", zap.Error(err))
				continue
			}

			// Create WebSocket message
			wsMessage := map[string]interface{}{
				"type":       "transcription",
				"session_id": transcriptionMsg.SessionID,
				"user_id":    transcriptionMsg.UserID,
				"device_id":  transcriptionMsg.DeviceID,
				"text":       transcriptionMsg.Text,
				"timestamp":  transcriptionMsg.Timestamp,
				"success":    transcriptionMsg.Success,
			}

			// Convert to JSON
			jsonData, err := json.Marshal(wsMessage)
			if err != nil {
				log.WithCtx(ctx).Error("❌ Failed to marshal WebSocket message", zap.Error(err))
				continue
			}

			// Broadcast to all WebSocket clients
			s.hub.Broadcast(jsonData)
			log.WithCtx(ctx).Info("📤 Broadcasted transcription to WebSocket clients",
				zap.String("session_id", transcriptionMsg.SessionID),
				zap.String("text", transcriptionMsg.Text))

		case <-ctx.Done():
			log.WithCtx(ctx).Info("🔒 Transcription listener stopped")
			return
		}
	}
}
