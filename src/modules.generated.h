#pragma once
// --------- GENERATED CODE ---------
// --------- DO NOT MODIFY ----------

#include "modules.types.h"
#include "modules2.h"

enum tr_module_type
{
	TR_SPEAKER,
	TR_SCOPE,
	TR_VCO,
	TR_CLOCK,
	TR_VCA,
	TR_LP,
	TR_MIXER,
	TR_NOISE,
	TR_CLOCKDIV,
	TR_SEQ8,
	TR_ADSR,
	TR_RANDOM,
	TR_QUANTIZER,
	TR_MODULE_COUNT
};
enum
{
	TR_SPEAKER_in_audio,
	TR_SPEAKER_FIELD_COUNT
};
static const struct tr_module_field_info tr_speaker__fields[] = {
	[TR_SPEAKER_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_speaker, in_audio), "in_audio", 50, 60, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_speaker tr_speaker_t;
enum
{
	TR_SCOPE_in_0,
	TR_SCOPE_FIELD_COUNT
};
static const struct tr_module_field_info tr_scope__fields[] = {
	[TR_SCOPE_in_0] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_scope, in_0), "in_0", 20, 210, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_scope tr_scope_t;
enum
{
	TR_VCO_phase,
	TR_VCO_in_v0,
	TR_VCO_in_voct,
	TR_VCO_out_sin,
	TR_VCO_out_sqr,
	TR_VCO_out_saw,
	TR_VCO_FIELD_COUNT
};
static const struct tr_module_field_info tr_vco__fields[] = {
	[TR_VCO_phase] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_vco, phase), "phase", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCO_in_v0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_vco, in_v0), "in_v0", 24, 50, 20.000000, 1000.000000, 0.000000, 0, 0},
	[TR_VCO_in_voct] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_vco, in_voct), "in_voct", 20, 80, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCO_out_sin] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_vco, out_sin), "out_sin", 70, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCO_out_sqr] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_vco, out_sqr), "out_sqr", 70, 85, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCO_out_saw] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_vco, out_saw), "out_saw", 70, 120, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_vco tr_vco_t;
enum
{
	TR_CLOCK_phase,
	TR_CLOCK_in_hz,
	TR_CLOCK_out_gate,
	TR_CLOCK_FIELD_COUNT
};
static const struct tr_module_field_info tr_clock__fields[] = {
	[TR_CLOCK_phase] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_clock, phase), "phase", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCK_in_hz] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_clock, in_hz), "in_hz", 24, 50, 0.010000, 50.000000, 0.000000, 0, 0},
	[TR_CLOCK_out_gate] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clock, out_gate), "out_gate", 70, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_clock tr_clock_t;
enum
{
	TR_VCA_in_audio,
	TR_VCA_in_cv,
	TR_VCA_out_mix,
	TR_VCA_FIELD_COUNT
};
static const struct tr_module_field_info tr_vca__fields[] = {
	[TR_VCA_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_vca, in_audio), "in_audio", 24, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCA_in_cv] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_vca, in_cv), "in_cv", 54, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_VCA_out_mix] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_vca, out_mix), "out_mix", 84, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_vca tr_vca_t;
enum
{
	TR_LP_value,
	TR_LP_z,
	TR_LP_in_audio,
	TR_LP_in_cut,
	TR_LP_in_cut0,
	TR_LP_in_cut_mul,
	TR_LP_out_audio,
	TR_LP_FIELD_COUNT
};
static const struct tr_module_field_info tr_lp__fields[] = {
	[TR_LP_value] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_lp, value), "value", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_LP_z] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_lp, z), "z", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_LP_in_audio] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_lp, in_audio), "in_audio", 110, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_LP_in_cut] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_lp, in_cut), "in_cut", 150, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_LP_in_cut0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_lp, in_cut0), "in_cut0", 24, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_LP_in_cut_mul] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_lp, in_cut_mul), "in_cut_mul", 64, 50, 0.000000, 2.000000, 0.000000, 0, 0},
	[TR_LP_out_audio] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_lp, out_audio), "out_audio", 190, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_lp tr_lp_t;
enum
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
	TR_MIXER_FIELD_COUNT
};
static const struct tr_module_field_info tr_mixer__fields[] = {
	[TR_MIXER_in_0] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_mixer, in_0), "in_0", 24, 85, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_MIXER_in_1] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_mixer, in_1), "in_1", 64, 85, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_MIXER_in_2] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_mixer, in_2), "in_2", 104, 85, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_MIXER_in_3] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_mixer, in_3), "in_3", 144, 85, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_MIXER_in_vol0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_mixer, in_vol0), "in_vol0", 24, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_MIXER_in_vol1] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_mixer, in_vol1), "in_vol1", 64, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_MIXER_in_vol2] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_mixer, in_vol2), "in_vol2", 104, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_MIXER_in_vol3] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_mixer, in_vol3), "in_vol3", 144, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_MIXER_in_vol_final] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_mixer, in_vol_final), "in_vol_final", 184, 50, 0.000000, 1.000000, 1.000000, 0, 0},
	[TR_MIXER_out_mix] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_mixer, out_mix), "out_mix", 184, 85, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_mixer tr_mixer_t;
enum
{
	TR_NOISE_rng,
	TR_NOISE_red_state,
	TR_NOISE_out_white,
	TR_NOISE_out_red,
	TR_NOISE_FIELD_COUNT
};
static const struct tr_module_field_info tr_noise__fields[] = {
	[TR_NOISE_rng] = {TR_MODULE_FIELD_INT, offsetof(struct tr_noise, rng), "rng", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_NOISE_red_state] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_noise, red_state), "red_state", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_NOISE_out_white] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_noise, out_white), "out_white", 50, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_NOISE_out_red] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_noise, out_red), "out_red", 50, 85, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_noise tr_noise_t;
enum
{
	TR_CLOCKDIV_gate,
	TR_CLOCKDIV_state,
	TR_CLOCKDIV_in_gate,
	TR_CLOCKDIV_out_0,
	TR_CLOCKDIV_out_1,
	TR_CLOCKDIV_out_2,
	TR_CLOCKDIV_out_3,
	TR_CLOCKDIV_out_4,
	TR_CLOCKDIV_out_5,
	TR_CLOCKDIV_out_6,
	TR_CLOCKDIV_out_7,
	TR_CLOCKDIV_FIELD_COUNT
};
static const struct tr_module_field_info tr_clockdiv__fields[] = {
	[TR_CLOCKDIV_gate] = {TR_MODULE_FIELD_INT, offsetof(struct tr_clockdiv, gate), "gate", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_state] = {TR_MODULE_FIELD_INT, offsetof(struct tr_clockdiv, state), "state", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_in_gate] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_clockdiv, in_gate), "in_gate", 50, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_0] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_0), "out_0", 90, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_1] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_1), "out_1", 130, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_2] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_2), "out_2", 170, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_3] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_3), "out_3", 210, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_4] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_4), "out_4", 250, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_5] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_5), "out_5", 290, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_6] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_6), "out_6", 330, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_CLOCKDIV_out_7] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_clockdiv, out_7), "out_7", 370, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_clockdiv tr_clockdiv_t;
enum
{
	TR_SEQ8_step,
	TR_SEQ8_trig,
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
	TR_SEQ8_FIELD_COUNT
};
static const struct tr_module_field_info tr_seq8__fields[] = {
	[TR_SEQ8_step] = {TR_MODULE_FIELD_INT, offsetof(struct tr_seq8, step), "step", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_SEQ8_trig] = {TR_MODULE_FIELD_INT, offsetof(struct tr_seq8, trig), "trig", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_step] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_seq8, in_step), "in_step", 20, 50, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_0] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_0), "in_cv_0", 50, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_1] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_1), "in_cv_1", 90, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_2] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_2), "in_cv_2", 130, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_3] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_3), "in_cv_3", 170, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_4] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_4), "in_cv_4", 210, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_5] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_5), "in_cv_5", 250, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_6] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_6), "in_cv_6", 290, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_in_cv_7] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_seq8, in_cv_7), "in_cv_7", 330, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_SEQ8_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_seq8, out_cv), "out_cv", 370, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_seq8 tr_seq8_t;
enum
{
	TR_ADSR_value,
	TR_ADSR_gate,
	TR_ADSR_state,
	TR_ADSR_in_attack,
	TR_ADSR_in_decay,
	TR_ADSR_in_sustain,
	TR_ADSR_in_release,
	TR_ADSR_in_gate,
	TR_ADSR_out_env,
	TR_ADSR_FIELD_COUNT
};
static const struct tr_module_field_info tr_adsr__fields[] = {
	[TR_ADSR_value] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_adsr, value), "value", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_ADSR_gate] = {TR_MODULE_FIELD_INT, offsetof(struct tr_adsr, gate), "gate", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_ADSR_state] = {TR_MODULE_FIELD_INT, offsetof(struct tr_adsr, state), "state", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_ADSR_in_attack] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_adsr, in_attack), "in_attack", 24, 50, 0.001000, 1.000000, 0.001000, 0, 0},
	[TR_ADSR_in_decay] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_adsr, in_decay), "in_decay", 64, 50, 0.001000, 1.000000, 0.001000, 0, 0},
	[TR_ADSR_in_sustain] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_adsr, in_sustain), "in_sustain", 104, 50, 0.000000, 1.000000, 0.000000, 0, 0},
	[TR_ADSR_in_release] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_adsr, in_release), "in_release", 144, 50, 0.001000, 1.000000, 0.001000, 0, 0},
	[TR_ADSR_in_gate] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_adsr, in_gate), "in_gate", 24, 80, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_ADSR_out_env] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_adsr, out_env), "out_env", 60, 80, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_adsr tr_adsr_t;
enum
{
	TR_RANDOM_t0,
	TR_RANDOM_t1,
	TR_RANDOM_t2,
	TR_RANDOM_in_speed,
	TR_RANDOM_out_cv,
	TR_RANDOM_FIELD_COUNT
};
static const struct tr_module_field_info tr_random__fields[] = {
	[TR_RANDOM_t0] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_random, t0), "t0", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_RANDOM_t1] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_random, t1), "t1", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_RANDOM_t2] = {TR_MODULE_FIELD_FLOAT, offsetof(struct tr_random, t2), "t2", 0, 0, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_RANDOM_in_speed] = {TR_MODULE_FIELD_INPUT_FLOAT, offsetof(struct tr_random, in_speed), "in_speed", 24, 50, 0.000000, 10.000000, 0.000000, 0, 0},
	[TR_RANDOM_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_random, out_cv), "out_cv", 80, 50, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_random tr_random_t;
enum
{
	TR_QUANTIZER_in_mode,
	TR_QUANTIZER_in_cv,
	TR_QUANTIZER_out_cv,
	TR_QUANTIZER_FIELD_COUNT
};
static const struct tr_module_field_info tr_quantizer__fields[] = {
	[TR_QUANTIZER_in_mode] = {TR_MODULE_FIELD_INPUT_INT, offsetof(struct tr_quantizer, in_mode), "in_mode", 24, 50, 0.000000, 0.000000, 0.000000, 0, 6},
	[TR_QUANTIZER_in_cv] = {TR_MODULE_FIELD_INPUT_BUFFER, offsetof(struct tr_quantizer, in_cv), "in_cv", 24, 86, 0.000000, 0.000000, 0.000000, 0, 0},
	[TR_QUANTIZER_out_cv] = {TR_MODULE_FIELD_BUFFER, offsetof(struct tr_quantizer, out_cv), "out_cv", 166, 86, 0.000000, 0.000000, 0.000000, 0, 0},
};
typedef struct tr_quantizer tr_quantizer_t;
static const struct tr_module_info tr_module_infos[] = {
	[TR_SPEAKER] = {"speaker", sizeof(struct tr_speaker), tr_speaker__fields, 1, 100, 100},
	[TR_SCOPE] = {"scope", sizeof(struct tr_scope), tr_scope__fields, 1, 200, 220},
	[TR_VCO] = {"vco", sizeof(struct tr_vco), tr_vco__fields, 6, 100, 160},
	[TR_CLOCK] = {"clock", sizeof(struct tr_clock), tr_clock__fields, 3, 100, 100},
	[TR_VCA] = {"vca", sizeof(struct tr_vca), tr_vca__fields, 3, 200, 100},
	[TR_LP] = {"lp", sizeof(struct tr_lp), tr_lp__fields, 7, 250, 100},
	[TR_MIXER] = {"mixer", sizeof(struct tr_mixer), tr_mixer__fields, 10, 250, 110},
	[TR_NOISE] = {"noise", sizeof(struct tr_noise), tr_noise__fields, 4, 100, 120},
	[TR_CLOCKDIV] = {"clockdiv", sizeof(struct tr_clockdiv), tr_clockdiv__fields, 11, 400, 100},
	[TR_SEQ8] = {"seq8", sizeof(struct tr_seq8), tr_seq8__fields, 12, 400, 100},
	[TR_ADSR] = {"adsr", sizeof(struct tr_adsr), tr_adsr__fields, 9, 200, 100},
	[TR_RANDOM] = {"random", sizeof(struct tr_random), tr_random__fields, 5, 100, 100},
	[TR_QUANTIZER] = {"quantizer", sizeof(struct tr_quantizer), tr_quantizer__fields, 3, 190, 110},
};