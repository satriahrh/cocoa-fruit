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

	// Subscribe to all transcription messages (using wildcard subscription)
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

			// Get the client by device id
			client := s.hub.GetClientByDevice(transcriptionMsg.DeviceID)
			if client == nil {
				log.WithCtx(ctx).Warn("⚠️ Device not connected, ignoring transcription",
					zap.String("device_id", transcriptionMsg.DeviceID),
					zap.String("session_id", transcriptionMsg.SessionID),
					zap.String("text", transcriptionMsg.Text))
				continue
			}

			// Send transcription to chat service
			if err := client.SendInput(transcriptionMsg.Text); err != nil {
				log.WithCtx(ctx).Error("❌ Failed to send transcription to chat service",
					zap.String("device_id", transcriptionMsg.DeviceID),
					zap.String("session_id", transcriptionMsg.SessionID),
					zap.String("text", transcriptionMsg.Text),
					zap.Error(err))
				continue
			}

			log.WithCtx(ctx).Info("📤 Sent transcription to chat service",
				zap.String("device_id", transcriptionMsg.DeviceID),
				zap.String("session_id", transcriptionMsg.SessionID),
				zap.String("text", transcriptionMsg.Text))

		case <-ctx.Done():
			log.WithCtx(ctx).Info("🔒 Transcription listener stopped")
			return
		}
	}
}
