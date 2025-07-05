package websocket

import (
	"fmt"

	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
)

type Hub struct {
	clients    map[*Client]bool
	register   chan *Client
	unregister chan *Client
}

// NewHub creates a new WebSocket hub
func NewHub() *Hub {
	return &Hub{
		clients:    make(map[*Client]bool),
		register:   make(chan *Client),
		unregister: make(chan *Client),
	}
}

// Run starts the hub
func (h *Hub) Run() {
	go h.run()
}

func (h *Hub) run() {
	for {
		select {
		case client := <-h.register:
			h.clients[client] = true
			log.WithCtx(client.ctx).Debug("New client registered")

		case client := <-h.unregister:
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				client.Close() // Use the proper Close method
				log.WithCtx(client.ctx).Debug("Client unregistered")
			}
		}
	}
}

// Register adds a client to the hub
func (h *Hub) Register(client *Client) {
	h.register <- client
}

// Unregister removes a client from the hub
func (h *Hub) Unregister(client *Client) {
	h.unregister <- client
}

// Broadcast sends a message to all connected clients
func (h *Hub) Broadcast(message []byte) {
	for client := range h.clients {
		if !client.IsClosed() {
			client.SendMessage(message)
		}
	}
}

// SendToDevice sends a message to a specific client by device ID
func (h *Hub) SendToDevice(deviceID string, message []byte) error {
	for client := range h.clients {
		if client.deviceID == deviceID && !client.IsClosed() {
			return client.SendMessage(message)
		}
	}
	return fmt.Errorf("client with device ID %s not found", deviceID)
}

// GetClientByDevice returns a client by device ID
func (h *Hub) GetClientByDevice(deviceID string) *Client {
	for client := range h.clients {
		if client.deviceID == deviceID && !client.IsClosed() {
			return client
		}
	}
	return nil
}

// IsDeviceConnected checks if a device is already connected
func (h *Hub) IsDeviceConnected(deviceID string) bool {
	return h.GetClientByDevice(deviceID) != nil
}

// ClientCount returns the number of connected clients
func (h *Hub) ClientCount() int {
	return len(h.clients)
}
