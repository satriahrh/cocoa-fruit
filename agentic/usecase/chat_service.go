package usecase

import (
	"context"
	"encoding/json"
	"fmt"
	"os"

	"github.com/satriahrh/cocoa-fruit/agentic/domain"
)

type ChatService struct {
	llm domain.Llm
}

func NewChatService(gen domain.Llm) *ChatService {
	return &ChatService{llm: gen}
}

func (s *ChatService) Execute(ctx context.Context, userID int, input chan string, output chan string) error {
	filename := fmt.Sprintf("chat_history_%d.json", userID)
	var history []domain.ChatMessage
	file, err := os.Open(filename)
	if err != nil {
		if !os.IsNotExist(err) {
			return fmt.Errorf("error opening file: %w", err)
		}
	} else {
		defer file.Close()
		decoder := json.NewDecoder(file)
		err = decoder.Decode(&history)
		if err != nil {
			return fmt.Errorf("error decoding JSON: %w", err)
		}
	}

	chatSession, err := s.llm.GenerateChat(ctx, history)
	if err != nil {
		return err
	}

	for {
		select {
		case msg := <-input:
			responseMessage, err := chatSession.SendMessage(ctx, domain.ChatMessage{
				Role:    domain.UserRole,
				Content: msg,
			})
			if err != nil {
				output <- "ERROR: " + err.Error()
				continue
			}
			output <- responseMessage.Content
		case <-ctx.Done():
			history, err := chatSession.History()
			if err != nil {
				fmt.Println("Error getting history:", err)
			}

			// Store history to a JSON file
			filename := fmt.Sprintf("chat_history_%d.json", userID)
			file, err := os.Create(filename)
			if err != nil {
				fmt.Println("Error creating file:", err)
			} else {
				encoder := json.NewEncoder(file)
				err = encoder.Encode(history)
				if err != nil {
					fmt.Println("Error encoding history to JSON:", err)
				}
				file.Close()
			}

			fmt.Println("History stored to", filename)
			return nil
		}
	}
}
