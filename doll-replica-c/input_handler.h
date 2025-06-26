#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <pthread.h>

// Input handler functions
int start_input_thread(void);
void stop_input_thread(void);
void wait_for_input_thread(void);

// Input thread function
void* input_thread(void *arg);

#endif // INPUT_HANDLER_H 