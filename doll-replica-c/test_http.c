#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_client.h"

int main(void) {
    printf("🧪 Testing HTTP client functionality...\n");
    
    // Initialize HTTP client
    if (!http_init()) {
        printf("❌ Failed to initialize HTTP client\n");
        return 1;
    }
    
    // Test health check
    if (!http_health_check()) {
        printf("❌ Health check failed - make sure the server is running\n");
        http_cleanup();
        return 1;
    }
    
    // Test JWT token retrieval
    char jwt_token[MAX_JWT_TOKEN_LENGTH];
    if (!http_get_jwt_token(jwt_token, sizeof(jwt_token))) {
        printf("❌ Failed to get JWT token\n");
        http_cleanup();
        return 1;
    }
    
    printf("✅ JWT token obtained: %s\n", jwt_token);
    printf("✅ HTTP client test completed successfully!\n");
    
    http_cleanup();
    return 0;
} 