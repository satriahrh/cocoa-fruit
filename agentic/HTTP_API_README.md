# Secure HTTP Audio Streaming API with Message Broker

This document describes the secure HTTP endpoints for audio streaming and transcription with message broker integration, designed for ESP32 and other IoT devices.

## Overview

The API provides secure endpoints for:
- **JWT-based authentication** for enhanced security (both HTTP and WebSocket)
- **Audio streaming** for real-time chunked processing
- **Message broker integration** for scalable communication
- **Rate limiting** to prevent abuse
- **CORS support** for web clients

## Architecture

### Message Flow

```
HTTP Audio Stream → Message Broker → WebSocket Broadcast
     ↓                    ↓              ↓
Transcription    Channel-based    Real-time Clients
Processing       (Single Pod)     (ESP32, Web, etc.)
```

### Components

- **HTTP Handler**: Receives audio streams and publishes transcription results to message broker
- **Message Broker**: Routes messages between components (currently channel-based)
- **WebSocket Server**: Subscribes to message broker and broadcasts to clients
- **Speech Service**: Google Speech-to-Text integration

## Base URL

```
http://localhost:8080
```

## Authentication

### Getting a JWT Token

**Endpoint:** `POST /api/v1/auth/token`

**Headers:**
```
X-API-Key: John
X-API-Secret: Doe
Content-Type: application/json
```

**Response:**
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "type": "Bearer"
}
```

### Using JWT Token

Include the JWT token in the Authorization header for all protected endpoints (both HTTP and WebSocket):

```
Authorization: Bearer <your-jwt-token>
```

For WebSocket connections, you can also pass the token as a query parameter:
```
ws://localhost:8080/ws?token=<your-jwt-token>
```

## Endpoints

### 1. Health Check

**Endpoint:** `GET /api/v1/health`

**Description:** Check server health and status

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2024-01-15T10:30:00Z",
  "service": "audio-streaming"
}
```

### 2. Audio Streaming

**Endpoint:** `POST /api/v1/audio/stream`

**Description:** Stream audio data in chunks for real-time processing. Results are published to message broker.

**Headers:**
```
Authorization: Bearer <jwt-token>
Content-Type: audio/wav
Transfer-Encoding: chunked
```

**Body:** Chunked audio data

**Response:**
```json
{
  "success": true,
  "message": "Audio processed successfully",
  "session_id": "a1b2c3d4e5f6g7h8",
  "text": "Hello, how are you today?"
}
```

**Message Broker Publishing:**
The transcription result is automatically published to the message broker with:
- **Topic**: `transcription.results`
- **Routing Key**: Session ID
- **Payload**: JSON transcription message

### 3. WebSocket Connection

**Endpoint:** `GET /ws`

**Description:** WebSocket connection for real-time communication. One connection per device only.

**Authentication:** JWT token required (same as HTTP endpoints)

**Connection Limits:** 
- One WebSocket connection per device ID
- Duplicate connections will be rejected with HTTP 409

**Headers:**
```
Authorization: Bearer <jwt-token>
```

**Or Query Parameter:**
```
ws://localhost:8080/ws?token=<jwt-token>
```

**Error Response (409 Conflict):**
```json
{
  "message": "Device already connected"
}
```

**Message Format:**
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

## Message Broker Integration

### Current Implementation (Channel-based)

The system uses Go channels for in-memory message passing:

```go
// Domain interface
type MessageBroker interface {
    Publish(ctx context.Context, topic string, routingKey string, message []byte) error
    Subscribe(ctx context.Context, topic string, routingKey string) (<-chan Message, error)
    Close() error
}
```

### Message Flow

1. **HTTP Handler** processes audio and publishes transcription:
   ```go
   messageBroker.Publish(ctx, "transcription.results", sessionID, payload)
   ```

2. **WebSocket Server** subscribes and routes by device:
   ```go
   messageChan, _ := messageBroker.Subscribe(ctx, "transcription.results", "")
   // Route transcription to specific device only
   hub.SendToDevice(transcriptionMsg.DeviceID, jsonData)
   ```

3. **Device-based Routing**: Messages are sent only to the specific device that initiated the audio stream

### Future Migration

Easy migration to external brokers (RabbitMQ, Redis, etc.) by implementing the same interface.

## Security Features

### Rate Limiting
- **Global:** 20 requests per minute
- **Concurrent:** Maximum 10 simultaneous requests
- **File size:** Maximum 10MB per request
- **Audio duration:** Maximum 60 seconds

### CORS Configuration
```javascript
{
  "AllowOrigins": ["*"],
  "AllowMethods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
  "AllowHeaders": [
    "Origin", "Content-Type", "Accept", "Authorization",
    "X-API-Key", "X-API-Secret", "Content-Length"
  ],
  "AllowCredentials": true,
  "MaxAge": 86400
}
```

### JWT Token Details
- **Algorithm:** HS256
- **Expiry:** 24 hours
- **Issuer:** cocoa-fruit-agentic
- **Subject:** audio-streaming

## Error Responses

### 401 Unauthorized
```json
{
  "message": "Missing JWT token"
}
```

### 400 Bad Request
```json
{
  "message": "Invalid content type. Expected audio/* or application/octet-stream"
}
```

### 413 Payload Too Large
```json
{
  "message": "Request entity too large"
}
```

### 409 Conflict
```json
{
  "message": "Device already connected"
}
```

### 429 Too Many Requests
```json
{
  "message": "Too many concurrent requests"
}
```

### 500 Internal Server Error
```json
{
  "message": "Failed to transcribe audio"
}
```

## ESP32 Integration Example

### C++ Example

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>

const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";
const char* serverUrl = "http://localhost:8080";

String jwtToken = "";
WebSocketsClient webSocket;

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  // Get JWT token
  getJWTToken();
  
  // Connect to WebSocket
  webSocket.begin(serverUrl, 8080, "/ws");
  webSocket.setAuthorization("Bearer " + jwtToken);
  webSocket.onEvent(webSocketEvent);
}

void getJWTToken() {
  HTTPClient http;
  http.begin(serverUrl + "/api/v1/auth/token");
  http.addHeader("X-API-Key", "John");
  http.addHeader("X-API-Secret", "Doe");
  
  int httpCode = http.POST("");
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    // Parse JSON to extract token
    jwtToken = extractToken(payload);
  }
  http.end();
}

void streamAudio() {
  HTTPClient http;
  http.begin(serverUrl + "/api/v1/audio/stream");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  http.addHeader("Content-Type", "audio/wav");
  
  // Stream audio data
  http.beginRequest();
  // Send audio chunks...
  http.endRequest();
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Transcription: " + response);
  }
  http.end();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      break;
    case WStype_TEXT:
      // Handle transcription message
      handleTranscriptionMessage(payload, length);
      break;
  }
}

void handleTranscriptionMessage(uint8_t * payload, size_t length) {
  String message = String((char*)payload);
  // Parse JSON and handle transcription
  Serial.println("Received transcription: " + message);
}
```

### JavaScript Example

```javascript
// Get JWT token
async function getJWTToken() {
  const response = await fetch('http://localhost:8080/api/v1/auth/token', {
    method: 'POST',
    headers: {
      'X-API-Key': 'John',
      'X-API-Secret': 'Doe'
    }
  });
  const data = await response.json();
  return data.token;
}

// Connect to WebSocket
async function connectWebSocket() {
  const token = await getJWTToken();
  const ws = new WebSocket(`ws://localhost:8080/ws?token=${token}`);
  
  ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    if (data.type === 'transcription') {
      console.log('Transcription:', data.text);
    }
  };
  
  return ws;
}

// Stream audio
async function streamAudio(audioBlob) {
  const token = await getJWTToken();
  const response = await fetch('http://localhost:8080/api/v1/audio/stream', {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${token}`,
      'Content-Type': 'audio/wav'
    },
    body: audioBlob
  });
  
  const result = await response.json();
  console.log('Transcription result:', result);
}
```

## Performance Considerations

### Audio Format
- **Encoding**: LINEAR16 (PCM)
- **Sample Rate**: 16000 Hz
- **Channels**: 1 (Mono)
- **Language**: Indonesian (id-ID)

### Streaming Settings
- **Chunk Size**: 4KB (4096 bytes)
- **Buffer Size**: 100 chunks
- **Timeout**: 30 seconds per session
- **Max Duration**: 60 seconds per stream

### Message Broker
- **Topic**: `transcription.results`
- **Routing Key**: Session ID
- **Buffer Size**: 100 messages per channel

## Troubleshooting

### Common Issues

1. **No Transcription Results**
   - Check audio format (must be LINEAR16, 16kHz, mono)
   - Verify audio contains speech (not just silence)
   - Check Google Speech-to-Text API credentials

2. **WebSocket Connection Issues**
   - Verify JWT token is valid and not expired
   - Check CORS configuration for web clients
   - Ensure WebSocket server is running

3. **Message Broker Issues**
   - Check if message broker is properly initialized
   - Verify topic and routing key configuration
   - Monitor message broker buffer capacity

### Debug Logging

Enable debug logging to see detailed information:

```bash
# Set log level to debug
export LOG_LEVEL=debug
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

## Conclusion

The HTTP API with message broker integration provides a robust, scalable solution for real-time audio processing. The decoupled architecture enables easy migration to external message brokers while maintaining the same interface and functionality. 