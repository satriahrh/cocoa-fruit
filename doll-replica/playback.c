// playback.c: PortAudio playback implementation
#include "playback.h"
#include <stdio.h>

int playback_init(PaStream **stream) {
    if (Pa_Initialize() != paNoError) {
        printf("Failed to initialize PortAudio\n");
        return 0;
    }
    if (Pa_OpenDefaultStream(stream, 0, CHANNELS, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL) != paNoError) {
        printf("Failed to open PortAudio stream\n");
        Pa_Terminate();
        return 0;
    }
    if (Pa_StartStream(*stream) != paNoError) {
        printf("Failed to start PortAudio stream\n");
        Pa_CloseStream(*stream);
        Pa_Terminate();
        return 0;
    }
    return 1;
}

void playback_close(PaStream *stream) {
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate();
}

int playback_write(PaStream *stream, const void *data, size_t len) {
    if (!stream || !data || len == 0) return 0;
    return Pa_WriteStream(stream, data, len / 2) == paNoError;
}
