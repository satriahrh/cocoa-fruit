SRCS = main.c playback.c websocket.c
OBJS = $(SRCS:.c=.o)

all: build

build: $(OBJS)
	gcc -o doll-replica $(OBJS) -I/opt/homebrew/include -L/opt/homebrew/lib -lwebsockets -lportaudio

%.o: %.c
	gcc -c $< -o $@ -I/opt/homebrew/include

clean:
	rm -f *.o doll-replica