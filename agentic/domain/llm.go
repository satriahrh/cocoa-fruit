package domain

import "context"

// Llm abstracts any chat/LLM provider.
type Llm interface {
	// Generate takes a user prompt and returns the model's reply.
	Generate(prompt string) (string, error)
	GenerateChat(ctx context.Context, history []ChatMessage) (ChatSession, error)
}

type ChatSession interface {
	SendMessage(ctx context.Context, message ChatMessage) (ChatMessage, error)
	History() ([]ChatMessage, error)
}

type ChatMessage struct {
	Role    Role   `json:"role"`
	Content string `json:"content"`
}

type Role string

const (
	UserRole   Role = "user"
	DollRole   Role = "doll"
	SystemRole Role = "system"
)
