package llm

import (
	"context"
	"fmt"

	"google.golang.org/genai"

	"github.com/satriahrh/cocoa-fruit/agentic/domain"
)

type GeminiClient struct {
	client *genai.Client
}

func NewGeminiClient() domain.Llm {
	ctx := context.TODO()

	client, err := genai.NewClient(
		ctx,
		&genai.ClientConfig{
			HTTPOptions: genai.HTTPOptions{APIVersion: "v1"},
		},
	)
	if err != nil {
		panic(fmt.Errorf("creating genai client: %w", err))
	}

	return &GeminiClient{client: client}
}

func (g *GeminiClient) Generate(prompt string) (string, error) {
	ctx := context.TODO()

	// call the same model you were using
	resp, err := g.client.Models.GenerateContent(
		ctx,
		"gemini-2.0-flash-001", // or "gemini-pro"
		genai.Text(prompt),     // your prompt
		nil,                    // any extra options
	)
	if err != nil {
		return "", fmt.Errorf("generate content: %w", err)
	}

	// pull out the text
	text := resp.Text()
	return text, nil
}

func (g *GeminiClient) GenerateChat(ctx context.Context, history []domain.ChatMessage) (domain.ChatSession, error) {
	geminiHistory := make([]*genai.Content, len(history))
	for i, msg := range history {
		role := genai.RoleModel
		if msg.Role == domain.UserRole {
			role = genai.RoleUser
		}
		geminiHistory[i] = &genai.Content{
			Role: role,
			Parts: []*genai.Part{
				{Text: msg.Content},
			},
		}
	}
	chat, err := g.client.Chats.Create(ctx, "gemini-2.0-flash-001", nil, geminiHistory)
	if err != nil {
		panic(fmt.Errorf("creating chat: %w", err))
	}

	chatSession := &GeminiChatSession{
		client: g.client,
		chat:   chat,
	}

	return chatSession, nil
}

type GeminiChatSession struct {
	client *genai.Client
	chat   *genai.Chat
}

// SendMessage implements domain.ChatSession.
func (g *GeminiChatSession) SendMessage(ctx context.Context, message domain.ChatMessage) (
	domain.ChatMessage,
	error,
) {
	resp, err := g.chat.SendMessage(ctx, genai.Part{Text: message.Content})
	if err != nil {
		return domain.ChatMessage{}, fmt.Errorf("send message: %w", err)
	}

	// Pull out the text from the response
	return domain.ChatMessage{
		Role:    domain.DollRole,
		Content: resp.Text(),
	}, nil
}

func (g *GeminiChatSession) History() ([]domain.ChatMessage, error) {
	resp := g.chat.History(false)
	history := make([]domain.ChatMessage, len(resp))
	for i, part := range resp {
		// Extract text from part.Parts
		var content string
		for _, p := range part.Parts {
			content += p.Text
		}
		role := domain.DollRole
		if part.Role == genai.RoleUser {
			role = domain.UserRole
		}
		history[i] = domain.ChatMessage{
			Role:    role,
			Content: content,
		}
	}
	return history, nil
}
