package websocket

import (
	"context"
	"strings"

	"github.com/gorilla/websocket"
	"github.com/labstack/echo/v4"
)

// Handler returns an http.Handler for your "/ws" endpoint.
func (s *Server) Handler(c echo.Context) error {
	conn, err := s.upgrader.Upgrade(c.Response(), c.Request(), nil)
	if err != nil {
		return err
	}
	defer conn.Close()

	userID := c.Get("user_id").(int)
	input := make(chan string)
	output := make(chan string)
	go func(userID int, ctx context.Context) {
		err := s.svc.Execute(ctx, userID, input, output)
		if err != nil {
			output <- "ERROR: " + err.Error()
		}
	}(userID, c.Request().Context())

	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			break
		}
		trimmed := strings.TrimSpace(string(msg))
		if trimmed == "" {
			continue // skip blank/whitespace-only messages
		}

		input <- trimmed

		responseMessage := <-output
		if strings.HasPrefix(responseMessage, "ERROR: ") {
			break
		}
		if err := conn.WriteMessage(websocket.TextMessage, []byte(responseMessage)); err != nil {
			break
		}
	}
	return nil
}
