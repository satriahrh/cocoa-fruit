#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "config.h"
#include <pthread.h>

// Message queue structure
typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    int length;
    int is_binary;
    // For large binary data, store pointer instead of copying
    unsigned char *large_binary_data;
    size_t large_binary_size;
} queued_message_t;

// Message queue functions
int add_message_to_queue(const char *message);
int add_binary_message_to_queue(const unsigned char *data, size_t data_size);
int get_message_from_queue(char *message, int *length);
int get_binary_message_from_queue(unsigned char **data, size_t *data_size);
void init_message_queue(void);
void cleanup_message_queue(void);

// Global queue variables (extern for access from other modules)
extern queued_message_t message_queue[MAX_QUEUE_SIZE];
extern int queue_head;
extern int queue_tail;
extern pthread_mutex_t queue_mutex;

#endif // MESSAGE_QUEUE_H 