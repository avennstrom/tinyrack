#include "wav.h"

#include <string.h>

typedef struct
{
    char typeId[4];
} riff_header_t;

typedef struct 
{
    uint16_t formatTag;
    uint16_t channelCount;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} fmt_header_t;

static void wav_write_chunk_header(FILE* f, const char* id, uint32_t size)
{
    fwrite(id, 1, 4, f);
    fwrite(&size, 1, sizeof(size), f);
}

void write_wav_file(FILE* f, const int16_t* samples, size_t sample_count, uint32_t sample_rate)
{
    riff_header_t riff_header;
    memcpy(riff_header.typeId, "WAVE", 4);

    const uint16_t channelCount = 1;
    
    fmt_header_t fmtHeader;
    fmtHeader.formatTag = 1;
    fmtHeader.channelCount = channelCount;
    fmtHeader.sampleRate = sample_rate;
    fmtHeader.byteRate = channelCount * sample_rate * sizeof(int16_t);
    fmtHeader.blockAlign = channelCount * sizeof(int16_t);
    fmtHeader.bitsPerSample = sizeof(int16_t) * 8;

    const uint32_t dataSize = channelCount * (uint32_t)sample_count * sizeof(uint16_t);

    wav_write_chunk_header(f, "RIFF", 36 + dataSize);
    fwrite(&riff_header, 1, sizeof(riff_header), f);

    wav_write_chunk_header(f, "fmt ", 16);
    fwrite(&fmtHeader, 1, sizeof(fmtHeader), f);

    wav_write_chunk_header(f, "data", dataSize);
    fwrite(samples, 1, dataSize, f);
}