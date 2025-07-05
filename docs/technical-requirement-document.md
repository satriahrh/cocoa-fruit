# Technical Requirements Document

## Project Title: **Talking Doll with AI-Driven Real-Time Conversation**

---

### **1. Overview**

The project aims to develop an IoT-based talking doll capable of real-time, AI-driven conversations. The doll will capture user speech, process it through a cloud-based server for speech-to-text (STT), text generation, and text-to-speech (TTS), and play back the AI-generated response. The system leverages WebSocket for persistent, low-latency communication and a message broker for scalable architecture between the doll and the server, enabling seamless conversational sessions.

---

### **2. Functional Requirements**

#### **Doll (IoT Device)**
1. **Input:**
   - Capture user voice using a microphone (e.g., INMP441).
   - Send audio data to the server via HTTP streaming or WebSocket in real time.

2. **Output:**
   - Play the server-generated audio response through a speaker (e.g., MAX98357A amplifier and 8Ω speaker).

3. **Communication:**
   - Establish and maintain a long-lived WebSocket connection with the server.
   - Support HTTP streaming for audio upload.
   - Auto-reconnect in case of connection loss.

4. **Power Management:**
   - Use a 3.7V LiPo battery with onboard charging and power monitoring through the ESP32.

#### **Server**
1. **Speech-to-Text (STT):**
   - Convert the doll's streamed audio into text using Google Speech-to-Text API.
   - Support real-time streaming transcription for low latency.

2. **Text Generation:**
   - Generate AI-driven responses based on the input text using Google Gemini.

3. **Text-to-Speech (TTS):**
   - Convert the generated text into speech using Google TTS.

4. **Message Broker Integration:**
   - Publish transcription results to message broker for decoupled communication.
   - Subscribe to transcription messages for real-time broadcasting.
   - Support routing keys for session-specific message delivery.

5. **Session Management:**
   - Maintain a stateful session for each connected doll to provide context-aware responses.
   - Use session IDs for message routing and tracking.

6. **Communication:**
   - Handle HTTP endpoints for audio streaming with JWT authentication.
   - Handle WebSocket connections for bidirectional communication with the doll.
   - Stream audio responses back to the doll in real time.

7. **Scalability:**
   - Support multiple concurrent connections from different dolls.
   - Message broker architecture enables horizontal scaling.

---

### **3. Non-Functional Requirements**

#### **Performance:**
- End-to-end response time (user input to doll output) must be under **1 second**.
- Audio streaming latency should not exceed **100ms**.
- Message broker throughput: 10,000+ messages/second (channel-based).

#### **Reliability:**
- Ensure **99.9% uptime** for the server.
- Implement robust error handling and auto-reconnection logic.
- Message broker provides fault tolerance and message persistence (future RabbitMQ).

#### **Scalability:**
- Support up to **100 concurrent connections** in the initial phase.
- Message broker architecture enables easy horizontal scaling.
- Channel-based implementation for single pod, external broker for multi-pod.

#### **Security:**
- Use **JWT authentication** for HTTP and WebSocket endpoints.
- **WebSocket Secure (wss://)** for encrypted communication.
- Rate limiting and request validation.

#### **Compatibility:**
- ESP32-based IoT device with support for microphone and speaker peripherals.
- Server must run on cloud infrastructure (AWS, GCP, or Azure).
- Cross-platform C client for testing and development.

---

### **4. Technical Architecture**

#### **Current Architecture (Single Pod)**
```
HTTP Audio Stream → Message Broker → WebSocket Broadcast
     ↓                    ↓              ↓
Transcription    Channel-based    Real-time Clients
Processing       (Go Channels)    (ESP32, Web, etc.)
```

#### **Future Architecture (Multi-pod)**
```
Transcription    RabbitMQ/Redis   WebSocket
Pod 1 ──────────▶   Message     ──▶ Pod 1
                   Broker
Transcription                    WebSocket
Pod 2 ──────────▶              ──▶ Pod 2
```

#### **IoT Device (Doll)**
- **Microcontroller:** ESP32 (LOLIN32 Lite)
- **Microphone:** INMP441 (I2S Digital Microphone)
- **Amplifier:** MAX98357A (I2S Digital Audio Amplifier)
- **Speaker:** 8Ω, 2–3W
- **Battery:** 3.7V LiPo with PH2.0 connector
- **Communication Protocol:** HTTP streaming + WebSocket over Wi-Fi

#### **Server**
- **Backend Framework:** Golang
- **STT Service:** Google Speech-to-Text API
- **Text Generation:** Google Gemini
- **TTS Service:** Google TTS
- **Message Broker:** Channel-based (current), RabbitMQ/Redis (future)
- **Authentication:** JWT-based
- **Hosting:** Cloud provider (AWS, GCP, or Azure)

---

### **5. Data Flow**

1. **User Input:**
   - User speaks into the doll.
   - Doll streams audio data to the server via HTTP POST `/api/v1/audio/stream`.

2. **Server Processing:**
   - HTTP handler processes audio and performs STT conversion.
   - Transcription result is published to message broker with session ID as routing key.
   - WebSocket server subscribes to message broker and routes to specific device only.

3. **Output:**
   - WebSocket server receives transcription and routes to specific doll device.
   - Doll receives real-time transcription updates via WebSocket (one connection per device).

---

### **6. API Specification**

#### **HTTP Endpoints**
- **Authentication:** `POST /api/v1/auth/token`
- **Health Check:** `GET /api/v1/health`
- **Audio Streaming:** `POST /api/v1/audio/stream`

#### **WebSocket Communication**
- **Endpoint:** `ws://example.com/ws`
- **Authentication:** JWT token in header or query parameter
- **Connection Limit:** One WebSocket connection per device ID
- **Duplicate Handling:** HTTP 409 error for duplicate connections

#### **Message Broker Topics**
- **Topic:** `transcription.results`
- **Routing Key:** Session ID (for audio stream tracking)
- **Device Routing:** WebSocket messages routed by deviceID

#### **Message Formats**
- **Transcription Message:**
  ```json
  {
    "session_id": "a1b2c3d4e5f6g7h8",
    "user_id": 99,
    "device_id": "12312",
    "text": "Hello, how are you today?",
    "timestamp": "2024-01-15T10:30:00Z",
    "success": true
  }
  ```

- **WebSocket Broadcast:**
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

---

### **7. Development Milestones**

1. **Week 1–2: Core Architecture**
   - Implement message broker with channel-based implementation.
   - Set up HTTP and WebSocket endpoints with JWT authentication.

2. **Week 3–4: Audio Processing**
   - Integrate Google Speech-to-Text for streaming transcription.
   - Implement message broker publishing and subscription.

3. **Week 5: AI Integration**
   - Integrate Google Gemini for text generation.
   - Integrate Google TTS for audio synthesis.

4. **Week 6: End-to-End Testing**
   - Test the system for real-time interactions.
   - Optimize latency and handle edge cases.

5. **Week 7: Deployment & Scaling**
   - Deploy the server on a cloud provider.
   - Prepare for RabbitMQ migration for multi-pod scaling.

---

### **8. Risks and Mitigation**

| **Risk**                        | **Mitigation**                                                                 |
|----------------------------------|-------------------------------------------------------------------------------|
| High latency in audio processing | Optimize server processing; use streaming transcription for real-time results.|
| Connection drops mid-session     | Implement auto-reconnect and session restoration logic.                       |
| Limited battery life             | Optimize ESP32 power consumption; use sleep modes when idle.                  |
| Scalability issues               | Message broker architecture enables horizontal scaling and load balancing.    |
| Message broker bottlenecks       | Migrate from channel-based to RabbitMQ/Redis for production scaling.         |

---

### **9. Tools and Resources**

- **Hardware:**
  - ESP32 development board (LOLIN32 Lite)
  - INMP441 microphone, MAX98357A amplifier, 8Ω speaker
  - 3.7V LiPo battery

- **Software:**
  - Golang backend with Echo framework
  - WebSocket libraries (Gorilla WebSocket)
  - Message broker (Go channels, future RabbitMQ)
  - APIs: Google Speech-to-Text, Google Gemini, Google TTS

- **Cloud Resources:**
  - AWS EC2/Google Cloud Compute for hosting the server
  - Future: RabbitMQ/Redis for message broker

- **Testing:**
  - C-based WebSocket client (doll-replica-c)
  - Go-based test client (doll-replica)

---

### **10. Glossary**

- **STT:** Speech-to-Text
- **TTS:** Text-to-Speech
- **IoT:** Internet of Things
- **WebSocket:** A protocol for full-duplex communication over a single TCP connection.
- **Message Broker:** A system that enables communication between different components by routing messages.
- **Routing Key:** A string used to route messages to specific consumers in a message broker.
- **JWT:** JSON Web Token for secure authentication.