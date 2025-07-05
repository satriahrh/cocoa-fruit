# doll-replica-c

A WebSocket client with real-time audio streaming capabilities using PortAudio and libwebsockets.

## Features

- WebSocket client with Basic Authentication
- Real-time audio capture and streaming
- Server-controlled audio recording (start/stop commands)
- Automatic ping/pong heartbeat mechanism
- Cross-platform audio support via PortAudio

## Installation

### Prerequisites

You need to install the required dependencies:

```bash
# Run the installation script
./install_deps.sh
```

Or manually install:

```bash
# Install dependencies via Homebrew
brew install portaudio libwebsockets openssl@1.1
```

### Build

```bash
# Use the Makefile
make build

# Or compile manually
gcc /Users/satria/code/satriahrh/cocoa-fruit/doll-replica-c/main.c -o doll-replica-c \
  -I$(brew --prefix openssl@1.1)/include \
  -L$(brew --prefix openssl@1.1)/lib \
  $(pkg-config --cflags --libs libwebsockets) \
  -lssl -lcrypto \
  -pthread \
  -lportaudio
```

## Usage

### Quick Test with Provided Server

1. **Start the test server** (in one terminal):
   ```bash
   node test_server.js
   ```

2. **Run the client** (in another terminal):
   ```bash
   ./doll-replica-c
   ```

3. **Control audio streaming** (in the server terminal):
   - Type `start` to begin audio capture
   - Type `stop` to end audio capture
   - Type `quit` to exit

4. **Play captured audio** (optional):
   ```bash
   # Install PyAudio first: pip3 install pyaudio
   python3 play_audio.py audio_stream.raw
   ```

### Integration with Your Server

1. Start your WebSocket server on `127.0.0.1:8080/ws`
2. Run the client: `./doll-replica-c`
3. The client will connect and start sending ping messages
4. To start audio streaming, send `start_audio` message from the server
5. To stop audio streaming, send `stop_audio` message from the server

## Audio Configuration

The client is configured with:
- Sample Rate: 16kHz
- Channels: 1 (Mono)
- Sample Format: 16-bit signed integer
- Buffer Size: 512 frames

## Server Protocol

The server can send these commands:
- `start_audio`: Begin audio capture and streaming
- `stop_audio`: Stop audio capture and streaming

Audio data is sent as binary WebSocket frames.

## Files

- `main.c` - Main WebSocket client with audio streaming
- `test_server.js` - Node.js test server for development
- `play_audio.py` - Python script to play captured audio
- `install_deps.sh` - Script to install dependencies
- `Makefile` - Build configuration

## Technical Details

- Uses PortAudio for cross-platform audio capture
- Implements WebSocket client with libwebsockets
- Supports Basic Authentication
- Thread-safe audio streaming with mutex locks
- Automatic cleanup on connection loss