package websocket

import (
	"net/http"

	"github.com/gorilla/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/tts"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
)

type Server struct {
	upgrader     websocket.Upgrader
	svc          *usecase.ChatService
	googleTTS    *tts.GoogleTTS
	googleSpeech *speech.GoogleSpeech
	hub          *Hub
}

func NewServer(svc *usecase.ChatService, googleTTS *tts.GoogleTTS, googleSpeech *speech.GoogleSpeech) *Server {
	hub := NewHub()

	return &Server{
		upgrader:     websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }},
		svc:          svc,
		googleTTS:    googleTTS,
		googleSpeech: googleSpeech,
		hub:          hub,
	}
}

func (s *Server) RunWebsocketHub() {
	s.hub.Run()
}

func (s *Server) GetHub() *Hub {
	return s.hub
}
