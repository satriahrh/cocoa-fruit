package domain

import (
	"context"
	"time"
)

// MessageBroker defines the interface for message broker operations
type MessageBroker interface {
	// Publish sends a message to a specific topic/channel with a routing key
	Publish(ctx context.Context, topic string, routingKey string, message []byte) error

	// Subscribe listens for messages on a specific topic/channel and routing key
	Subscribe(ctx context.Context, topic string, routingKey string) (<-chan Message, error)

	// Close closes the message broker connection
	Close() error
}

// Message represents a message received from the broker
type Message struct {
	Topic      string
	RoutingKey string
	Payload    []byte
	Timestamp  time.Time
}

// TranscriptionMessage represents a transcription result message
type TranscriptionMessage struct {
	SessionID string    `json:"session_id"`
	UserID    int       `json:"user_id"`
	DeviceID  string    `json:"device_id"`
	Text      string    `json:"text"`
	Timestamp time.Time `json:"timestamp"`
	Success   bool      `json:"success"`
	Error     string    `json:"error,omitempty"`
}
