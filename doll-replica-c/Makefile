OPENSSL_PREFIX := $(shell brew --prefix openssl@1.1)
CFLAGS := -I$(OPENSSL_PREFIX)/include $(shell pkg-config --cflags libwebsockets)
LDFLAGS := -L$(OPENSSL_PREFIX)/lib $(shell pkg-config --libs libwebsockets) -lssl -lcrypto -pthread

build:
	@gcc /Users/satria/code/satriahrh/cocoa-fruit/doll-replica-c/main.c -o doll-replica-c $(CFLAGS) $(LDFLAGS)
