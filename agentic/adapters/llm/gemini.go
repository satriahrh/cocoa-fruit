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
	// Create system prompt message
	systemPrompt := domain.ChatMessage{
		Role:    domain.SystemRole,
		Content: "You are a friendly talking doll for a 5-year-old girl named [Girl's Name] who goes to Little Caliph kindergarten, KG 1 Al Aqsha class. You are her special friend and helper. Keep your responses very short and simple - maximum 3 sentences. Use simple words that a 5-year-old can understand. Be cheerful, encouraging, and educational. You can help with learning, answer questions about school, and be a good friend.",
	}

	// Prepend system prompt to history if it's not already there
	hasSystemPrompt := false
	if len(history) > 0 && history[0].Role == domain.SystemRole {
		hasSystemPrompt = true
	}

	var geminiHistory []*genai.Content

	// Add system prompt if not already present
	if !hasSystemPrompt {
		geminiHistory = append(geminiHistory, &genai.Content{
			Role: genai.RoleUser, // Gemini uses RoleUser for system prompts
			Parts: []*genai.Part{
				{Text: systemPrompt.Content},
			},
		})
	}

	// Convert domain messages to Gemini format
	for _, msg := range history {
		// Skip system messages as they're handled separately
		if msg.Role == domain.SystemRole {
			continue
		}

		role := genai.RoleModel
		if msg.Role == domain.UserRole {
			role = genai.RoleUser
		}

		geminiHistory = append(geminiHistory, &genai.Content{
			Role: role,
			Parts: []*genai.Part{
				{Text: msg.Content},
			},
		})
	}

	chat, err := g.client.Chats.Create(ctx, "gemini-2.0-flash-001", nil, geminiHistory)
	if err != nil {
		panic(fmt.Errorf("creating chat: %w", err))
	}

	chatSession := &GeminiChatSession{
		client:       g.client,
		chat:         chat,
		systemPrompt: systemPrompt,
	}

	return chatSession, nil
}

type GeminiChatSession struct {
	client       *genai.Client
	chat         *genai.Chat
	systemPrompt domain.ChatMessage
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
	history := make([]domain.ChatMessage, 0, len(resp)+1) // +1 for system prompt

	// Add system prompt at the beginning
	history = append(history, g.systemPrompt)

	// Convert Gemini history to domain format
	for _, part := range resp {
		// Extract text from part.Parts
		var content string
		for _, p := range part.Parts {
			content += p.Text
		}
		role := domain.DollRole
		if part.Role == genai.RoleUser {
			role = domain.UserRole
		}
		history = append(history, domain.ChatMessage{
			Role:    role,
			Content: content,
		})
	}
	return history, nil
}
