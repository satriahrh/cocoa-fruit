#include "message_queue.h"
#include <string.h>

// Global queue variables
queued_message_t message_queue[MAX_QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize message queue
void init_message_queue(void) {
    queue_head = 0;
    queue_tail = 0;
    pthread_mutex_init(&queue_mutex, NULL);
}

// Cleanup message queue
void cleanup_message_queue(void) {
    pthread_mutex_destroy(&queue_mutex);
}

// Helper function to add message to queue
int add_message_to_queue(const char *message) {
    pthread_mutex_lock(&queue_mutex);
    
    int next_tail = (queue_tail + 1) % MAX_QUEUE_SIZE;
    if (next_tail == queue_head) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is full
    }
    
    strncpy(message_queue[queue_tail].message, message, MAX_MESSAGE_LENGTH - 1);
    message_queue[queue_tail].message[MAX_MESSAGE_LENGTH - 1] = '\0';
    message_queue[queue_tail].length = strlen(message_queue[queue_tail].message);
    queue_tail = next_tail;
    
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
}

// Helper function to get message from queue
int get_message_from_queue(char *message, int *length) {
    pthread_mutex_lock(&queue_mutex);
    
    if (queue_head == queue_tail) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is empty
    }
    
    strcpy(message, message_queue[queue_head].message);
    *length = message_queue[queue_head].length;
    queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
    
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
} 