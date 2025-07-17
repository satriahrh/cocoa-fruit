# Agentic Makefile
.PHONY: all build test clean

# Default target
all: build

# Build the main server
build:
	@echo "🔨 Building agentic server..."
	@go build -o agentic .
	@echo "✅ Build successful! Run with: ./agentic"

# Run the audio streaming test
test-audio:
	@echo "🧪 Running audio streaming test..."
	@go run test_audio_streaming.go

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -f agentic
	@rm -f audio_chunk_*.bin
	@echo "✅ Cleaned!"

# Run the server
run: build
	@echo "🚀 Starting agentic server..."
	@./agentic

# Install dependencies
deps:
	@echo "📦 Installing Go dependencies..."
	@go mod tidy
	@echo "✅ Dependencies installed!" 