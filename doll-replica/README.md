# doll-replica

A minimal C application that connects to a WebSocket server, receives PCM audio, and plays it using PortAudio.

## Overview
- Uses `libwebsockets` for WebSocket communication.
- Uses `PortAudio` for audio playback.
- Authenticates via a JWT token obtained from a local HTTP API.
- Designed for simplicity and modularity.

## Main Flow
1. Retrieve JWT token from HTTP API.
2. Initialize audio playback stream.
3. Configure and establish WebSocket connection (with JWT).
4. Enter service loop to receive and play audio.
5. Clean up resources on exit.

## Component Diagram

```
+-------------------+         +-------------------+         +-------------------+
|                   |         |                   |         |                   |
|   main.c          | <-----> |   playback.h/.c   |         | websocket.h/.c    |
|                   |         |                   |         |                   |
+-------------------+         +-------------------+         +-------------------+
        |                             |                              |
        |                             |                              |
        |                             v                              v
        |                  +-------------------+         +-------------------+
        |                  |   PortAudio Lib   |         | libwebsockets Lib |
        |                  +-------------------+         +-------------------+
        |                             |                              |
        |                             v                              v
        |                  +-------------------+         +-------------------+
        |                  |   Audio Output    |         |  WebSocket Server |
        |                  +-------------------+         +-------------------+
        |
        v
+-------------------+
|   HTTP API        |
|   (for JWT token) |
+-------------------+
```

## Build Instructions

- Requires `libwebsockets` and `PortAudio`.
- Build using the provided Makefile:
  ```sh
  make
  ```

## File Structure
- `main.c`: Orchestrates authentication, playback, and WebSocket connection.
- `playback.h/.c`: Audio playback logic.
- `websocket.h/.c`: WebSocket connection and audio data handling.

## Usage
1. Start the HTTP API server (for JWT).
2. Start the WebSocket audio server.
3. Run `./doll-replica`.

---

## Troubleshooting Build on macOS

- **Missing PortAudio or libwebsockets:**
  - Install with Homebrew:
    ```sh
    brew install portaudio libwebsockets
    ```
- **Linker errors (e.g., undefined symbols):**
  - The Makefile should handle this, but if you modify it or build manually, ensure you add `-lportaudio -lwebsockets` at the end of your gcc command.
- **Header not found:**
  - If you see errors like `portaudio.h: No such file or directory`, you may need to edit the Makefile to add the correct include path:
    ```makefile
    CFLAGS += -I/usr/local/include
    ```
  - For Homebrew, libraries are usually in `/usr/local/lib` or `/opt/homebrew/lib` (Apple Silicon). You may need to add these to the Makefile's LDFLAGS:
    ```makefile
    LDFLAGS += -L/usr/local/lib -L/opt/homebrew/lib
    ```
- **Apple Silicon (M1/M2):**
  - Use Homebrew installed for ARM (`/opt/homebrew`). Adjust include and lib paths accordingly.
- **PortAudio permissions:**
  - If you get audio device errors, check System Preferences > Security & Privacy > Microphone and grant terminal access.
