#include "engine.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

enum { TEST_RATE = 48000, BLOCK = 256, TEST_KEY = 69, TEST_VELOCITY = 100,
       SUSTAIN_CC = 64, SOSTENUTO_CC = 66, PEDAL_DOWN = 127 };

static double energy(Engine *engine, int blocks)
{
    float samples[BLOCK * M1_STEREO];
    double result = 0;
    for (int b = 0; b < blocks; ++b) {
        assert(engine_render(engine, samples, BLOCK) == FLUID_OK);
        result = 0;
        for (int i = 0; i < BLOCK * M1_STEREO; ++i) {
            assert(isfinite(samples[i]) && fabsf(samples[i]) <= 1.0f);
            result += (double)samples[i] * samples[i];
        }
    }
    return result;
}

static void note(Engine *e, int velocity)
{
    engine_event(e, (Event){NOTE_ON, 0, TEST_KEY, velocity});
}

static void pedal(Engine *e, int cc, int value)
{
    engine_event(e, (Event){CONTROL, 0, cc, value});
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Options o = { .soundfont = argv[1], .sample_rate = TEST_RATE,
                  .polyphony = 64, .gain = 0.3 };
    Engine engine;
    assert(engine_open(&engine, &o) == 0);
    assert(energy(&engine, 4) == 0);
    note(&engine, TEST_VELOCITY);
    double loud = energy(&engine, 20);
    assert(loud > 0.01);
    note(&engine, 0);
    assert(energy(&engine, 200) < loud * 0.001);
    note(&engine, TEST_VELOCITY / 2);
    double quiet = energy(&engine, 20);
    assert(quiet > 0 && quiet < loud);
    engine_panic(&engine);
    energy(&engine, 20);
    pedal(&engine, 67, PEDAL_DOWN);
    note(&engine, TEST_VELOCITY);
    double soft = energy(&engine, 20);
    assert(soft > 0 && soft < loud * 0.5);
    pedal(&engine, 67, 0);
    engine_panic(&engine);
    energy(&engine, 20);
    for (int cc = SUSTAIN_CC; cc <= SOSTENUTO_CC; cc += 2) {
        note(&engine, TEST_VELOCITY);
        energy(&engine, 20);
        pedal(&engine, cc, PEDAL_DOWN);
        note(&engine, 0);
        assert(energy(&engine, 200) > loud * 0.5);
        pedal(&engine, cc, 0);
        assert(energy(&engine, 200) < loud * 0.001);
    }
    note(&engine, TEST_VELOCITY);
    pedal(&engine, SUSTAIN_CC, PEDAL_DOWN);
    energy(&engine, 20);
    engine_panic(&engine);
    assert(energy(&engine, 200) == 0);
    engine_close(&engine);
    puts("Engine: audio bounds, velocity, zero-velocity note-off, pedals and panic passed.");
    return 0;
}
