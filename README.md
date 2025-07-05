# Cocoa Fruit
**Talking Doll with AI-Driven Real-Time Conversation**

This project is an IoT-based talking doll that uses AI to engage in real-time conversations. The doll captures user speech, processes it through a cloud-based server for speech-to-text (STT), intelligent text generation, and text-to-speech (TTS), and responds with audio playback. The system leverages WebSocket for persistent, low-latency communication and a message broker for scalable architecture, enabling seamless and interactive conversational experiences.

### Key Features:
- **Real-Time Processing:** Speech-to-Text (STT), Text Generation, and Text-to-Speech (TTS).
- **Persistent Connection:** WebSocket-based communication for low latency and real-time interaction.
- **Message Broker Architecture:** Decoupled communication between components for scalability.
- **IoT Integration:** ESP32 microcontroller, microphone (INMP441), and speaker (MAX98357A) for input and output.
- **AI-Powered Responses:** GPT-based text generation for intelligent and context-aware conversations.

### Architecture:
```
HTTP Audio Stream → Message Broker → WebSocket Broadcast
     ↓                    ↓              ↓
Transcription    Channel-based    Real-time Clients
Processing       (Single Pod)     (ESP32, Web, etc.)
```

### Tech Stack:
- **IoT Hardware:** ESP32, INMP441 (microphone), MAX98357A (audio amplifier), 8Ω speaker.
- **Backend:** Golang, WebSocket server, message broker, cloud-based STT, GPT, and TTS services.
- **Communication Protocol:** WebSocket for bidirectional streaming, message broker for internal communication.

### About this Project

This is a mono repo project, which means all the code is stored in a single git repository.

This table summarizes the structure of the project:

| Service name | Language | Purpose                                                                                      |
|--------------|----------|----------------------------------------------------------------------------------------------|
| agentic      | Go       | The core application of Agentic AI with message broker architecture. Consolidates input, processing, and output with scalable communication between components. |
| doll-replica | Go       | This service replicates the behavior of the IoT device for testing the agentic service. |
| doll-replica-c | C       | C-based WebSocket client with real-time audio streaming capabilities using PortAudio and libwebsockets. |

### Recent Updates

#### Message Broker Architecture
- **Simplified Flow**: HTTP adapter → Message Broker → WebSocket adapter
- **Routing Key Support**: Session-based message routing for targeted communication
- **Channel-based Implementation**: Current in-memory solution using Go channels
- **Future-ready**: Easy migration to external brokers (RabbitMQ, Redis, etc.)
- **Scalable Design**: Prepared for multi-pod deployment

#### Key Components
- **HTTP Handler** (`adapters/http/handler.go`): Receives audio streams and publishes transcription results
- **Message Broker** (`adapters/message_broker/channel.go`): In-memory message routing using Go channels
- **WebSocket Server** (`adapters/websocket/server.go`): Subscribes to messages and broadcasts to clients
- **Speech Service** (`adapters/speech/google.go`): Google Speech-to-Text integration

### Documentation

- [`agentic/STREAMING_TRANSCRIPTION_README.md`](agentic/STREAMING_TRANSCRIPTION_README.md) - Streaming transcription with message broker
- [`agentic/HTTP_API_README.md`](agentic/HTTP_API_README.md) - HTTP API with message broker integration
- [`agentic/MESSAGE_BROKER_README.md`](agentic/MESSAGE_BROKER_README.md) - Message broker architecture and implementation
- [`doll-replica-c/README.md`](doll-replica-c/README.md) - C-based WebSocket client documentation