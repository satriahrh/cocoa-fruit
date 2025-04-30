package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/gorilla/websocket"
)

const serverURL = "ws://localhost:8080/ws"

func main() {
	// Connect to the WebSocket server
	conn, _, err := websocket.DefaultDialer.Dial(serverURL, nil)
	if err != nil {
		log.Fatalf("Failed to connect to server: %v", err)
	}
	defer conn.Close()

	// Handle incoming messages in a separate goroutine
	go func() {
		for {
			_, message, err := conn.ReadMessage()
			if err != nil {
				log.Println("Error reading message:", err)
				return
			}
			fmt.Printf("Received: %s\n", message)
		}
	}()

	// Set up a signal handler to gracefully shut down on interrupt
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sigChan
		log.Println("Shutting down...")
		conn.Close()
		os.Exit(0)
	}()

	// Read user input and send messages to the server
	reader := bufio.NewReader(os.Stdin)
	fmt.Println("Enter messages to send to the server (type 'exit' to quit):")
	for {
		fmt.Print("> ")
		text, _ := reader.ReadString('\n')
		if text == "exit\n" {
			break
		}
		if err := conn.WriteMessage(websocket.TextMessage, []byte(text)); err != nil {
			log.Println("Error sending message:", err)
			break
		}
	}
}
