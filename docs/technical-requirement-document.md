# Technical Requirements Document

## Project Title: **Talking Doll with AI-Driven Real-Time Conversation**

---

### **1. Overview**

The project aims to develop an IoT-based talking doll capable of real-time, AI-driven conversations. The doll will capture user speech, process it through a cloud-based server for speech-to-text (STT), text generation, and text-to-speech (TTS), and play back the AI-generated response. The system will leverage WebSocket for persistent, low-latency communication between the doll and the server, enabling seamless conversational sessions.

---

### **2. Functional Requirements**

#### **Doll (IoT Device)**
1. **Input:**
   - Capture user voice using a microphone (e.g., INMP441).
   - Send audio data to the server via WebSocket in real time.

2. **Output:**
   - Play the server-generated audio response through a speaker (e.g., MAX98357A amplifier and 8Ω speaker).

3. **Communication:**
   - Establish and maintain a long-lived WebSocket connection with the server.
   - Auto-reconnect in case of connection loss.

4. **Power Management:**
   - Use a 3.7V LiPo battery with onboard charging and power monitoring through the ESP32.

#### **Server**
1. **Speech-to-Text (STT):**
   - Convert the doll's streamed audio into text using a STT API (e.g., Whisper API or Google Speech-to-Text).

2. **Text Generation:**
   - Generate AI-driven responses based on the input text using a GPT model (e.g., OpenAI GPT-4 or Hugging Face Transformers).

3. **Text-to-Speech (TTS):**
   - Convert the generated text into speech using a TTS API (e.g., Google TTS or Amazon Polly).

4. **Session Management:**
   - Maintain a stateful session for each connected doll to provide context-aware responses.

5. **Communication:**
   - Handle WebSocket connections for bidirectional communication with the doll.
   - Stream audio responses back to the doll in real time.

6. **Scalability:**
   - Support multiple concurrent WebSocket connections from different dolls.

---

### **3. Non-Functional Requirements**

#### **Performance:**
- End-to-end response time (user input to doll output) must be under **1 second**.
- Audio streaming latency should not exceed **100ms**.

#### **Reliability:**
- Ensure **99.9% uptime** for the server.
- Implement robust error handling and auto-reconnection logic.

#### **Scalability:**
- Support up to **100 concurrent connections** in the initial phase, with room for scaling further.

#### **Security:**
- Use **WebSocket Secure (wss://)** for encrypted communication.
- Authenticate IoT devices during the WebSocket handshake.

#### **Compatibility:**
- ESP32-based IoT device with support for microphone and speaker peripherals.
- Server must run on cloud infrastructure (AWS, GCP, or Azure).

---

### **4. Technical Architecture**

#### **IoT Device (Doll)**
- **Microcontroller:** ESP32 (LOLIN32 Lite)
- **Microphone:** INMP441 (I2S Digital Microphone)
- **Amplifier:** MAX98357A (I2S Digital Audio Amplifier)
- **Speaker:** 8Ω, 2–3W
- **Battery:** 3.7V LiPo with PH2.0 connector
- **Communication Protocol:** WebSocket over Wi-Fi

#### **Server**
- **Backend Framework:** Golang
- **STT Service:** Whisper API or Google Speech-to-Text
- **Text Generation:** OpenAI GPT-4 or Hugging Face Transformers
- **TTS Service:** Google TTS or Amazon Polly
- **Database:** Redis (for session management)
- **Hosting:** Cloud provider (AWS, GCP, or Azure)

---

### **5. Data Flow**

1. **User Input:**
   - User speaks into the doll.
   - Doll streams audio data to the server via WebSocket.

2. **Server Processing:**
   - STT converts audio to text.
   - Text generation creates a response.
   - TTS converts the response text to audio.

3. **Output:**
   - Server streams the audio back to the doll.
   - Doll plays the audio response.

---

### **6. API Specification**

#### **WebSocket Communication**
- **Endpoint:** `wss://example.com/conversation`
- **Messages:**
  - **Client → Server:**
    ```json
    {
      "type": "audio_stream",
      "data": "<base64_encoded_audio_chunk>",
      "session_id": "unique_session_id"
    }
    ```
  - **Server → Client:**
    ```json
    {
      "type": "audio_response",
      "data": "<base64_encoded_audio_chunk>"
    }
    ```

---

### **7. Development Milestones**

1. **Week 1–2: IoT Development**
   - Implement audio capture and playback on ESP32.
   - Establish WebSocket communication with a mock server.

2. **Week 3–4: Backend Development**
   - Set up WebSocket server.
   - Integrate STT, text generation, and TTS services.

3. **Week 5: End-to-End Testing**
   - Test the system for real-time interactions.
   - Optimize latency and handle edge cases.

4. **Week 6: Deployment**
   - Deploy the server on a cloud provider.
   - Finalize the doll hardware and firmware.

---

### **8. Risks and Mitigation**

| **Risk**                        | **Mitigation**                                                                 |
|----------------------------------|-------------------------------------------------------------------------------|
| High latency in audio processing | Optimize server processing; use lightweight formats like `.ogg` for streaming.|
| Connection drops mid-session     | Implement auto-reconnect and session restoration logic.                       |
| Limited battery life             | Optimize ESP32 power consumption; use sleep modes when idle.                  |
| Scalability issues               | Use load balancing and horizontally scale WebSocket servers.                  |

---

### **9. Tools and Resources**

- **Hardware:**
  - ESP32 development board (LOLIN32 Lite)
  - INMP441 microphone, MAX98357A amplifier, 8Ω speaker
  - 3.7V LiPo battery

- **Software:**
  - Python or Node.js
  - WebSocket libraries (e.g., `websockets` in Python, `Socket.IO` in Node.js)
  - APIs: Whisper API, OpenAI GPT-4, Google TTS

- **Cloud Resources:**
  - AWS EC2/Google Cloud Compute for hosting the server
  - Redis for session management

---

### **10. Glossary**

- **STT:** Speech-to-Text
- **TTS:** Text-to-Speech
- **IoT:** Internet of Things
- **WebSocket:** A protocol for full-duplex communication over a single TCP connection.