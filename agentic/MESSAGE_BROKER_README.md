# Message Broker Architecture

This document describes the message broker implementation and architecture for the agentic system, designed to enable scalable communication between components.

## Overview

The message broker provides a decoupled communication layer between:
- **HTTP Handler**: Publishes transcription results
- **WebSocket Server**: Subscribes to transcription messages
- **Future Components**: Additional services can easily join the communication

## Architecture

### Current Implementation (Single Pod)

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   HTTP Handler  │───▶│  Message Broker  │───▶│ WebSocket Server│
│                 │    │  (Go Channels)   │    │                 │
│ - Audio Stream  │    │                  │    │ - Client Hub    │
│ - Transcription │    │ - Topic:         │    │ - Broadcasting  │
│ - Publishing    │    │   transcription  │    │ - Real-time     │
└─────────────────┘    │   .results       │    └─────────────────┘
                       │ - Routing Keys   │
                       │ - Buffered       │
                       └──────────────────┘
```

### Future Implementation (Multi-pod)

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ Transcription   │───▶│   RabbitMQ/      │───▶│ WebSocket       │
│ Pod 1           │    │   Redis/Kafka    │    │ Pod 1           │
└─────────────────┘    │                  │    └─────────────────┘
                       │ - Exchange:      │
┌─────────────────┐    │   transcription  │    ┌─────────────────┐
│ Transcription   │───▶│ - Routing Keys:  │───▶│ WebSocket       │
│ Pod 2           │    │   session.*      │    │ Pod 2           │
└─────────────────┘    │ - Queues:        │    └─────────────────┘
                       │   per-session    │
                       └──────────────────┘
```

## Domain Interface

### MessageBroker Interface

```go
type MessageBroker interface {
    // Publish sends a message to a specific topic with routing key
    Publish(ctx context.Context, topic string, routingKey string, message []byte) error
    
    // Subscribe listens for messages on a specific topic and routing key
    Subscribe(ctx context.Context, topic string, routingKey string) (<-chan Message, error)
    
    // Close closes the message broker connection
    Close() error
}
```

### Message Structure

```go
type Message struct {
    Topic      string    // Message topic (e.g., "transcription.results")
    RoutingKey string    // Routing key (e.g., session ID)
    Payload    []byte    // Message payload (JSON)
    Timestamp  time.Time // Message timestamp
}
```

### Transcription Message

```go
type TranscriptionMessage struct {
    SessionID string    `json:"session_id"`
    UserID    int       `json:"user_id"`
    DeviceID  string    `json:"device_id"`
    Text      string    `json:"text"`
    Timestamp time.Time `json:"timestamp"`
    Success   bool      `json:"success"`
    Error     string    `json:"error,omitempty"`
}
```

## Implementation

### Channel-based Message Broker

**File**: `adapters/message_broker/channel.go`

Uses Go channels for in-memory message passing:

```go
type ChannelMessageBroker struct {
    topics map[string]chan domain.Message
    mu     sync.RWMutex
    closed bool
}
```

**Features**:
- In-memory communication
- Thread-safe operations
- Buffered channels (100 messages)
- Automatic topic creation
- Graceful shutdown

**Usage**:
```go
// Create broker
messageBroker := message_broker.NewChannelMessageBroker()
defer messageBroker.Close()

// Publish message
err := messageBroker.Publish(ctx, "transcription.results", sessionID, payload)

// Subscribe to messages
messageChan, err := messageBroker.Subscribe(ctx, "transcription.results", sessionID)
```

### Future: RabbitMQ Implementation

**File**: `adapters/message_broker/rabbitmq.go` (to be implemented)

```go
type RabbitMQBroker struct {
    conn    *amqp.Connection
    channel *amqp.Channel
    config  RabbitMQConfig
}

type RabbitMQConfig struct {
    URL         string
    Exchange    string
    QueuePrefix string
}
```

**Features**:
- Persistent message storage
- Multiple consumer support
- Message acknowledgment
- Dead letter queues
- Message routing with patterns

## Message Flow

### 1. HTTP Handler Publishing

```go
// In adapters/http/handler.go
func (h *AudioHandler) StreamAudio(c echo.Context) error {
    // ... audio processing ...
    
    // Create transcription message
    transcriptionMsg := domain.TranscriptionMessage{
        SessionID: sessionID,
        UserID:    userID,
        DeviceID:  deviceID,
        Text:      transcription,
        Success:   true,
        Timestamp: time.Now(),
    }
    
    // Convert to JSON
    payload, _ := json.Marshal(transcriptionMsg)
    
    // Publish to message broker with session ID as routing key
    err = h.messageBroker.Publish(ctx, "transcription.results", sessionID, payload)
    if err != nil {
        log.Printf("Error publishing transcription result: %v", err)
    }
    
    return c.JSON(http.StatusOK, response)
}
```

### 2. WebSocket Server Subscribing

```go
// In adapters/websocket/server.go
func (s *Server) startTranscriptionListener() {
    ctx := context.Background()
    
    // Subscribe to all transcription messages
    messageChan, err := s.messageBroker.Subscribe(ctx, "transcription.results", "")
    if err != nil {
        log.WithCtx(ctx).Error("Failed to subscribe to transcription topic", zap.Error(err))
        return
    }
    
    for {
        select {
        case msg := <-messageChan:
            // Process transcription message
            var transcriptionMsg domain.TranscriptionMessage
            json.Unmarshal(msg.Payload, &transcriptionMsg)
            
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
            
            // Broadcast to all WebSocket clients
            jsonData, _ := json.Marshal(wsMessage)
            s.hub.Broadcast(jsonData)
            
        case <-ctx.Done():
            return
        }
    }
}
```

## Routing Keys

### Current Usage

- **Topic**: `transcription.results`
- **Routing Key**: Session ID (e.g., `a1b2c3d4e5f6g7h8`)
- **Purpose**: Enable session-specific message routing

### Future RabbitMQ Patterns

```go
// Session-specific routing
messageBroker.Publish(ctx, "transcription.results", "session.abc123", payload)

// Device-specific routing
messageBroker.Publish(ctx, "transcription.results", "device.esp32.001", payload)

// User-specific routing
messageBroker.Publish(ctx, "transcription.results", "user.99", payload)

// Wildcard subscriptions
messageBroker.Subscribe(ctx, "transcription.results", "session.*")  // All sessions
messageBroker.Subscribe(ctx, "transcription.results", "device.*")   // All devices
messageBroker.Subscribe(ctx, "transcription.results", "")           // All messages
```

## Configuration

### Channel Broker Settings

```go
const (
    // Buffer sizes
    ChannelBufferSize = 100
    
    // Topics
    TranscriptionTopic = "transcription.results"
    
    // Timeouts
    PublishTimeout = 5 * time.Second
    SubscribeTimeout = 30 * time.Second
)
```

### Future RabbitMQ Settings

```go
type RabbitMQConfig struct {
    URL:         "amqp://localhost:5672"
    Exchange:    "transcription"
    QueuePrefix: "transcription_"
    RoutingKey:  "session.*"
    
    // Connection settings
    Heartbeat:   30 * time.Second
    MaxRetries:  3
    RetryDelay:  5 * time.Second
}
```

## Error Handling

### Channel Broker Errors

```go
// Channel full error
if err := messageBroker.Publish(ctx, topic, routingKey, payload); err != nil {
    if strings.Contains(err.Error(), "channel is full") {
        // Handle buffer overflow
        log.Warn("Message broker buffer full, dropping message")
    }
}

// Context cancellation
select {
case messageChan <- msg:
    // Message sent successfully
case <-ctx.Done():
    return ctx.Err()
}
```

### Future RabbitMQ Errors

```go
// Connection errors
if err := rabbitmqBroker.Publish(ctx, topic, routingKey, payload); err != nil {
    if amqpError, ok := err.(*amqp.Error); ok {
        switch amqpError.Code {
        case amqp.ChannelError:
            // Handle channel errors
        case amqp.ConnectionError:
            // Handle connection errors
        }
    }
}
```

## Performance Considerations

### Channel Broker Performance

- **Memory Usage**: ~1KB per message in buffer
- **Latency**: <1ms for in-memory operations
- **Throughput**: 10,000+ messages/second
- **Concurrency**: Thread-safe with RWMutex

### Future RabbitMQ Performance

- **Memory Usage**: Depends on queue size and persistence
- **Latency**: 1-10ms for network operations
- **Throughput**: 50,000+ messages/second
- **Persistence**: Messages survive broker restarts

## Monitoring

### Channel Broker Metrics

```go
// Get topic count
topicCount := messageBroker.GetTopicCount()

// Check if broker is closed
isClosed := messageBroker.IsClosed()

// Monitor buffer usage
for topic, channel := range messageBroker.topics {
    bufferUsage := len(channel)
    log.Info("Topic buffer usage", 
        zap.String("topic", topic),
        zap.Int("usage", bufferUsage),
        zap.Int("capacity", cap(channel)))
}
```

### Future RabbitMQ Metrics

```go
// Queue metrics
queueInfo, err := channel.QueueInspect(queueName)
if err == nil {
    log.Info("Queue metrics",
        zap.String("queue", queueName),
        zap.Int("messages", queueInfo.Messages),
        zap.Int("consumers", queueInfo.Consumers))
}
```

## Migration Guide

### From Channel to RabbitMQ

1. **Implement RabbitMQ Adapter**:
   ```go
   // Create new adapter
   rabbitmqBroker := rabbitmq.NewRabbitMQBroker(config)
   ```

2. **Update Main Function**:
   ```go
   // Replace channel broker
   // messageBroker := message_broker.NewChannelMessageBroker()
   messageBroker := rabbitmq.NewRabbitMQBroker(rabbitmqConfig)
   ```

3. **Configure Exchange and Queues**:
   ```go
   // Set up exchange
   err := channel.ExchangeDeclare(
       "transcription", // name
       "topic",         // type
       true,           // durable
       false,          // auto-deleted
       false,          // internal
       false,          // no-wait
       nil,            // arguments
   )
   ```

4. **Update Routing Logic**:
   ```go
   // Use session-specific routing keys
   routingKey := fmt.Sprintf("session.%s", sessionID)
   messageBroker.Publish(ctx, "transcription.results", routingKey, payload)
   ```

### Testing Migration

```go
// Test both implementations
func TestMessageBrokerMigration() {
    // Test channel broker
    channelBroker := message_broker.NewChannelMessageBroker()
    testMessageBroker(channelBroker, "Channel Broker")
    
    // Test RabbitMQ broker
    rabbitmqBroker := rabbitmq.NewRabbitMQBroker(config)
    testMessageBroker(rabbitmqBroker, "RabbitMQ Broker")
}

func testMessageBroker(broker domain.MessageBroker, name string) {
    ctx := context.Background()
    
    // Subscribe
    messageChan, _ := broker.Subscribe(ctx, "test.topic", "test.key")
    
    // Publish
    payload := []byte(`{"test": "message"}`)
    broker.Publish(ctx, "test.topic", "test.key", payload)
    
    // Receive
    msg := <-messageChan
    log.Info("Message received", 
        zap.String("broker", name),
        zap.String("payload", string(msg.Payload)))
}
```

## Security Considerations

### Channel Broker Security

- **In-memory**: No network exposure
- **Process isolation**: Messages only accessible within the process
- **No persistence**: Messages lost on restart

### Future RabbitMQ Security

- **Authentication**: Username/password or certificates
- **Authorization**: Queue and exchange permissions
- **Encryption**: TLS for network communication
- **Access Control**: VHost isolation

## Conclusion

The message broker architecture provides a flexible, scalable foundation for component communication. The current channel-based implementation offers simplicity and performance for single-pod deployments, while the interface design enables easy migration to external brokers for multi-pod scalability. 