package main

import (
	"crypto/subtle"
	"log"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/llm"
	"github.com/satriahrh/cocoa-fruit/agentic/adapters/websocket"
	"github.com/satriahrh/cocoa-fruit/agentic/usecase"
	"github.com/subosito/gotenv"
)

func main() {
	gotenv.Load()

	geminiLlm := llm.NewGeminiClient()
	svc := usecase.NewChatService(geminiLlm)

	server := websocket.NewServer(svc)

	e := echo.New()
	e.Use(middleware.BasicAuth(func(username, password string, c echo.Context) (bool, error) {
		// Be careful to use constant time comparison to prevent timing attacks
		if subtle.ConstantTimeCompare([]byte(username), []byte("John")) == 1 &&
			subtle.ConstantTimeCompare([]byte(password), []byte("Doe")) == 1 {
			const userID int = 99
			c.Set("user_id", userID)
			return true, nil
		}
		return false, nil
	}))
	e.GET("/ws", server.Handler)

	log.Println("listening on :8080")
	log.Fatal(e.Start(":8080"))
}
