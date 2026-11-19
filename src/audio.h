#ifndef M1_AUDIO_H
#define M1_AUDIO_H

#include "engine.h"
#include <portaudio.h>

typedef struct {
    PaStream *stream;
    Engine *engine;
    EventQueue *queue;
    atomic_uint underruns;
    atomic_bool failed;
} Audio;

int audio_list(void);
int audio_open(Audio *audio, Engine *engine, EventQueue *queue, const Options *options);
void audio_close(Audio *audio);

#endif
