#include "types.h"

TR_MODULE(Name="speaker", Width=100, Height=100)
struct tr_speaker
{
    TR_FIELD(X=50, Y=60)
    tr_input in_audio;
};

TR_MODULE(Name="scope", Width=200, Height=220)
struct tr_scope
{
    TR_FIELD(X=20, Y=210)
    tr_input in_0;
};

TR_MODULE(Name="vco", Width=100, Height=160)
struct tr_vco
{
    float phase;

    TR_FIELD(X=24, Y=50, Min=20.0, Max=1000.0)
    float in_v0;

    TR_FIELD(X=20, Y=80)
    tr_input in_voct;

    TR_FIELD(X=70, Y=50)
    tr_buf out_sin;

    TR_FIELD(X=70, Y=85)
    tr_buf out_sqr;
    
    TR_FIELD(X=70, Y=120)
    tr_buf out_saw;
};

TR_MODULE(Name="clock", Width=100, Height=100)
struct tr_clock
{
    float phase;

    TR_FIELD(X=24, Y=50, Min=0.01, Max=50.0)
    float in_hz;

    TR_FIELD(X=70, Y=50)
    tr_buf out_gate;
};

TR_MODULE(Name="vca", Width=200, Height=100)
struct tr_vca
{
    TR_FIELD(X=24, Y=50)
    tr_input in_audio;

    TR_FIELD(X=54, Y=50)
    tr_input in_cv;

    TR_FIELD(X=84, Y=50)
    tr_buf out_mix;
};

TR_MODULE(Name="lp", Width=250, Height=100)
struct tr_lp
{
    float value;
    float z;

    TR_FIELD(X=110, Y=50)
    tr_input in_audio;

    TR_FIELD(X=150, Y=50)
    tr_input in_cut;

    TR_FIELD(X=24, Y=50, Min=0.0, Max=1.0)
    float in_cut0;

    TR_FIELD(X=64, Y=50, Min=0.0, Max=2.0)
    float in_cut_mul;

    TR_FIELD(X=190, Y=50)
    tr_buf out_audio;
};

TR_MODULE(Name="mixer", Width=250, Height=110)
struct tr_mixer
{
    TR_FIELD(X=24, Y=85)
    tr_input in_0;

    TR_FIELD(X=64, Y=85)
    tr_input in_1;

    TR_FIELD(X=104, Y=85)
    tr_input in_2;

    TR_FIELD(X=144, Y=85)
    tr_input in_3;

    TR_FIELD(X=24, Y=50, Max=1.0)
    float in_vol0;

    TR_FIELD(X=64, Y=50, Max=1.0)
    float in_vol1;
    
    TR_FIELD(X=104, Y=50, Max=1.0)
    float in_vol2;

    TR_FIELD(X=144, Y=50, Max=1.0)
    float in_vol3;

    TR_FIELD(X=184, Y=50, Min=0.0, Max=1.0, Default=1.0)
    float in_vol_final;

    TR_FIELD(X=184, Y=85)
    tr_buf out_mix;
};

TR_MODULE(Name="noise", Width=100, Height=120)
struct tr_noise
{
    int rng;
    float red_state;

    TR_FIELD(X=50, Y=50)
    tr_buf out_white;

    TR_FIELD(X=50, Y=85)
    tr_buf out_red;
};

TR_MODULE(Name="clockdiv", Width=400, Height=100)
struct tr_clockdiv
{
    int gate;
    int state;

    TR_FIELD(X=50, Y=50)
    tr_input in_gate;

    TR_FIELD(X=90, Y=50)
    tr_buf out_0;

    TR_FIELD(X=130, Y=50)
    tr_buf out_1;

    TR_FIELD(X=170, Y=50)
    tr_buf out_2;

    TR_FIELD(X=210, Y=50)
    tr_buf out_3;

    TR_FIELD(X=250, Y=50)
    tr_buf out_4;

    TR_FIELD(X=290, Y=50)
    tr_buf out_5;

    TR_FIELD(X=330, Y=50)
    tr_buf out_6;
    
    TR_FIELD(X=370, Y=50)
    tr_buf out_7;
};

TR_MODULE(Name="seq8", Width=400, Height=100)
struct tr_seq8
{
    int step;
    int trig;

    TR_FIELD(X=20, Y=50)
    tr_input in_step;

    TR_FIELD(X=50, Y=50, Max=1.0)
    float in_cv_0;

    TR_FIELD(X=90, Y=50, Max=1.0)
    float in_cv_1;

    TR_FIELD(X=130, Y=50, Max=1.0)
    float in_cv_2;

    TR_FIELD(X=170, Y=50, Max=1.0)
    float in_cv_3;

    TR_FIELD(X=210, Y=50, Max=1.0)
    float in_cv_4;

    TR_FIELD(X=250, Y=50, Max=1.0)
    float in_cv_5;

    TR_FIELD(X=290, Y=50, Max=1.0)
    float in_cv_6;
    
    TR_FIELD(X=330, Y=50, Max=1.0)
    float in_cv_7;

    TR_FIELD(X=370, Y=50)
    tr_buf out_cv;
};

TR_MODULE(Name="adsr", Width=200, Height=100)
struct tr_adsr
{
    float value;
    int gate;
    int state;

    TR_FIELD(X=24, Y=50, Min=0.001, Max=1.0, Default=0.001)
    float in_attack;

    TR_FIELD(X=64, Y=50, Min=0.001, Max=1.0, Default=0.001)
    float in_decay;
    
    TR_FIELD(X=104, Y=50, Min=0.0, Max=1.0, Default=0.0)
    float in_sustain;

    TR_FIELD(X=144, Y=50, Min=0.001, Max=1.0, Default=0.001)
    float in_release;
    
    TR_FIELD(X=24, Y=80)
    tr_input in_gate;

    TR_FIELD(X=60, Y=80)
    tr_buf out_env;
};

TR_MODULE(Name="random", Width=100, Height=100)
struct tr_random
{
    float t0;
    float t1;
    float t2;

    TR_FIELD(X=24, Y=50, Max=10.0)
    float in_speed;

    TR_FIELD(X=80, Y=50)
    tr_buf out_cv;
};

enum tr_quantizer_mode
{
    TR_QUANTIZER_CHROMATIC,
    TR_QUANTIZER_MINOR,
    TR_QUANTIZER_MINOR_TRIAD,
    TR_QUANTIZER_MAJOR,
    TR_QUANTIZER_MAJOR_TRIAD,
    TR_QUANTIZER_PENTATONIC,
    TR_QUANTIZER_BLUES,
};

TR_MODULE(Name="quantizer", Width=190, Height=110)
struct tr_quantizer
{
    TR_FIELD(X=24, Y=50)
    enum tr_quantizer_mode in_mode;

    TR_FIELD(X=24, Y=86)
    tr_input in_cv;

    TR_FIELD(X=166, Y=86)
    tr_buf out_cv;
};