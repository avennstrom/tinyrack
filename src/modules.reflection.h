#pragma once

#include "modules.h"

#include <stddef.h>

enum tr_vco_field
{
    TR_VCO_v0,
    TR_VCO_in_voct,
    TR_VCO_out_sin,
    TR_VCO_out_sqr,
    TR_VCO_out_saw,
    TR_VCO_FIELD_COUNT,
};
enum tr_clock_field
{
    TR_CLOCK_in_hz,
    TR_CLOCK_out_gate,
    TR_CLOCK_FIELD_COUNT,
};
enum tr_seq8_field
{
    TR_SEQ8_in_step,
    TR_SEQ8_in_cv_0,
    TR_SEQ8_in_cv_1,
    TR_SEQ8_in_cv_2,
    TR_SEQ8_in_cv_3,
    TR_SEQ8_in_cv_4,
    TR_SEQ8_in_cv_5,
    TR_SEQ8_in_cv_6,
    TR_SEQ8_in_cv_7,
    TR_SEQ8_out_cv,
    TR_SEQ8_FIELD_COUNT,
};
enum tr_adsr_field
{
    TR_ADSR_in_attack,
    TR_ADSR_in_decay,
    TR_ADSR_in_sustain,
    TR_ADSR_in_release,
    TR_ADSR_in_gate,
    TR_ADSR_out_env,
    TR_ADSR_FIELD_COUNT,
};
enum tr_vca_field
{
    TR_VCA_in_audio,
    TR_VCA_in_cv,
    TR_VCA_out_mix,
    TR_VCA_FIELD_COUNT,
};
enum tr_lp_field
{
    TR_LP_in_audio,
    TR_LP_in_cut,
    TR_LP_in_cut0,
    TR_LP_in_cut_mul,
    TR_LP_out_audio,
    TR_LP_FIELD_COUNT,
};
enum tr_mixer_field
{
    TR_MIXER_in_0,
    TR_MIXER_in_1,
    TR_MIXER_in_2,
    TR_MIXER_in_3,
    TR_MIXER_in_vol0,
    TR_MIXER_in_vol1,
    TR_MIXER_in_vol2,
    TR_MIXER_in_vol3,
    TR_MIXER_in_vol_final,
    TR_MIXER_out_mix,
    TR_MIXER_FIELD_COUNT,
};
enum tr_noise_field
{
    TR_NOISE_out_white,
    TR_NOISE_out_red,
    TR_NOISE_FIELD_COUNT,
};
enum tr_quantizer_field
{
    TR_QUANTIZER_in_mode,
    TR_QUANTIZER_in_cv,
    TR_QUANTIZER_out_cv,
    TR_QUANTIZER_FIELD_COUNT,
};
enum tr_random_field
{
    TR_RANDOM_in_speed,
    TR_RANDOM_out_cv,
    TR_RANDOM_FIELD_COUNT,
};
enum tr_speaker_field
{
    TR_SPEAKER_in_audio,
    TR_SPEAKER_FIELD_COUNT,
};
enum tr_scope_field
{
    TR_SCOPE_in_0,
    TR_SCOPE_FIELD_COUNT,
};

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
    int x;
    int y;
    float min;
    float max;
    float default_value;
    int min_int;
    int max_int;
} tr_module_field_info_t;

static const tr_module_field_info_t tr_vco__fields[TR_VCO_FIELD_COUNT] = {
    [TR_VCO_v0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_vco_t, in_v0), "in_v0", 24, 50, .min = 20.0f, .max = 1000.0f},
    [TR_VCO_in_voct] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vco_t, in_voct), "in_voct", 20, 80},
    [TR_VCO_out_sin] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_sin), "out_sin", 70, 50},
    [TR_VCO_out_sqr] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_sqr), "out_sqr", 70, 85},
    [TR_VCO_out_saw] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_vco_t, out_saw), "out_saw", 70, 120},
};
static const tr_module_field_info_t tr_clock__fields[TR_CLOCK_FIELD_COUNT] = {
    [TR_CLOCK_in_hz] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_clock_t, in_hz), "in_hz", 24, 50, .min = 0.01f, .max = 50.0f},
    [TR_CLOCK_out_gate] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_clock_t, out_gate), "out_gate", 70, 50},
};
#define TR_SEQ8_KNOB_POS(i) (50 + (i * 40)), 50
static const tr_module_field_info_t tr_seq8__fields[TR_SEQ8_FIELD_COUNT] = {
    [TR_SEQ8_in_step] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_seq8_t, in_step), "in_step", 20, 50},
    [TR_SEQ8_in_cv_0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_0), "in_cv_0", TR_SEQ8_KNOB_POS(0), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_1] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_1), "in_cv_1", TR_SEQ8_KNOB_POS(1), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_2] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_2), "in_cv_2", TR_SEQ8_KNOB_POS(2), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_3] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_3), "in_cv_3", TR_SEQ8_KNOB_POS(3), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_4] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_4), "in_cv_4", TR_SEQ8_KNOB_POS(4), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_5] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_5), "in_cv_5", TR_SEQ8_KNOB_POS(5), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_6] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_6), "in_cv_6", TR_SEQ8_KNOB_POS(6), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_in_cv_7] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_seq8_t, in_cv_7), "in_cv_7", TR_SEQ8_KNOB_POS(7), .min = 0.0f, .max = 1.0f},
    [TR_SEQ8_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_seq8_t, out_cv), "out_cv", 380, 50},
};
static const tr_module_field_info_t tr_adsr__fields[TR_ADSR_FIELD_COUNT] = {
    [TR_ADSR_in_attack] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_attack), "in_attack", 24 + 0*40, 50, .min = 0.001f, .max = 1.0f},
    [TR_ADSR_in_decay] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_decay), "in_decay", 24 + 1*40, 50, .min = 0.001f, .max = 1.0f},
    [TR_ADSR_in_sustain] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_sustain), "in_sustain", 24 + 2*40, 50, .min = 0.0f, .max = 1.0f},
    [TR_ADSR_in_release] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_adsr_t, in_release), "in_release", 24 + 3*40, 50, .min = 0.001f, .max = 1.0f},
    [TR_ADSR_in_gate] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_adsr_t, in_gate), "in_gate", 24, 80},
    [TR_ADSR_out_env] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_adsr_t, out_env), "out_env", 60, 80},
};
static const tr_module_field_info_t tr_vca__fields[TR_VCA_FIELD_COUNT] = {
    [TR_VCA_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vca_t, in_audio), "in_audio", 24 + 0*30, 50},
    [TR_VCA_in_cv] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_vca_t, in_cv), "in_cv", 24 + 1*30, 50},
    [TR_VCA_out_mix] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_vca_t, out_mix), "out_mix", 24 + 2*30, 50},
};
static const tr_module_field_info_t tr_lp__fields[TR_LP_FIELD_COUNT] = {
    [TR_LP_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_lp_t, in_audio), "in_audio", 110, 50},
    [TR_LP_in_cut] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_lp_t, in_cut), "in_cut", 150, 50},
    [TR_LP_in_cut0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_lp_t, in_cut0), "in_cut0", 24, 50, .min = 0.0f, .max = 1.0f},
    [TR_LP_in_cut_mul] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_lp_t, in_cut_mul), "in_cut_mul", 24+40, 50, .min = 0.0f, .max = 2.0f},
    [TR_LP_out_audio] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_lp_t, out_audio), "out_audio", 190, 50},
};
static const tr_module_field_info_t tr_mixer__fields[TR_MIXER_FIELD_COUNT] = {
    [TR_MIXER_in_0] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_0), "in_0", 24 + (0*40), 85},
    [TR_MIXER_in_1] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_1), "in_1", 24 + (1*40), 85},
    [TR_MIXER_in_2] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_2), "in_2", 24 + (2*40), 85},
    [TR_MIXER_in_3] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_mixer_t, in_3), "in_3", 24 + (3*40), 85},
    [TR_MIXER_in_vol0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol0), "in_vol0", 24 + (0*40), 50, .min = 0.0f, .max = 1.0f},
    [TR_MIXER_in_vol1] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol1), "in_vol1", 24 + (1*40), 50, .min = 0.0f, .max = 1.0f},
    [TR_MIXER_in_vol2] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol2), "in_vol2", 24 + (2*40), 50, .min = 0.0f, .max = 1.0f},
    [TR_MIXER_in_vol3] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol3), "in_vol3", 24 + (3*40), 50, .min = 0.0f, .max = 1.0f},
    [TR_MIXER_in_vol_final] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_mixer_t, in_vol_final), "in_vol_final", 24 + (4*40), 50, .min = 0.0f, .max = 1.0f, .default_value = 1.0f},
    [TR_MIXER_out_mix] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_mixer_t, out_mix), "out_mix", 24 + (4*40), 85},
};
static const tr_module_field_info_t tr_noise__fields[TR_NOISE_FIELD_COUNT] = {
    [TR_NOISE_out_white] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_noise_t, out_white), "out_white", 50, 50},
    [TR_NOISE_out_red] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_noise_t, out_red), "out_red", 50, 80},
};
static const tr_module_field_info_t tr_quantizer__fields[TR_QUANTIZER_FIELD_COUNT] = {
    [TR_QUANTIZER_in_mode] = {TR_MODULE_FIELD_INPUT_INT, offsetof(tr_quantizer_t, in_mode), "in_mode", 24, 50, .max_int = TR_QUANTIZER_MODE_COUNT - 1},
    [TR_QUANTIZER_in_cv] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_quantizer_t, in_cv), "in_cv", 24, 110-24},
    [TR_QUANTIZER_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_quantizer_t, out_cv), "out_cv", 190-24, 110-24},
};
static const tr_module_field_info_t tr_random__fields[TR_RANDOM_FIELD_COUNT] = {
    [TR_RANDOM_in_speed] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(tr_random_t, in_speed), "in_speed", 24, 50, .min = 0.0f, .max = 10.0f},
    [TR_RANDOM_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(tr_random_t, out_cv), "out_cv", 80, 50},
};
static const tr_module_field_info_t tr_speaker__fields[TR_SPEAKER_FIELD_COUNT] = {
    [TR_SPEAKER_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_speaker_t, in_audio), "in_audio", 50, 60},
};
static const tr_module_field_info_t tr_scope__fields[TR_SCOPE_FIELD_COUNT] = {
    [TR_SCOPE_in_0] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(tr_scope_t, in_0), "in_0", 20, 210},
};

typedef struct tr_module_info
{
    const char* id;
    size_t struct_size;
    const tr_module_field_info_t* fields;
    size_t field_count;
    int width;
    int height;
} tr_module_info_t;

static const tr_module_info_t tr_module_infos[TR_MODULE_COUNT] = {
    [TR_VCO] = {"vco", sizeof(tr_vco_t), tr_vco__fields, tr_countof(tr_vco__fields), 100, 160},
    [TR_CLOCK] = {"clock", sizeof(tr_clock_t), tr_clock__fields, tr_countof(tr_clock__fields), 100, 100},
    [TR_SEQ8] = {"seq8", sizeof(tr_seq8_t), tr_seq8__fields, tr_countof(tr_seq8__fields), 400, 100},
    [TR_ADSR] = {"adsr", sizeof(tr_adsr_t), tr_adsr__fields, tr_countof(tr_adsr__fields), 200, 100},
    [TR_VCA] = {"vca", sizeof(tr_vca_t), tr_vca__fields, tr_countof(tr_vca__fields), 200, 100},
    [TR_LP] = {"lp", sizeof(tr_lp_t), tr_lp__fields, tr_countof(tr_lp__fields), 250, 100},
    [TR_MIXER] = {"mixer", sizeof(tr_mixer_t), tr_mixer__fields, tr_countof(tr_mixer__fields), 250, 110},
    [TR_NOISE] = {"noise", sizeof(tr_noise_t), tr_noise__fields, tr_countof(tr_noise__fields), 100, 100},
    [TR_QUANTIZER] = {"quantizer", sizeof(tr_quantizer_t), tr_quantizer__fields, tr_countof(tr_quantizer__fields), 190, 110},
    [TR_RANDOM] = {"random", sizeof(tr_random_t), tr_random__fields, tr_countof(tr_random__fields), 100, 100},
    [TR_SPEAKER] = {"speaker", sizeof(tr_speaker_t), tr_speaker__fields, tr_countof(tr_speaker__fields), 100, 100},
    [TR_SCOPE] = {"scope", sizeof(tr_scope_t), tr_scope__fields, tr_countof(tr_scope__fields), 200, 220},
};