
#include <iostream>
#include <vector>
#include <windows.h>
#include <dsound.h>

#include <stdint.h>

#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")

#include "gfxm.h"

inline size_t FixIndexOverflow(size_t index, size_t size)
{
    return index % size;
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

class AudioBuffer
{
public:
    enum RETCODE
    {
        RET_OK,
        RET_INVALID_PARAMETERS,
        RET_INVALID_BIT_PER_SAMPLE_VALUE
    };

    AudioBuffer(int sampleRate, int bitPerSample, int nChannels)
    : data(0), bytes(0), sampleRate(sampleRate), bitPerSample(bitPerSample), nChannels(nChannels)
    {
        
    }
    ~AudioBuffer()
    {
        if(data)
            free(data);
    }
    int Upload(void* data, size_t len, int srcSampleRate, int srcBitPerSample, int srcNChannels)
    {
        this->bytes = len;
        this->data = malloc(len);
        memcpy(this->data, data, len);
        sampleRate = srcSampleRate;
        bitPerSample = srcBitPerSample;
        nChannels = srcNChannels;
        /*
        this->bytes = len;
        
        if(srcBitPerSample != 8 &&
            srcBitPerSample != 16 &&
            srcBitPerSample != 24)
        {
            return RET_INVALID_BIT_PER_SAMPLE_VALUE;
        }
        if(srcNChannels > 2 || srcNChannels < 1)
        {
            return RET_INVALID_PARAMETERS;
        }
        
        int srcBytePerSample = srcBitPerSample / 8;
        int destBytePerSample = bitPerSample / 8;
        int srcSampleCount = len / srcBytePerSample;
        
        this->data = malloc(srcSampleCount * (bitPerSample / 8) * nChannels);
        char* t_data = (char*)(this->data);
        char* t_src = (char*)data;
        for(unsigned i = 0; i < srcSampleCount; ++i)
        {
            ConvertSample(
                &t_src[i * srcBytePerSample],
                &t_data[i * destBytePerSample],
                srcBytePerSample,
                destBytePerSample
            ); 
        }
        */
        return RET_OK;
    }
    
    void ConvertSample(char* src, char* dest, int fromBps, int toBps)
    {
        for(int i = 0; i < fromBps && i < toBps; ++i)
        {
            dest[toBps - i - 1] = src[fromBps - i - 1];
        }
    }
    
    size_t Size() { return bytes; }
    /*
    void Mix(char* data,
        size_t tgtBytes,
        int tgtSampleRate,
        int tgtBitPerSample,
        int tgtNChannels,
        size_t cursor,
        float cursorFraction)
    {
        int tgtBytePerSample = tgtBitPerSample / 8;
        int bytePerSample = bitPerSample / 8;
        char* buf = (char*)this->data;
        size_t bufSize = this->bytes;
        size_t iCursor = cursor;
        int szClip1 = (bufSize - iCursor > tgtBytes) ? (tgtBytes) : (bufSize - iCursor);
        int szClip2 = (szClip1 < tgtBytes) ? (tgtBytes - szClip1) : 0;
        char* clip1 = (char*)buf + iCursor;
        char* clip2 = (char*)buf;
        int tgtSamplePerChannelCount = tgtBytes / tgtBytePerSample;
        
        for(size_t sampleIndex = 0; sampleIndex < tgtSamplePerChannelCount; sampleIndex += tgtNChannels)
        {
            for(size_t chan = 0; chan < tgtNChannels; ++chan)
            {
                char* pSampleA = 
                    clip1 + sampleIndex * (bytePerSample + chan);
                char* pSampleB = 
                    clip1 + sampleIndex * (bytePerSample + chan) + bytePerSample * nChannels;
                int sampleA = DataToSample32(
                    pSampleA, 
                    bitPerSample
                );
                int sampleB = DataToSample32(
                    pSampleB, 
                    bitPerSample
                );
                int sampleInterpolated = gfxm::lerp(sampleA, sampleB, cursorFraction);
                ConvertSample(
                    data + (sampleIndex + chan) * tgtBytePerSample, 
                    (char*)&sampleInterpolated,
                    tgtBytePerSample,
                    bytePerSample
                );
            }
        }
    }
    
    int DataToSample32(char* data, int bitPerSample)
    {
        int sample = 0;
        char* pSample = (char*)&sample;
        int bytePerSample = bitPerSample / 8;
        for(size_t b = 0; b < bytePerSample; ++b)
        {
            pSample[b] = data[b];
        }
        return sample;
    }
    
    void Sample32ToData(char* data, int sample, int bitPerSample)
    {
        char* pSample = (char*)&sample;
        int bytePerSample = bitPerSample / 8;
        for(size_t b = 0; b < bytePerSample; ++b)
        {
            data[b] = pSample[b];
        }
    }
    */
    void CopyData(void* data, size_t bytes, size_t cursor, gfxm::vec3 leftEarPos, gfxm::vec3 rightEarPos, gfxm::vec3 sourcePos)
    {
        short* clip = (short*)data;
        //int szAudio1 = this->bytes - (size_t)cursor > bytes ? bytes : this->bytes - (size_t)cursor;
        //int szAudio2 = szAudio1 < bytes ? bytes - szAudio1 : 0;
        
        size_t szAudio1 = this->bytes - (size_t)cursor;
        if(szAudio1 > bytes) szAudio1 = bytes;
        size_t szAudio2 = bytes - szAudio1;
        
        char* clip1 = (char*)this->data + (size_t)cursor;
        char* clip2 = (char*)this->data;
        
        float leftDist = gfxm::length(leftEarPos - sourcePos);
        float rightDist = gfxm::length(rightEarPos - sourcePos);
        float leftVol = 1.0f - leftDist;
        if(leftVol < 0.0f) leftVol = 0.0f;
        float rightVol = 1.0f - rightDist;
        if(rightVol < 0.0f) rightVol = 0.0f;
        
        CopyClip(clip1, szAudio1, nChannels, clip, 2, cursor);
        CopyClip(clip2, szAudio2, nChannels, clip + (szAudio1 / 2), 2, cursor);
    }
    
    void CopyClip(char* clip, size_t szClip, int srcNChannels, short* tgtClip, int tgtNChannels, size_t cursor)
    {
        for(unsigned i = 0; i < szClip / 2 ; i += 2)
        {
            int sample = 0;
            for(int srcChan = 0; srcChan < srcNChannels; ++srcChan)
            {
                sample += ((short*)clip)[i + srcChan];
            }
            sample /= srcNChannels;
            for(int tgtChan = 0; tgtChan < tgtNChannels; ++tgtChan)
            {
                tgtClip[i + tgtChan] += (short)sample;
            }
        }
    }
    
    int SampleRate() { return sampleRate; }
private:
    void* data;
    size_t bytes;
    int sampleRate;
    int bitPerSample;
    int nChannels;
};

struct AudioEmitter
{
    AudioEmitter()
    : buffer(0),
    cursor(0),
    stopped(true)
    {}
    
    AudioBuffer* buffer = 0;
    size_t cursor = 0;
    gfxm::vec3 position;
    bool looping;
    bool stopped;
    
    void Play(bool loop = false)
    {
        looping = loop;
        stopped = false;
        cursor = 0;
    }
    
    bool IsStopped() { return stopped; }
    bool IsLooped() { return looping; }
    
    void Mix(char* data,
        size_t tgtBytes,
        int tgtSampleRate,
        int tgtBitPerSample,
        int tgtNChannels, 
        gfxm::vec3& leftEarPos, 
        gfxm::vec3& rightEarPos)
    {
        if(!buffer)
            return;
        if(stopped)
            return;
        //std::cout << "Cursor: " << cursor << std::endl;
        /*
        buffer->Mix(
            data,
            tgtBytes,
            tgtSampleRate,
            tgtBitPerSample,
            tgtNChannels,
            cursor,
            0.0f
        );*/
        buffer->CopyData(
            data, 
            tgtBytes, 
            cursor,
            leftEarPos,
            rightEarPos,
            position
        );
    }
    
    void Advance(int adv)
    {
        if(!buffer)
            return;
        size_t bufSize = buffer->Size();
        cursor += adv;
        if(cursor >= bufSize)
            cursor = cursor - bufSize;
    }
};

class AudioMixer3D
{
public:
    int Init(int sampleRate, int bitPerSample);
    void Cleanup();
    
    void EarDistance(float meters) { earDistance = meters; }
    float EarDistance() { return earDistance; }
    
    UINT64 prevSamplesPlayed = 0;
    size_t prevCursor = 0;
    void Update()
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState(&state);
        size_t cursor = state.SamplesPlayed * (bitPerSample / 8) * nChannels;
        size_t cursorDelta = cursor - prevCursor;
        size_t cursorBuffer = cursor % sizeof(buffer);
        if(cursor == prevCursor)
        {
            return;
        }
        
        char tempBuffer[22050];
        ZeroMemory(tempBuffer, sizeof(tempBuffer));
        for(AudioEmitter* obj : objects)
        {
            obj->Mix(
                (char*)tempBuffer, 
                sizeof(tempBuffer),
                sampleRate,
                bitPerSample,
                nChannels, 
                gfxm::vec3(-0.1075f, 0.0f, 0.0f),
                gfxm::vec3(0.1075f, 0.0f, 0.0f)
            );
            obj->Advance(cursorDelta);
        }
        
        char* buf1 = (char*)buffer + cursorBuffer;
        char* buf2 = (char*)buffer;
        
        size_t szBuf1 = sizeof(buffer) - cursorBuffer;
        if(szBuf1 > sizeof(tempBuffer)) szBuf1 = sizeof(tempBuffer);
        size_t szBuf2 = sizeof(tempBuffer) - szBuf1;
        
        memcpy(buf1, tempBuffer, szBuf1);
        memcpy(buf2, tempBuffer + szBuf1, szBuf2);
        
        prevCursor = cursor;
    }
    
    AudioBuffer* CreateBuffer()
    {
        AudioBuffer* buf = new AudioBuffer(sampleRate, bitPerSample, nChannels);
        buffers.push_back(buf);
        return buf;
    }
    void DestroyBuffer(AudioBuffer* buf)
    {
        for(unsigned i = 0; i < buffers.size(); ++i)
            if(buffers[i] == buf)
            {
                delete buf;
                buffers.erase(buffers.begin() + i);
            }
    }
    
    AudioEmitter* CreateInstance()
    {
        AudioEmitter* obj = new AudioEmitter();
        objects.push_back(obj);
        return obj;
    }
    void DestroyObject(AudioEmitter* obj)
    {
        for(unsigned i = 0; i < objects.size(); ++i)
            if(objects[i] == obj)
            {
                delete obj;
                objects.erase(objects.begin() + i);
            }
    }
    
private:
    IXAudio2SourceVoice* pSourceVoice;

    int sampleRate;
    int bitPerSample;
    int nChannels;

    float earDistance = 0.215f;
    std::vector<AudioBuffer*> buffers;
    std::vector<AudioEmitter*> objects;

    short buffer[88200];
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    LPDIRECTSOUNDBUFFER SecondaryBuffer;
    DWORD prevWritePos = 0;
    
    HWND hWnd;
};

int AudioMixer3D::Init(int sampleRate, int bitPerSample)
{
    ZeroMemory(buffer, sizeof(buffer));
    this->sampleRate = sampleRate;
    this->bitPerSample = bitPerSample;
    this->nChannels = 2;
    
    int blockAlign = (bitPerSample * nChannels) / 8;
    WAVEFORMATEX wfx = { 
        WAVE_FORMAT_PCM, 
        nChannels, 
        sampleRate, 
        sampleRate * blockAlign, 
        blockAlign, 
        bitPerSample, 
        0 
    };
    
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(FAILED(hr))
    {
        std::cout << "Failed to init COM: " << hr << std::endl;
        return 1;
    }
    
#if(_WIN32_WINNT < 0x602)
#ifdef _DEBUG
    HMODULE xAudioDll = LoadLibraryExW(L"XAudioD2_7.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
    HMODULE xAudioDll = LoadLibraryExW(L"XAudio2_7.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif
    if(!xAudioDll)
    {
        std::cout << "Failed to find XAudio2.7 dll" << std::endl;
        CoUninitialize();
        return 1;
    }
#endif
    
    UINT32 flags = 0;
#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    IXAudio2* pXAudio2;
    hr = XAudio2Create(&pXAudio2, flags);
    if(FAILED(hr))
    {
        std::cout << "Failed to init XAudio2: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }
    
    IXAudio2MasteringVoice* pMasteringVoice = 0;
    if(FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice)))
    {
        std::cout << "Failed to create mastering voice: " << hr << std::endl;
        //pXAudio2.Reset();
        CoUninitialize();
        return 1;
    }
    
    if(FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx, 0, 1.0f, 0)))
    {
        std::cout << "Failed to create source voice: " << hr << std::endl;
        return 1;
    }
        
    pSourceVoice->Start(0, 0);
        
    XAUDIO2_BUFFER buf = { 0 };
    buf.AudioBytes = sizeof(buffer);
    buf.pAudioData = (BYTE*)buffer;
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;
    
    hr = pSourceVoice->SubmitSourceBuffer(&buf);
    
    pMasteringVoice->SetVolume(0.25f);
    
    return 0;
}

void AudioMixer3D::Cleanup()
{
    
}

extern "C"{
#include "stb_vorbis.h"
}

AudioBuffer* LoadAudioBuffer(AudioMixer3D* mixer, const std::string& name)
{
    AudioBuffer* buffer = mixer->CreateBuffer();
    
    int channels = 2;
    int sampleRate = 44100;
    short* decoded;
    int len;
    len = stb_vorbis_decode_filename(name.c_str(), &channels, &sampleRate, &decoded);
    
    buffer->Upload((void*)decoded, len * sizeof(short) * channels, sampleRate, 16, channels);
    free(decoded);
    
    return buffer;
}

int main()
{
    AudioMixer3D mixer;
    mixer.Init(44100, 16);
    
    AudioEmitter* obj = mixer.CreateInstance();
    obj->buffer = LoadAudioBuffer(&mixer, "test24.ogg");
    obj->position = gfxm::vec3(0.2f, 0.2f, -0.65f);
    obj->Play(1);
    obj = mixer.CreateInstance();
    obj->buffer = LoadAudioBuffer(&mixer, "test2.ogg");
    
    
    
    float pos = 0.0f;
    while(1)
    {
        pos += 0.05f;
        obj->position = gfxm::vec3(sinf(pos) * 0.4f, -0.2f, 0.1f);
        mixer.Update();
        Sleep(5);
    }
    return 0;
}