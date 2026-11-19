#ifndef M1_OPTIONS_H
#define M1_OPTIONS_H

#include <stdbool.h>

enum { M1_CHANNELS = 16, M1_STEREO = 2, M1_MIDI_MAX = 127 };

typedef struct {
    const char *soundfont;
    const char *midi;
    const char *render;
    int audio_device;
    int sample_rate;
    int buffer_frames;
    int polyphony;
    int bank;
    int program;
    int channel;
    int velocity;
    double gain;
    bool list;
    bool presets;
    bool effects;
} Options;

int options_parse(Options *options, int argc, char **argv);
void options_usage(const char *name);

#endif
