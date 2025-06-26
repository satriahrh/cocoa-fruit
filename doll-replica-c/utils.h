#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <string.h>

// Base64 encoding function
void encode_base64(const char *input, char *output);

// Timestamp formatting function
void get_timestamp(char *timestamp, size_t size);

#endif // UTILS_H 