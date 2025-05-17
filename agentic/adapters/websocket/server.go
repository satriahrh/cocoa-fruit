package websocket

import (
	"net/http"

	"github.com/gorilla/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
)

type Server struct {
	upgrader websocket.Upgrader
	svc      *usecase.ChatService
}

func NewServer(svc *usecase.ChatService) *Server {
	return &Server{
		upgrader: websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }},
		svc:      svc,
	}
}
