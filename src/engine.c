#include "engine.h"

#include <stdio.h>

static int soft_pedal(fluid_synth_t *synth)
{
    enum { SOFT_CC = 67, SOFT_ATTENUATION_CB = 60, SOFT_CUTOFF_CENTS = -1200 };
    fluid_mod_t *mod = new_fluid_mod();
    if (!mod) return FLUID_FAILED;
    fluid_mod_set_source1(mod, SOFT_CC, FLUID_MOD_CC | FLUID_MOD_LINEAR);
    fluid_mod_set_source2(mod, FLUID_MOD_NONE, FLUID_MOD_GC);
    fluid_mod_set_dest(mod, GEN_ATTENUATION);
    fluid_mod_set_amount(mod, SOFT_ATTENUATION_CB);
    int result = fluid_synth_add_default_mod(synth, mod, FLUID_SYNTH_ADD);
    if (result == FLUID_OK) {
        fluid_mod_set_dest(mod, GEN_FILTERFC);
        fluid_mod_set_amount(mod, SOFT_CUTOFF_CENTS);
        result = fluid_synth_add_default_mod(synth, mod, FLUID_SYNTH_ADD);
    }
    delete_fluid_mod(mod);
    return result;
}

int engine_open(Engine *e, const Options *o)
{
    *e = (Engine){0};
    e->settings = new_fluid_settings();
    if (!e->settings) return -1;
    if (fluid_settings_setnum(e->settings, "synth.sample-rate", o->sample_rate) != FLUID_OK ||
        fluid_settings_setnum(e->settings, "synth.gain", o->gain) != FLUID_OK ||
        fluid_settings_setint(e->settings, "synth.polyphony", o->polyphony) != FLUID_OK ||
        fluid_settings_setint(e->settings, "synth.reverb.active", o->effects) != FLUID_OK ||
        fluid_settings_setint(e->settings, "synth.chorus.active", o->effects) != FLUID_OK ||
        fluid_settings_setint(e->settings, "synth.threadsafe-api", 0) != FLUID_OK ||
        fluid_settings_setint(e->settings, "synth.cpu-cores", 1) != FLUID_OK)
        goto fail;
    e->synth = new_fluid_synth(e->settings);
    if (!e->synth) goto fail;
    if (soft_pedal(e->synth) != FLUID_OK) goto fail;
    e->soundfont_id = fluid_synth_sfload(e->synth, o->soundfont, 0);
    if (e->soundfont_id == FLUID_FAILED) goto fail;
    if (o->presets) return 0;
    for (int ch = 0; ch < M1_CHANNELS; ++ch) {
        if (fluid_synth_program_select(e->synth, ch, e->soundfont_id,
                                      o->bank, o->program) == FLUID_FAILED) {
            fprintf(stderr, "No preset at bank %d, program %d; use --presets.\n",
                    o->bank, o->program);
            goto fail;
        }
    }
    return 0;
fail:
    engine_close(e);
    return -1;
}

void engine_close(Engine *e)
{
    if (e->synth) delete_fluid_synth(e->synth);
    if (e->settings) delete_fluid_settings(e->settings);
    *e = (Engine){0};
}

void engine_presets(Engine *e)
{
    fluid_sfont_t *font = fluid_synth_get_sfont_by_id(e->synth, e->soundfont_id);
    fluid_sfont_iteration_start(font);
    fluid_preset_t *preset;
    while ((preset = fluid_sfont_iteration_next(font)))
        printf("bank=%d program=%d  %s\n", fluid_preset_get_banknum(preset),
               fluid_preset_get_num(preset), fluid_preset_get_name(preset));
}

void engine_event(Engine *e, Event v)
{
    switch (v.type) {
    case NOTE_ON:
        if (v.value) fluid_synth_noteon(e->synth, v.channel, v.key, v.value);
        else fluid_synth_noteoff(e->synth, v.channel, v.key);
        break;
    case NOTE_OFF: fluid_synth_noteoff(e->synth, v.channel, v.key); break;
    case CONTROL: fluid_synth_cc(e->synth, v.channel, v.key, v.value); break;
    case PITCH_BEND: fluid_synth_pitch_bend(e->synth, v.channel, v.value); break;
    case PRESSURE: fluid_synth_channel_pressure(e->synth, v.channel, v.value); break;
    case KEY_PRESSURE: fluid_synth_key_pressure(e->synth, v.channel, v.key, v.value); break;
    }
}

void engine_panic(Engine *e)
{
    enum { SUSTAIN = 64, SOSTENUTO = 66 };
    for (int ch = 0; ch < M1_CHANNELS; ++ch) {
        fluid_synth_cc(e->synth, ch, SUSTAIN, 0);
        fluid_synth_cc(e->synth, ch, SOSTENUTO, 0);
        fluid_synth_all_sounds_off(e->synth, ch);
    }
}

int engine_render(Engine *e, float *stereo, int frames)
{
    int result = fluid_synth_write_float(e->synth, frames, stereo, 0, M1_STEREO,
                                       stereo, 1, M1_STEREO);
    for (int i = 0; i < frames * M1_STEREO; ++i) {
        if (stereo[i] > 1.0f) stereo[i] = 1.0f;
        else if (stereo[i] < -1.0f) stereo[i] = -1.0f;
    }
    return result;
}
