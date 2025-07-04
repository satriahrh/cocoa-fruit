package message_broker

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/satriahrh/cocoa-fruit/agentic/domain"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

// ChannelMessageBroker implements MessageBroker using Go channels
type ChannelMessageBroker struct {
	topics map[string]chan domain.Message
	mu     sync.RWMutex
	closed bool
}

// NewChannelMessageBroker creates a new channel-based message broker
func NewChannelMessageBroker() *ChannelMessageBroker {
	return &ChannelMessageBroker{
		topics: make(map[string]chan domain.Message),
	}
}

// makeKey creates a unique key for topic and routingKey
func makeKey(topic, routingKey string) string {
	return topic + ":" + routingKey
}

// Publish sends a message to a specific topic and routing key
func (b *ChannelMessageBroker) Publish(ctx context.Context, topic string, routingKey string, message []byte) error {
	b.mu.RLock()
	defer b.mu.RUnlock()

	if b.closed {
		return fmt.Errorf("message broker is closed")
	}

	key := makeKey(topic, routingKey)
	channel, exists := b.topics[key]
	if !exists {
		// Create new channel for this topic/routingKey if it doesn't exist
		b.mu.RUnlock()
		b.mu.Lock()
		channel = make(chan domain.Message, 100) // Buffered channel with 100 capacity
		b.topics[key] = channel
		b.mu.Unlock()
		b.mu.RLock()
	}

	msg := domain.Message{
		Topic:      topic,
		RoutingKey: routingKey,
		Payload:    message,
		Timestamp:  time.Now(),
	}

	select {
	case channel <- msg:
		log.WithCtx(ctx).Debug("📤 Message published to topic",
			zap.String("topic", topic),
			zap.String("routingKey", routingKey),
			zap.Int("payload_size", len(message)))
		return nil
	case <-ctx.Done():
		return ctx.Err()
	default:
		return fmt.Errorf("topic channel is full: %s:%s", topic, routingKey)
	}
}

// Subscribe listens for messages on a specific topic and routing key
func (b *ChannelMessageBroker) Subscribe(ctx context.Context, topic string, routingKey string) (<-chan domain.Message, error) {
	b.mu.Lock()
	defer b.mu.Unlock()

	if b.closed {
		return nil, fmt.Errorf("message broker is closed")
	}

	key := makeKey(topic, routingKey)
	channel, exists := b.topics[key]
	if !exists {
		// Create new channel for this topic/routingKey if it doesn't exist
		channel = make(chan domain.Message, 100) // Buffered channel with 100 capacity
		b.topics[key] = channel
	}

	log.WithCtx(ctx).Info("📡 Subscribed to topic", zap.String("topic", topic), zap.String("routingKey", routingKey))
	return channel, nil
}

// Close closes the message broker and all topic channels
func (b *ChannelMessageBroker) Close() error {
	b.mu.Lock()
	defer b.mu.Unlock()

	if b.closed {
		return nil
	}

	b.closed = true

	// Close all topic channels
	for key, channel := range b.topics {
		close(channel)
		log.WithCtx(context.Background()).Debug("🔒 Closed topic channel", zap.String("key", key))
	}

	// Clear the topics map
	b.topics = make(map[string]chan domain.Message)

	log.WithCtx(context.Background()).Info("🔒 Message broker closed")
	return nil
}

// GetTopicCount returns the number of active topics (useful for monitoring)
func (b *ChannelMessageBroker) GetTopicCount() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return len(b.topics)
}

// IsClosed returns whether the broker is closed
func (b *ChannelMessageBroker) IsClosed() bool {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return b.closed
}
