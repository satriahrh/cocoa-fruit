# Lemon
**Talking Doll with AI-Driven Real-Time Conversation**

This project is an IoT-based talking doll that uses AI to engage in real-time conversations. The doll captures user speech, processes it through a cloud-based server for speech-to-text (STT), intelligent text generation, and text-to-speech (TTS), and responds with audio playback. The system leverages WebSocket for persistent, low-latency communication, enabling seamless and interactive conversational experiences.

### Key Features:
- **Real-Time Processing:** Speech-to-Text (STT), Text Generation, and Text-to-Speech (TTS).
- **Persistent Connection:** WebSocket-based communication for low latency and real-time interaction.
- **IoT Integration:** ESP32 microcontroller, microphone (INMP441), and speaker (MAX98357A) for input and output.
- **AI-Powered Responses:** GPT-based text generation for intelligent and context-aware conversations.

### Tech Stack:
- **IoT Hardware:** ESP32, INMP441 (microphone), MAX98357A (audio amplifier), 8Ω speaker.
- **Backend:** Python or Node.js, WebSocket server, cloud-based STT, GPT, and TTS services.
- **Communication Protocol:** WebSocket for bidirectional streaming.

### About this Project

This is a mono repo project, which mean all the code are stored in a single git repository.

This table summarize the structure of the project.


| Service name | Language | Purpose                                                                                      |
|--------------|----------|----------------------------------------------------------------------------------------------|
| agentic      | Go       | The core application of Agentic AI, where it will consolidate input, processing, and output. |
| doll-replica | Go       | This service is only to replicate the behavior of the IoT device, to be able for us testing the agentic service. |