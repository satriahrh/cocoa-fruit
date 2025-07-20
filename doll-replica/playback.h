// playback.h: PortAudio playback interface
#ifndef PLAYBACK_H
#define PLAYBACK_H


#include <portaudio.h>
#include <stddef.h>

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define FRAMES_PER_BUFFER 512

int playback_init(PaStream **stream);
void playback_close(PaStream *stream);
int playback_write(PaStream *stream, const void *data, size_t len);

#endif // PLAYBACK_H
