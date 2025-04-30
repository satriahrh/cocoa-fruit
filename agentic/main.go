package main

import (
	"crypto/sha256"
	"encoding/hex"
	"log"
	"net/http"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

func wsHandler(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("upgrade error:", err)
		return
	}
	defer conn.Close()

	log.Println("Client connected, awaiting text messages...")
	for {
		msgType, data, err := conn.ReadMessage()
		if err != nil {
			log.Println("read error:", err)
			break
		}
		if msgType != websocket.TextMessage {
			log.Println("non-text message ignored")
			continue
		}

		sum := sha256.Sum256(data)
		hash := hex.EncodeToString(sum[:])
		log.Printf("received: %s → hash: %s\n", data, hash)

		if err := conn.WriteMessage(websocket.TextMessage, []byte(hash)); err != nil {
			log.Println("write error:", err)
			break
		}
	}
	log.Println("Connection closed.")
}

func main() {
	http.HandleFunc("/ws", wsHandler)
	log.Fatal(http.ListenAndServe(":8080", nil))
}
