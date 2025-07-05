# Streaming Transcription with Message Broker

This document describes the implementation of real-time streaming audio transcription using Google Speech-to-Text API with a message broker architecture for scalable communication.

## Overview

The streaming transcription feature allows for real-time processing of audio chunks, providing faster and more responsive transcription compared to batch processing. The system now uses a message broker to decouple transcription processing from result delivery, enabling:

- **Scalable Architecture**: Support for multiple pods/instances
- **Real-time Communication**: Decoupled HTTP and WebSocket components
- **Session-based Routing**: Messages can be routed to specific sessions
- **Future-ready**: Easy migration to external message brokers (RabbitMQ, Redis, etc.)

## Architecture

### Message Flow

```
HTTP Audio Stream → Message Broker → WebSocket Broadcast
     ↓                    ↓              ↓
Transcription    Channel-based    Real-time Clients
Processing       (Single Pod)     (ESP32, Web, etc.)
```

### Core Components

1. **HTTP Handler** (`adapters/http/handler.go`) - Receives audio streams and publishes transcription results
2. **Message Broker** (`adapters/message_broker/channel.go`) - In-memory message routing using Go channels
3. **WebSocket Server** (`adapters/websocket/server.go`) - Subscribes to messages and broadcasts to clients
4. **Speech Service** (`adapters/speech/google.go`) - Google Speech-to-Text integration

### Key Features

- **Real-time Processing**: Audio chunks are processed as they arrive
- **Message Broker Integration**: Decoupled architecture with routing key support
- **Session-based Routing**: Messages can be targeted to specific sessions
- **WebSocket Broadcasting**: Real-time delivery to connected clients
- **Error Handling**: Robust error handling with proper cleanup
- **Context Support**: Full context cancellation support for timeouts

## Message Broker Implementation

### Current: Channel-based (Single Pod)

The system currently uses Go channels for in-memory message passing:

```go
// Domain interface
type MessageBroker interface {
    Publish(ctx context.Context, topic string, routingKey string, message []byte) error
    Subscribe(ctx context.Context, topic string, routingKey string) (<-chan Message, error)
    Close() error
}

// Channel implementation
messageBroker := message_broker.NewChannelMessageBroker()
```

### Future: External Broker (Multi-pod)

Easy migration to external brokers like RabbitMQ:

```go
// Same interface, different implementation
messageBroker := rabbitmq.NewRabbitMQBroker(config)
```

## API Usage

### HTTP Audio Streaming

The `POST /api/v1/audio/stream` endpoint processes audio and publishes results:

```bash
POST /api/v1/audio/stream
Authorization: Bearer <jwt-token>
Content-Type: audio/wav
Transfer-Encoding: chunked

[Audio chunks...]
```

**Response:**
```json
{
  "success": true,
  "message": "Audio processed successfully",
  "session_id": "a1b2c3d4e5f6g7h8",
  "text": "Hello, how are you today?"
}
```

### Message Broker Publishing

Transcription results are automatically published to the message broker:

```go
// HTTP handler publishes to broker
transcriptionMsg := domain.TranscriptionMessage{
    SessionID: sessionID,
    UserID:    userID,
    DeviceID:  deviceID,
    Text:      transcription,
    Success:   true,
    Timestamp: time.Now(),
}

payload, _ := json.Marshal(transcriptionMsg)
messageBroker.Publish(ctx, "transcription.results", sessionID, payload)
```

### WebSocket Subscription

The WebSocket server subscribes to transcription messages and broadcasts to clients:

```go
// WebSocket server subscribes to broker
messageChan, _ := messageBroker.Subscribe(ctx, "transcription.results", "")
for msg := range messageChan {
    // Broadcast to all WebSocket clients
    hub.Broadcast(msg.Payload)
}
```

### WebSocket Message Format

Clients receive real-time transcription updates:

```json
{
  "type": "transcription",
  "session_id": "a1b2c3d4e5f6g7h8",
  "user_id": 99,
  "device_id": "12312",
  "text": "Hello, how are you today?",
  "timestamp": "2024-01-15T10:30:00Z",
  "success": true
}
```

## Configuration

### Audio Format Requirements

- **Encoding**: LINEAR16 (PCM)
- **Sample Rate**: 16000 Hz
- **Channels**: 1 (Mono)
- **Language**: Indonesian (id-ID)

### Performance Settings

- **Chunk Size**: 4KB (4096 bytes) - ~128ms of audio at 16kHz
- **Buffer Sizes**: 
  - Audio chunks: 100 chunks
  - Message broker: 100 messages per topic
- **Timeout**: 30 seconds per session
- **Max Duration**: 60 seconds per stream

### Message Broker Settings

- **Topic**: `transcription.results`
- **Routing Key**: Session ID (for session-specific routing)
- **Buffer Size**: 100 messages per channel

## Error Handling

The implementation includes comprehensive error handling:

1. **Connection Errors**: Automatic retry and cleanup
2. **Timeout Handling**: Context cancellation support
3. **Buffer Overflow**: Graceful handling of full channels
4. **API Errors**: Proper error propagation and logging
5. **Message Broker Errors**: Graceful degradation

## Testing

### Run the Test

```bash
cd agentic/test
go run test_streaming.go
```

### Expected Output

```
🧪 Testing streaming transcription...
📤 Created 8 audio chunks for testing
🎤 Starting streaming transcription session
✅ Streaming configuration sent successfully
📤 Sent audio chunk to streaming
📥 Received final transcript
✅ Streaming transcription completed in 2.5s
📝 No transcript detected (expected for test audio)
🎉 Streaming transcription test completed!
```

## Migration Path

### Current State (Single Pod)
- Channel-based message broker
- In-memory communication
- Simple deployment

### Future State (Multi-pod)
1. **Replace Channel Broker**: Implement RabbitMQ/Redis adapter
2. **Session Routing**: Use session IDs as routing keys
3. **Load Balancing**: Multiple transcription pods
4. **Scalability**: Horizontal scaling support

### Example RabbitMQ Migration

```go
// Replace channel broker with RabbitMQ
messageBroker := rabbitmq.NewRabbitMQBroker(rabbitmq.Config{
    URL: "amqp://localhost:5672",
    Exchange: "transcription",
    QueuePrefix: "transcription_",
})

// Same interface, different implementation
messageBroker.Publish(ctx, "transcription.results", sessionID, payload)
```

## Security Considerations

1. **JWT Authentication**: All endpoints require valid JWT tokens
2. **Rate Limiting**: Built-in rate limiting to prevent abuse
3. **Audio Validation**: Input validation for audio format and size
4. **Session Isolation**: Each streaming session is isolated
5. **Secure Communication**: Use HTTPS/WSS in production

## Conclusion

The streaming transcription implementation with message broker provides a robust, scalable solution for real-time audio processing. The decoupled architecture enables easy migration to external message brokers while maintaining the same interface and functionality. 