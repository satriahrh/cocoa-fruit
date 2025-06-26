#include "utils.h"

// Base64 encoding table
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Function to encode string to base64 (for Basic Auth)
void encode_base64(const char *input, char *output) {
    int input_length = strlen(input);
    int i, j = 0;
    
    for (i = 0; i < input_length; i += 3) {
        unsigned char byte1 = input[i];
        unsigned char byte2 = (i + 1 < input_length) ? input[i + 1] : 0;
        unsigned char byte3 = (i + 2 < input_length) ? input[i + 2] : 0;
        
        output[j++] = base64_chars[byte1 >> 2];
        output[j++] = base64_chars[((byte1 & 3) << 4) | (byte2 >> 4)];
        output[j++] = (i + 1 < input_length) ? base64_chars[((byte2 & 15) << 2) | (byte3 >> 6)] : '=';
        output[j++] = (i + 2 < input_length) ? base64_chars[byte3 & 63] : '=';
    }
    output[j] = '\0';
}

// Helper function to get current timestamp string
void get_timestamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, size, "%H:%M:%S", tm_info);
} 