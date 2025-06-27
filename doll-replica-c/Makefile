OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
PORTAUDIO_PREFIX := $(shell brew --prefix portaudio)
CJSON_PREFIX := $(shell brew --prefix cjson)
CFLAGS := -I$(OPENSSL_PREFIX)/include -I$(PORTAUDIO_PREFIX)/include -I$(CJSON_PREFIX)/include $(shell pkg-config --cflags libwebsockets)
LDFLAGS := -L$(OPENSSL_PREFIX)/lib -L$(PORTAUDIO_PREFIX)/lib -L$(CJSON_PREFIX)/lib $(shell pkg-config --libs libwebsockets) -lssl -lcrypto -pthread -lportaudio -lcjson

# Source files
SOURCES := main.c utils.c message_queue.c websocket_client.c input_handler.c audio.c
OBJECTS := $(SOURCES:.c=.o)

# Default target
all: build

# Build the WebSocket client
build: $(OBJECTS)
	@gcc $(OBJECTS) -o doll-replica-c $(LDFLAGS) -lcjson
	@echo "✅ Build successful! Run with: ./doll-replica-c"

# Compile individual source files
%.o: %.c
	@gcc $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@rm -f doll-replica-c $(OBJECTS)
	@echo "🧹 Cleaned build artifacts"

# Install dependencies (macOS)
install-deps:
	@echo "📦 Installing dependencies..."
	@brew install libwebsockets openssl@1.1 portaudio cjson
	@echo "✅ Dependencies installed!"

# Run the client
run: build
	@echo "🚀 Starting WebSocket client..."
	@./doll-replica-c

.PHONY: all build clean install-deps run
