#pragma once

#include "modules.generated.h"

#include <stdint.h>

void tr_vco_update(tr_vco_t* vco);
void tr_clock_update(tr_clock_t* clock);
void tr_vca_update(tr_vca_t* vca);
void tr_clockdiv_update(tr_clockdiv_t* clockdiv);
void tr_seq8_update(tr_seq8_t* seq);
void tr_adsr_update(tr_adsr_t* adsr);
void tr_lp_update(tr_lp_t* lp);
void tr_mixer_update(tr_mixer_t* mixer);
void tr_noise_update(tr_noise_t* noise);
void tr_random_update(tr_random_t* random);
void tr_quantizer_update(tr_quantizer_t* quantizer);
