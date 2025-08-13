#pragma once

#include "modules.h"

#include <stddef.h>

enum tr_module_field_type
{
    TR_MODULE_FIELD_INPUT_FLOAT,
    TR_MODULE_FIELD_INPUT_INT,
    TR_MODULE_FIELD_INPUT_BUFFER,
    TR_MODULE_FIELD_BUFFER,
};

typedef struct tr_module_field_info
{
    enum tr_module_field_type type;
    uintptr_t offset;
    const char* name;
} tr_module_field_info_t;

static const tr_module_field_info_t tr_vco__fields[] = {
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_vco_t, in_v0), "in_v0"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vco_t, in_voct), "in_voct"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_sin), "out_sin"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_sqr), "out_sqr"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_saw), "out_saw"},
};

static const tr_module_field_info_t tr_clock__fields[] = {
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_clock_t, in_hz), "in_hz"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_clock_t, out_gate), "out_gate"},
};

static const tr_module_field_info_t tr_seq8__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_seq8_t, in_step), "in_step"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_0), "in_cv_0"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_1), "in_cv_1"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_2), "in_cv_2"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_3), "in_cv_3"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_4), "in_cv_4"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_5), "in_cv_5"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_6), "in_cv_6"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_7), "in_cv_7"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_seq8_t, out_cv), "out_cv"},
};

static const tr_module_field_info_t tr_adsr__fields[] = {
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_attack), "in_attack"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_decay), "in_decay"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_sustain), "in_sustain"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_release), "in_release"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_adsr_t, in_gate), "in_gate"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_adsr_t, out_env), "out_env"},
};

static const tr_module_field_info_t tr_vca__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vca_t, in_audio), "in_audio"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vca_t, in_cv), "in_cv"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_vca_t, out_mix), "out_mix"},
};

static const tr_module_field_info_t tr_lp__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_lp_t, in_audio), "in_audio"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_lp_t, in_cut), "in_cut"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_lp_t, in_cut0), "in_cut0"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_lp_t, in_cut_mul), "in_cut_mul"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_lp_t, out_audio), "out_audio"},
};

static const tr_module_field_info_t tr_mixer__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_0), "in_0"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_1), "in_1"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_2), "in_2"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_3), "in_3"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol0), "in_vol0"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol1), "in_vol1"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol2), "in_vol2"},
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol3), "in_vol3"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_mixer_t, out_mix), "out_mix"},
};

static const tr_module_field_info_t tr_noise__fields[] = {
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_noise_t, out_white), "out_white"},
};

static const tr_module_field_info_t tr_quantizer__fields[] = {
    {TR_MODULE_FIELD_INPUT_INT, offsetof(tr_quantizer_t, in_mode), "in_mode"},
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_quantizer_t, in_cv), "in_cv"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_quantizer_t, out_cv), "out_cv"},
};

static const tr_module_field_info_t tr_random__fields[] = {
    {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_random_t, in_speed), "in_speed"},
    {TR_MODULE_FIELD_BUFFER, offsetof(tr_random_t, out_cv), "out_cv"},
};

static const tr_module_field_info_t tr_speaker__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_speaker_t, in_audio), "in_audio"},
};

static const tr_module_field_info_t tr_scope__fields[] = {
    {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_scope_t, in_0), "in_0"},
};

typedef struct tr_module_info
{
    const char* id;
    size_t struct_size;
    const tr_module_field_info_t* fields;
    size_t field_count;
} tr_module_info_t;

static const tr_module_info_t tr_module_infos[TR_MODULE_COUNT] = {
    [TR_VCO] = {"vco", sizeof(tr_vco_t), tr_vco__fields, tr_countof(tr_vco__fields)},
    [TR_CLOCK] = {"clock", sizeof(tr_clock_t), tr_clock__fields, tr_countof(tr_clock__fields)},
    [TR_SEQ8] = {"seq8", sizeof(tr_seq8_t), tr_seq8__fields, tr_countof(tr_seq8__fields)},
    [TR_ADSR] = {"adsr", sizeof(tr_adsr_t), tr_adsr__fields, tr_countof(tr_adsr__fields)},
    [TR_VCA] = {"vca", sizeof(tr_vca_t), tr_vca__fields, tr_countof(tr_vca__fields)},
    [TR_LP] = {"lp", sizeof(tr_lp_t), tr_lp__fields, tr_countof(tr_lp__fields)},
    [TR_MIXER] = {"mixer", sizeof(tr_mixer_t), tr_mixer__fields, tr_countof(tr_mixer__fields)},
    [TR_NOISE] = {"noise", sizeof(tr_noise_t), tr_noise__fields, tr_countof(tr_noise__fields)},
    [TR_QUANTIZER] = {"quantizer", sizeof(tr_quantizer_t), tr_quantizer__fields, tr_countof(tr_quantizer__fields)},
    [TR_RANDOM] = {"random", sizeof(tr_random_t), tr_random__fields, tr_countof(tr_random__fields)},
    [TR_SPEAKER] = {"speaker", sizeof(tr_speaker_t), tr_speaker__fields, tr_countof(tr_speaker__fields)},
    [TR_SCOPE] = {"scope", sizeof(tr_scope_t), tr_scope__fields, tr_countof(tr_scope__fields)},
};