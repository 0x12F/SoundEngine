#include "render.h"

#include <sndfile.h>
#include <stdio.h>

int render_demo(Engine *engine, const Options *o)
{
    enum { BLOCK_FRAMES = 256, HOLD_SECONDS = 2, TOTAL_SECONDS = 5,
           TEST_VELOCITY = 100 };
    const int chord[] = {60, 64, 67};
    SF_INFO info = { .samplerate = o->sample_rate, .channels = M1_STEREO,
                    .format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 };
    SNDFILE *file = sf_open(o->render, SFM_WRITE, &info);
    if (!file) { fprintf(stderr, "Cannot write WAV: %s\n", sf_strerror(NULL)); return -1; }
    for (size_t n = 0; n < sizeof chord / sizeof chord[0]; ++n)
        engine_event(engine, (Event){NOTE_ON, 0, chord[n], TEST_VELOCITY});
    float samples[BLOCK_FRAMES * M1_STEREO];
    int total = TOTAL_SECONDS * o->sample_rate;
    int release = HOLD_SECONDS * o->sample_rate;
    int result = -1;
    for (int frame = 0; frame < total;) {
        if (frame == release)
            for (size_t n = 0; n < sizeof chord / sizeof chord[0]; ++n)
                engine_event(engine, (Event){NOTE_OFF, 0, chord[n], 0});
        int count = total - frame;
        if (count > BLOCK_FRAMES) count = BLOCK_FRAMES;
        if (frame < release && frame + count > release) count = release - frame;
        if (engine_render(engine, samples, count) != FLUID_OK ||
            sf_writef_float(file, samples, count) != count) goto done;
        frame += count;
    }
    result = 0;
done:
    if (sf_close(file)) result = -1;
    if (result) fprintf(stderr, "WAV rendering failed.\n");
    else printf("Rendered %s (%d Hz stereo, %d seconds).\n",
                o->render, o->sample_rate, TOTAL_SECONDS);
    return result;
}
