package websocket

import (
	"context"
	"encoding/base64"
	"fmt"
	"io/ioutil"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/speech"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

type Client struct {
	conn          *websocket.Conn
	send          chan []byte
	incomingPing  chan string
	ctx           context.Context
	cancel        context.CancelFunc
	mu            sync.RWMutex
	closed        bool
	userID        int
	deviceID      string
	deviceVersion string
	chatService   *usecase.ChatService
	googleSpeech  *speech.GoogleSpeech
}

// Message types following your integration platform patterns
type Message struct {
	Type      string                 `json:"type"`
	DeviceID  string                 `json:"device_id"`
	Timestamp time.Time              `json:"timestamp"`
	Data      map[string]interface{} `json:"data"`
	Metadata  map[string]string      `json:"metadata,omitempty"`
}

type ErrorResponse struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	Details string `json:"details,omitempty"`
}

const (
	writeWait      = 10 * time.Second
	pongWait       = 60 * time.Second
	pingPeriod     = 30 * time.Second
	maxMessageSize = 512 * 1024 // Increased for audio data
)

// AudioResponse represents the JSON response sent to the doll
type AudioResponse struct {
	Text  string `json:"text"`
	Audio string `json:"audio"`
}

// getStaticAudioBase64 reads the congratulations WAV file and converts it to base64
func getStaticAudioBase64() string {
	audioData, err := ioutil.ReadFile("sample/congratulations-male-mono.wav")
	if err != nil {
		log.WithCtx(context.Background()).Error("❌ Failed to read audio file", zap.Error(err))
		return ""
	}

	return base64.StdEncoding.EncodeToString(audioData)
}

// isPrintableText checks if the data contains mostly printable ASCII characters
func isPrintableText(data []byte) bool {
	if len(data) == 0 {
		return true
	}

	printableCount := 0
	for _, b := range data {
		if b >= 32 && b <= 126 { // Printable ASCII range
			printableCount++
		}
	}

	// Consider it text if more than 80% is printable
	return float64(printableCount)/float64(len(data)) > 0.8
}

// NewClient creates a new WebSocket client
func NewClient(conn *websocket.Conn, userID int, deviceID, deviceVersion string, chatService *usecase.ChatService, googleSpeech *speech.GoogleSpeech) *Client {
	ctx := context.TODO()
	ctx = context.WithValue(ctx, "user_id", userID)
	ctx = context.WithValue(ctx, "device_id", deviceID)
	ctx = context.WithValue(ctx, "device_version", deviceVersion)
	ctx, cancel := context.WithCancel(ctx)
	return &Client{
		conn:          conn,
		send:          make(chan []byte, 256),
		incomingPing:  make(chan string, 1),
		ctx:           ctx,
		cancel:        cancel,
		closed:        false,
		userID:        userID,
		deviceID:      deviceID,
		deviceVersion: deviceVersion,
		chatService:   chatService,
		googleSpeech:  googleSpeech,
	}
}

func (c *Client) Run() {
	// Set up WebSocket handlers
	c.setupHandlers()

	// Start the goroutines
	go c.Ping()
	go c.readPump()
	go c.writePump()
}

// setupHandlers configures all WebSocket message handlers
func (c *Client) setupHandlers() {
	c.conn.SetCloseHandler(func(code int, text string) error {
		log.WithCtx(c.ctx).Info("🔌 WebSocket connection closed",
			zap.Int("code", code),
			zap.String("text", text),
			zap.String("device_id", c.ctx.Value("device_id").(string)),
			zap.Int("user_id", c.ctx.Value("user_id").(int)))
		c.Close()
		return nil
	})

	// Handle incoming ping messages - respond with pong
	c.conn.SetPingHandler(func(appData string) error {
		log.WithCtx(c.ctx).Debug("🏓 Received ping from doll",
			zap.String("ping_data", appData),
			zap.String("device_id", c.ctx.Value("device_id").(string)),
			zap.Int("user_id", c.ctx.Value("user_id").(int)))
		c.incomingPing <- appData
		return c.conn.WriteControl(websocket.PongMessage, []byte(appData), time.Now().Add(writeWait))
	})

	// Handle incoming pong messages - update read deadline
	c.conn.SetPongHandler(func(appData string) error {
		log.WithCtx(c.ctx).Debug("🏓 Received pong from doll",
			zap.String("pong_data", appData),
			zap.String("device_id", c.ctx.Value("device_id").(string)),
			zap.Int("user_id", c.ctx.Value("user_id").(int)))
		c.conn.SetReadDeadline(time.Now().Add(pongWait))
		return nil
	})
}

// Close gracefully closes the client connection
func (c *Client) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.closed {
		return
	}

	c.closed = true
	if c.cancel != nil {
		c.cancel()
	}

	if c.conn != nil {
		c.conn.Close()
	}

	if c.send != nil {
		close(c.send)
	}
}

// IsClosed returns true if the client connection is closed
func (c *Client) IsClosed() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.closed
}

// Context returns the client's context
func (c *Client) Context() context.Context {
	return c.ctx
}

func (c *Client) Ping() {
	for {
		select {
		case <-c.incomingPing:
		case <-time.After(pingPeriod):
			if c.IsClosed() {
				log.WithCtx(c.ctx).Debug("Connection closed, stopping ping routine")
				return
			}

			c.mu.RLock()
			conn := c.conn
			c.mu.RUnlock()

			if conn == nil {
				log.WithCtx(c.ctx).Debug("Connection is nil, stopping ping routine")
				return
			}

			if err := conn.WriteControl(websocket.PingMessage, []byte{}, time.Now().Add(writeWait)); err != nil {
				log.WithCtx(c.ctx).Error("Failed to send ping", zap.Error(err))
				c.Close() // Close the connection on ping failure
				return
			}
			log.WithCtx(c.ctx).Debug("Ping sent")
		case <-c.ctx.Done():
			log.WithCtx(c.ctx).Debug("Context cancelled, stopping ping routine")
			return
		}
	}
}

// readPump handles incoming WebSocket messages
func (c *Client) readPump() {
	defer c.Close()

	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))

	// Create channels for chat service
	inputChan := make(chan string, 10)
	outputChan := make(chan string, 10)

	// Start chat service in a goroutine
	go func() {
		if err := c.chatService.Execute(c.ctx, c.userID, inputChan, outputChan); err != nil {
			log.WithCtx(c.ctx).Error("❌ Chat service error", zap.Error(err))
		}
	}()

	// Start response handler goroutine
	go func() {
		for {
			select {
			case response := <-outputChan:
				if !c.IsClosed() {
					// Send response back to the doll as raw text
					if err := c.SendMessage([]byte(response)); err != nil {
						log.WithCtx(c.ctx).Error("❌ Failed to send response to doll", zap.Error(err))
					}
				}
			case <-c.ctx.Done():
				return
			}
		}
	}()

	for {
		if c.IsClosed() {
			return
		}

		messageType, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.WithCtx(c.ctx).Error("❌ WebSocket read error",
					zap.Error(err),
					zap.String("device_id", c.ctx.Value("device_id").(string)),
					zap.Int("user_id", c.ctx.Value("user_id").(int)))
			}
			return
		}

		// Log the received message
		msgType := "text"
		messagePreview := string(message)
		if len(messagePreview) > 50 {
			messagePreview = messagePreview[:50] + "..."
		}

		// Check if it's binary audio data (binary message type or large binary message)
		if messageType == websocket.BinaryMessage || (len(message) > 1000 && !isPrintableText(message)) {
			msgType = "audio"
			messagePreview = fmt.Sprintf("[BINARY AUDIO: %d bytes]", len(message))

			log.WithCtx(c.ctx).Info("🎤 Received binary audio message, transcribing...",
				zap.Int("websocket_message_type", messageType),
				zap.String("device_id", c.ctx.Value("device_id").(string)),
				zap.Int("user_id", c.ctx.Value("user_id").(int)))

			// Convert binary to base64 for the speech service
			base64Audio := base64.StdEncoding.EncodeToString(message)

			log.WithCtx(c.ctx).Info("🔄 Converted binary to base64",
				zap.Int("binary_bytes", len(message)),
				zap.Int("base64_length", len(base64Audio)),
				zap.String("device_id", c.ctx.Value("device_id").(string)),
				zap.Int("user_id", c.ctx.Value("user_id").(int)))

			// Transcribe audio to text
			transcript, err := c.googleSpeech.TranscribeAudio(c.ctx, base64Audio)
			if err != nil {
				log.WithCtx(c.ctx).Error("❌ Failed to transcribe audio",
					zap.Error(err),
					zap.String("device_id", c.ctx.Value("device_id").(string)),
					zap.Int("user_id", c.ctx.Value("user_id").(int)))
				continue
			}

			if transcript == "" {
				log.WithCtx(c.ctx).Warn("⚠️ Audio transcription returned empty result",
					zap.String("device_id", c.ctx.Value("device_id").(string)),
					zap.Int("user_id", c.ctx.Value("user_id").(int)))
				continue
			}

			log.WithCtx(c.ctx).Info("✅ Audio transcribed successfully",
				zap.String("transcript", transcript),
				zap.String("device_id", c.ctx.Value("device_id").(string)),
				zap.Int("user_id", c.ctx.Value("user_id").(int)))

			// Send transcribed text to chat service
			select {
			case inputChan <- transcript:
				// Transcribed text sent to chat service successfully
			case <-c.ctx.Done():
				return
			default:
				log.WithCtx(c.ctx).Error("❌ Chat service input channel is full")
			}
			continue
		}

		log.WithCtx(c.ctx).Info("📨 Received message from doll",
			zap.String("type", msgType),
			zap.String("preview", messagePreview),
			zap.Int("message_length", len(message)),
			zap.String("device_id", c.ctx.Value("device_id").(string)),
			zap.Int("user_id", c.ctx.Value("user_id").(int)))

		// Handle as regular text message
		select {
		case inputChan <- string(message):
			// Message sent to chat service successfully
		case <-c.ctx.Done():
			return
		default:
			log.WithCtx(c.ctx).Error("❌ Chat service input channel is full")
		}
	}
}

// writePump handles outgoing WebSocket messages
func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.Close()
	}()

	for {
		select {
		case message, ok := <-c.send:
			if c.IsClosed() {
				return
			}

			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			if err := c.conn.WriteMessage(websocket.TextMessage, message); err != nil {
				log.WithCtx(c.ctx).Error("❌ Failed to write message to doll",
					zap.Error(err),
					zap.String("device_id", c.ctx.Value("device_id").(string)),
					zap.Int("user_id", c.ctx.Value("user_id").(int)))
				return
			}

			log.WithCtx(c.ctx).Info("📤 Sent message to doll",
				zap.String("preview", func() string {
					msgStr := string(message)
					if len(msgStr) > 50 {
						return msgStr[:50] + "..."
					}
					return msgStr
				}()),
				zap.Int("message_length", len(message)),
				zap.String("device_id", c.ctx.Value("device_id").(string)),
				zap.Int("user_id", c.ctx.Value("user_id").(int)))

		case <-c.ctx.Done():
			return
		}
	}
}

// SendMessage sends a message to the client safely
func (c *Client) SendMessage(message []byte) error {
	if c.IsClosed() {
		return websocket.ErrCloseSent
	}

	select {
	case c.send <- message:
		return nil
	case <-c.ctx.Done():
		return c.ctx.Err()
	default:
		// Channel is full, close the connection
		c.Close()
		return websocket.ErrCloseSent
	}
}
