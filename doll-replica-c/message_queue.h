#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "config.h"
#include <pthread.h>

// Message queue structure
typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    int length;
} queued_message_t;

// Message queue functions
int add_message_to_queue(const char *message);
int get_message_from_queue(char *message, int *length);
void init_message_queue(void);
void cleanup_message_queue(void);

// Global queue variables (extern for access from other modules)
extern queued_message_t message_queue[MAX_QUEUE_SIZE];
extern int queue_head;
extern int queue_tail;
extern pthread_mutex_t queue_mutex;

#endif // MESSAGE_QUEUE_H 