#include "audio.h"

#include <dsound.h>
#include <assert.h>

typedef struct buffer_create_info_t
{
    uint8_t channel_count;
    uint8_t sample_size;
    uint32_t sample_rate;
    uint32_t sample_count;
} buffer_create_info_t;

typedef struct audio
{
    HANDLE notifyHandles[2];
    buffer_create_info_t buffer_info;
    int16_t* buffer_samples;
} audio_t;

static HRESULT create_basic_buffer(LPDIRECTSOUND8 ds, const buffer_create_info_t* info, LPDIRECTSOUNDBUFFER8* dsb8)
{
    HRESULT hr;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = info->channel_count;
    wfx.nSamplesPerSec = info->sample_rate;
    wfx.nBlockAlign = info->channel_count * info->sample_size;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.wBitsPerSample = info->sample_size * 8;

    DSBUFFERDESC dsbdesc;
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME;
    dsbdesc.dwBufferBytes = info->sample_count * wfx.nBlockAlign * 2; // double buffered
    dsbdesc.lpwfxFormat = &wfx;

    LPDIRECTSOUNDBUFFER dsb = NULL;
    hr = ds->lpVtbl->CreateSoundBuffer(ds, &dsbdesc, &dsb, NULL);
    if (SUCCEEDED(hr))
    {
        hr = dsb->lpVtbl->QueryInterface(dsb, &IID_IDirectSoundBuffer8, (void**)dsb8);
        dsb->lpVtbl->Release(dsb);
    }

    return hr;
}

audio_t* audio_create(uint32_t sample_rate, uint32_t sample_count)
{
    HRESULT hr;

    audio_t* audio = calloc(1, sizeof(audio_t));
    if (audio == NULL)
    {
        return NULL;
    }

    IDirectSound8* ds;
    hr = DirectSoundCreate8(NULL, &ds, NULL);
    assert(!FAILED(hr));

    hr = ds->lpVtbl->SetCooperativeLevel(ds, GetConsoleWindow(), DSSCL_PRIORITY);
    assert(!FAILED(hr));

    const buffer_create_info_t bufferInfo = {
        .channel_count    = 1, // mono
        .sample_size      = sizeof(int16_t),
        .sample_rate      = sample_rate,
        .sample_count     = sample_count,
    };

    const int block_size = bufferInfo.sample_size * bufferInfo.channel_count;

    IDirectSoundBuffer8* buffer;
    hr = create_basic_buffer(ds, &bufferInfo, &buffer);
    assert(!FAILED(hr));

    IDirectSoundNotify8* notify;
    hr = buffer->lpVtbl->QueryInterface(buffer, &IID_IDirectSoundNotify8, (void**)&notify);
    assert(!FAILED(hr));

    audio->notifyHandles[0] = CreateEvent(NULL, FALSE, FALSE, TEXT("BufferStart"));
    audio->notifyHandles[1] = CreateEvent(NULL, FALSE, FALSE, TEXT("BufferMiddle"));

    DSBPOSITIONNOTIFY notifyPositions[2];
    memset(notifyPositions, 0, sizeof(notifyPositions));
    notifyPositions[0].dwOffset         = 0;
    notifyPositions[0].hEventNotify     = audio->notifyHandles[0];
    notifyPositions[1].dwOffset         = bufferInfo.sample_count * block_size;
    notifyPositions[1].hEventNotify     = audio->notifyHandles[1];

    hr = notify->lpVtbl->SetNotificationPositions(notify, _countof(notifyPositions), notifyPositions);
    assert(!FAILED(hr));

    notify->lpVtbl->Release(notify);

    void* bufferMemory;
    DWORD audioBytes;
    hr = buffer->lpVtbl->Lock(buffer, 0, 0, &bufferMemory, &audioBytes, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    assert(!FAILED(hr));
    assert(audioBytes == bufferInfo.sample_count * block_size * 2);

    int16_t* const bufferSamples = (int16_t*)bufferMemory;

    memset(bufferSamples, 0, bufferInfo.sample_count * block_size * 2);
    hr = buffer->lpVtbl->Play(buffer, 0, 0, DSBPLAY_LOOPING);
    assert(!FAILED(hr));

    audio->buffer_info = bufferInfo;
    audio->buffer_samples = bufferSamples;
    return audio;
}

int16_t* audio_fill(audio_t* audio)
{
    const DWORD r = WaitForMultipleObjects(_countof(audio->notifyHandles), audio->notifyHandles, FALSE, 0);
    if (r == WAIT_TIMEOUT)
    {
        return NULL;
    }
    
    const size_t writeOffset = (r == WAIT_OBJECT_0) ? audio->buffer_info.sample_count * audio->buffer_info.channel_count : 0;
    return audio->buffer_samples + writeOffset;
}