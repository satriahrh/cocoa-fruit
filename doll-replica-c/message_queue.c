#include "message_queue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    pthread_mutex_lock(&queue_mutex);
    
    // Free any remaining large binary data
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        if (message_queue[i].large_binary_data) {
            free(message_queue[i].large_binary_data);
            message_queue[i].large_binary_data = NULL;
        }
    }
    
    pthread_mutex_unlock(&queue_mutex);
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

// Helper function to add binary message to queue
int add_binary_message_to_queue(const unsigned char *data, size_t data_size) {
    pthread_mutex_lock(&queue_mutex);
    
    int next_tail = (queue_tail + 1) % MAX_QUEUE_SIZE;
    if (next_tail == queue_head) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is full
    }
    
    if (data_size <= MAX_MESSAGE_LENGTH) {
        // Small data - copy directly
        memcpy(message_queue[queue_tail].message, data, data_size);
        message_queue[queue_tail].length = data_size;
        message_queue[queue_tail].is_binary = 1;
        message_queue[queue_tail].large_binary_data = NULL;
        message_queue[queue_tail].large_binary_size = 0;
    } else {
        // Large data - allocate dynamically
        message_queue[queue_tail].large_binary_data = malloc(data_size);
        if (!message_queue[queue_tail].large_binary_data) {
            pthread_mutex_unlock(&queue_mutex);
            return 0; // Memory allocation failed
        }
        memcpy(message_queue[queue_tail].large_binary_data, data, data_size);
        message_queue[queue_tail].large_binary_size = data_size;
        message_queue[queue_tail].length = 0;
        message_queue[queue_tail].is_binary = 1;
    }
    
    queue_tail = next_tail;
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
}

// Helper function to get binary message from queue
int get_binary_message_from_queue(unsigned char **data, size_t *data_size) {
    pthread_mutex_lock(&queue_mutex);
    
    if (queue_head == queue_tail) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is empty
    }
    
    if (!message_queue[queue_head].is_binary) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Not a binary message
    }
    
    if (message_queue[queue_head].large_binary_data) {
        // Large data - return the pointer directly
        *data = message_queue[queue_head].large_binary_data;
        *data_size = message_queue[queue_head].large_binary_size;
        message_queue[queue_head].large_binary_data = NULL; // Transfer ownership
    } else {
        // Small data - allocate and copy
        *data = malloc(message_queue[queue_head].length);
        if (!*data) {
            pthread_mutex_unlock(&queue_mutex);
            return 0; // Memory allocation failed
        }
        memcpy(*data, message_queue[queue_head].message, message_queue[queue_head].length);
        *data_size = message_queue[queue_head].length;
    }
    
    queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
} 