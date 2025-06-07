package websocket

import (
	"github.com/labstack/echo/v4"
)

// Handler returns an http.Handler for your "/ws" endpoint.
func (s *Server) Handler(c echo.Context) error {
	conn, err := s.upgrader.Upgrade(c.Response(), c.Request(), nil)
	if err != nil {
		return err
	}

	userID := c.Get("user_id").(int)
	deviceID := c.Get("device_id").(string)
	deviceVersion := c.Get("device_version").(string)

	client := NewClient(conn, userID, deviceID, deviceVersion)
	s.hub.Register(client)

	// Start the client goroutines
	client.Run()

	// Register cleanup when client is done
	defer s.hub.Unregister(client)

	// Wait for the client context to be done (connection closed)
	<-client.Context().Done()

	return nil
}
