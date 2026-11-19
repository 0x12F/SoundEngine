#ifndef M1_ENGINE_H
#define M1_ENGINE_H

#include "events.h"
#include "options.h"
#include <fluidsynth.h>

typedef struct {
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    int soundfont_id;
} Engine;

int engine_open(Engine *engine, const Options *options);
void engine_close(Engine *engine);
void engine_presets(Engine *engine);
void engine_event(Engine *engine, Event event);
void engine_panic(Engine *engine);
int engine_render(Engine *engine, float *stereo, int frames);

#endif
