# doll-replica-c

## Installation

```
gcc /Users/satria/code/satriahrh/cocoa-fruit/doll-replica-c/main.c -o doll-replica-c \
  -I$(brew --prefix openssl@1.1)/include \
  -L$(brew --prefix openssl@1.1)/lib \
  $(pkg-config --cflags --libs libwebsockets) \
  -lssl -lcrypto \
  -pthread
```