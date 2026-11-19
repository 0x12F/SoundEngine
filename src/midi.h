#ifndef M1_MIDI_H
#define M1_MIDI_H

#include "events.h"
#include "options.h"
#include <alsa/asoundlib.h>

typedef struct { snd_seq_t *seq; int port; int channel; } Midi;

int midi_list(void);
int midi_open(Midi *midi, const Options *options);
int midi_read(Midi *midi, EventQueue *queue);
void midi_close(Midi *midi);

#endif
