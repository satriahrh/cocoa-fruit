package main

import (
	"bytes"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

const (
	baseURL   = "http://localhost:8080"
	apiKey    = "John"
	apiSecret = "Doe"
)

type JWTResponse struct {
	Token string `json:"token"`
	Type  string `json:"type"`
}

type AudioResponse struct {
	Success   bool   `json:"success"`
	Message   string `json:"message"`
	SessionID string `json:"session_id"`
	Text      string `json:"text"`
}

func main() {
	fmt.Println("🚀 Starting audio streaming test...")

	// Step 1: Get JWT token
	token, err := getJWTToken()
	if err != nil {
		log.Fatalf("Failed to get JWT token: %v", err)
	}
	fmt.Printf("✅ JWT token obtained: %s...\n", token[:20])

	// Step 2: Stream audio file
	err = streamAudioFile(token)
	if err != nil {
		log.Fatalf("Failed to stream audio: %v", err)
	}

	fmt.Println("✅ Audio streaming test completed successfully!")
}

func getJWTToken() (string, error) {
	url := fmt.Sprintf("%s/api/v1/auth/token", baseURL)

	req, err := http.NewRequest("POST", url, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create request: %v", err)
	}

	req.Header.Set("X-API-Key", apiKey)
	req.Header.Set("X-API-Secret", apiSecret)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("failed to send request: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return "", fmt.Errorf("auth failed with status %d: %s", resp.StatusCode, string(body))
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("failed to read response body: %v", err)
	}

	// Simple JSON parsing for token
	// In a real application, you'd use json.Unmarshal
	// For simplicity, we'll extract the token manually
	tokenStart := bytes.Index(body, []byte(`"token":"`))
	if tokenStart == -1 {
		return "", fmt.Errorf("token not found in response: %s", string(body))
	}
	tokenStart += 9 // length of `"token":"`
	tokenEnd := bytes.Index(body[tokenStart:], []byte(`"`))
	if tokenEnd == -1 {
		return "", fmt.Errorf("malformed token in response: %s", string(body))
	}

	token := string(body[tokenStart : tokenStart+tokenEnd])
	return token, nil
}

func streamAudioFile(token string) error {
	// Read the audio file
	audioPath := filepath.Join("sample", "mantap.wav")
	audioData, err := os.ReadFile(audioPath)
	if err != nil {
		return fmt.Errorf("failed to read audio file: %v", err)
	}

	fmt.Printf("📁 Loaded audio file: %s (%d bytes)\n", audioPath, len(audioData))

	// Create request body
	body := bytes.NewReader(audioData)

	// Create HTTP request
	url := fmt.Sprintf("%s/api/v1/audio/stream", baseURL)
	req, err := http.NewRequest("POST", url, body)
	if err != nil {
		return fmt.Errorf("failed to create request: %v", err)
	}

	fmt.Println("🔑 Token: ", token)
	// Set headers
	req.Header.Set("Authorization", "Bearer "+token)
	req.Header.Set("Content-Type", "audio/wav")
	req.Header.Set("Content-Length", fmt.Sprintf("%d", len(audioData)))

	// Send request
	client := &http.Client{Timeout: 60 * time.Second} // Longer timeout for streaming
	fmt.Println("📤 Streaming audio to server...")

	startTime := time.Now()
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %v", err)
	}
	defer resp.Body.Close()

	duration := time.Since(startTime)
	fmt.Printf("⏱️  Request completed in %v\n", duration)

	// Read response
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response body: %v", err)
	}

	fmt.Printf("📊 Response Status: %d\n", resp.StatusCode)
	fmt.Printf("📄 Response Body: %s\n", string(respBody))

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("streaming failed with status %d: %s", resp.StatusCode, string(respBody))
	}

	return nil
}
