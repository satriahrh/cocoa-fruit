# WebSocket Routing and Connection Management

This document describes the WebSocket routing implementation and connection management for the cocoa-fruit agentic system.

## Overview

The WebSocket system implements device-based routing to ensure that transcription messages are delivered only to the specific device that initiated the audio stream. This prevents message spam and ensures efficient communication.

## Connection Management

### Single Connection per Device

- **Rule**: Only one WebSocket connection allowed per device ID
- **Enforcement**: Automatic rejection of duplicate connections
- **Error Code**: HTTP 409 Conflict for duplicate connections

### Connection Lifecycle

```
1. WebSocket Connection Request
   ↓
2. JWT Authentication
   ↓
3. Device Connection Check
   ↓
4a. If device already connected: HTTP 409 + Close
4b. If device not connected: Accept + Register
   ↓
5. Message Routing (device-based)
   ↓
6. Auto-cleanup on disconnect
```

## Implementation Details

### Hub Methods

```go
// Check if device is already connected
func (h *Hub) IsDeviceConnected(deviceID string) bool

// Send message to specific device
func (h *Hub) SendToDevice(deviceID string, message []byte) error

// Get client by device ID
func (h *Hub) GetClientByDevice(deviceID string) *Client

// Broadcast to all clients (fallback)
func (h *Hub) Broadcast(message []byte)
```

### Connection Handler

```go
func (s *Server) Handler(c echo.Context) error {
    // ... JWT validation ...
    
    // Check for duplicate connection
    if s.hub.IsDeviceConnected(deviceID) {
        conn.Close()
        return echo.NewHTTPError(409, "Device already connected")
    }
    
    // Create and register client
    client := NewClient(conn, userID, deviceID, deviceVersion, s.svc, s.googleSpeech)
    s.hub.Register(client)
    
    // ... handle connection ...
}
```

## Message Routing

### Transcription Message Flow

```
1. HTTP Audio Stream → Message Broker
   ↓
2. WebSocket Server subscribes to broker
   ↓
3. Transcription message received
   ↓
4. Extract deviceID from message
   ↓
5. Route to specific device
   ↓
6. If device not found: ignore message
```

### Routing Implementation

```go
func (s *Server) startTranscriptionListener() {
    for msg := range messageChan {
        // Try to send to specific device
        err := s.hub.SendToDevice(transcriptionMsg.DeviceID, jsonData)
        if err != nil {
            // Device not connected, ignore message
            log.Warn("Device not connected, ignoring transcription")
        } else {
            log.Info("Sent transcription to specific device")
        }
    }
}
```

## Error Handling

### Connection Errors

- **409 Conflict**: Device already connected
- **401 Unauthorized**: Invalid JWT token
- **Connection Close**: Automatic cleanup on disconnect

### Message Routing Errors

- **Device Not Found**: Message ignored (no broadcast)
- **Send Failure**: Logged but not retried
- **Connection Lost**: Automatic unregister

## Benefits

### 1. **No Message Spam**
- Messages sent only to intended device
- No broadcast to unrelated clients
- Efficient resource usage

### 2. **Connection Integrity**
- One device = one connection
- Prevents connection conflicts
- Clean connection management

### 3. **Scalable Architecture**
- Device-based routing scales well
- Easy to add device-specific features
- Future-ready for multi-pod deployment

### 4. **Better Monitoring**
- Clear connection tracking
- Device-specific logging
- Easy debugging and troubleshooting

## Configuration

### WebSocket Settings

```go
const (
    writeWait      = 10 * time.Second
    pongWait       = 60 * time.Second
    pingPeriod     = 30 * time.Second
    maxMessageSize = 512 * 1024
)
```

### Connection Limits

- **Per Device**: 1 WebSocket connection
- **Global**: No hard limit (scales with server resources)
- **Timeout**: 30 seconds for ping/pong

## Testing

### Connection Test

```bash
# First connection (should succeed)
websocat "ws://localhost:8080/ws?token=<jwt>"

# Second connection from same device (should fail with 409)
websocat "ws://localhost:8080/ws?token=<jwt>"
```

### Message Routing Test

1. Connect device A via WebSocket
2. Send audio stream from device A
3. Verify transcription received only by device A
4. Connect device B via WebSocket
5. Send audio stream from device B
6. Verify transcription received only by device B

## Future Enhancements

### 1. **Connection Pooling**
- Support multiple connections per device (if needed)
- Load balancing across connections
- Connection health monitoring

### 2. **Message Persistence**
- Store messages for offline devices
- Deliver when device reconnects
- Message expiration and cleanup

### 3. **Advanced Routing**
- User-based routing (multiple devices per user)
- Group-based routing (broadcast to device groups)
- Priority-based routing (urgent messages)

## Conclusion

The device-based WebSocket routing provides a clean, efficient, and scalable solution for real-time communication. The single-connection-per-device rule ensures connection integrity while the targeted message delivery prevents unnecessary network traffic and improves system performance. 