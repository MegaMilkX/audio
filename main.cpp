
#include <iostream>
#include <vector>
#include <windows.h>
#include <dsound.h>

#include <stdint.h>

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
        
        return RET_OK;
    }
    
    void ConvertSample(char* src, char* dest, int fromBps, int toBps)
    {
        for(unsigned i = 0; i < fromBps && i < toBps; ++i)
        {
            dest[toBps - i - 1] = src[fromBps - i - 1];
        }
    }
    
    size_t Size() { return bytes; }
    
    void Mix(char* data,
        size_t tgtBytes,
        int tgtSampleRate,
        int tgtBitPerSample,
        int tgtNChannels,
        float cursor)
    {
        int tgtBytePerSample = tgtBitPerSample / 8;
        int bytePerSample = bitPerSample / 8;
        char* buf = this->data;
        size_t bufSize = this->bytes;
        size_t iCursor = cursor;
        int szClip1 = (bufSize - iCursor > tgtBytes) ? (tgtBytes) : (bufSize - iCursor);
        int szClip2 = (szAudio1 < tgtBytes) ? (tgtBytes - szAudio1) : 0;
        char* clip1 = (char*)buf + iCursor;
        char* clip2 = (char*)buf;
        int tgtSamplePerChannelCount = tgtBytes / tgtBytePerSample;
        
        for(size_t sampleIndex = 0; sampleIndex < tgtSamplePerChannelCount; sampleIndex += tgtNChannels)
        {
            for(size_t chan = 0; chan < tgtNChannels; ++chan)
            {
                char* pSampleA = 
                    clip1 + (bytePerSample * chan);
                char* pSampleB = 
                    clip1 + (bytePerSample * chan) + bytePerSample * nChannels;
                int sampleA = DataToSample32(
                    pSampleA, 
                    bitPerSample
                );
                int sampleB = DataToSample32(
                    pSampleB, 
                    bitPerSample
                );
                int sampleInterpolated = gfxm::lerp(sampleA, sampleB, cursor = cursor - (long)cursor);
                Sample32ToData(data + (sampleIndex + chan) * tgtBytePerSample, sampleInterpolated, tgtBitPerSample);
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
    
    void Sample32ToData(char* data, int sample, bitPerSample)
    {
        char* pSample = (char*)&sample;
        int bytePerSample = bitPerSample / 8;
        for(size_t b = 0; b < bytePerSample; ++b)
        {
            data[b] = pSample[b];
        }
    }
    
    void CopyData(void* data, size_t bytes, int cursor, gfxm::vec3 leftEarPos, gfxm::vec3 rightEarPos, gfxm::vec3 sourcePos)
    {
        short* clip = (short*)data;
        int szAudio1 = this->bytes - cursor > bytes ? bytes : this->bytes - cursor;
        int szAudio2 = szAudio1 < bytes ? bytes - szAudio1 : 0;
        char* clip1 = (char*)this->data + cursor;
        char* clip2 = (char*)this->data;
        
        float leftDist = gfxm::length(leftEarPos - sourcePos);
        float rightDist = gfxm::length(rightEarPos - sourcePos);
        float leftVol = 1.0f - leftDist;
        if(leftVol < 0.0f) leftVol = 0.0f;
        float rightVol = 1.0f - rightDist;
        if(rightVol < 0.0f) rightVol = 0.0f;
        
        for(unsigned i = 0; i < szAudio1 /2 ; i += 2)
        {
            short ls = ((short*)clip1)[i];
            short rs = ((short*)clip1)[i + 1];
            short sample = ((int)ls + rs) / 2;
            clip[i] += sample * leftVol;
            clip[i + 1] += sample * rightVol;
        }
        for(unsigned i = 0; i < szAudio2 /2; i += 2)
        {
            short ls = ((short*)clip2)[i];
            short rs = ((short*)clip2)[i + 1];
            short sample = ((int)ls + rs) / 2;
            clip[szAudio1 / 2 + i] += sample * leftVol;
            clip[szAudio1 / 2 + i + 1] += sample * rightVol;
        }
    }
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
    uint64_t cursor = 0;
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
    
    void Mix(void* dest, size_t destSz, gfxm::vec3& leftEarPos, gfxm::vec3& rightEarPos)
    {
        if(!buffer)
            return;
        if(stopped)
            return;
        //std::cout << "Cursor: " << cursor << std::endl;
        buffer->CopyData(
            dest, 
            destSz, 
            cursor,
            leftEarPos,
            rightEarPos,
            position
        );
    }
    
    void Advance(int adv)
    {
        size_t bufSize = buffer->Size();
        cursor += adv;
        if(cursor >= bufSize)
            cursor = cursor - bufSize;
        
        std::cout << "Cursor: " << cursor << std::endl;
        std::cout << "BufSize: " << bufSize << std::endl;
    }
};

class AudioMixer3D
{
public:
    int Init(int sampleRate, int bitPerSample);
    void Cleanup();
    
    void EarDistance(float meters) { earDistance = meters; }
    float EarDistance() { return earDistance; }
    
    void Update()
    {
        HRESULT hr;
        DWORD writePos = 0;
        hr = SecondaryBuffer->GetCurrentPosition(NULL, &writePos);
        if(FAILED(hr))
        {
            std::cout << "GetCurrentPosition failed" << std::endl;
        }
        
        void* audio1;
        DWORD szAudio1;
        void* audio2;
        DWORD szAudio2;
        hr = SecondaryBuffer->Lock(
            writePos, 44100,
            &audio1, &szAudio1,
            &audio2, &szAudio2,
            0 
        );
        if(FAILED(hr))
        {
            std::cout << "Buffer lock failed: " << std::hex << hr << std::endl;
        }
        
        DWORD dPos;
        if(writePos >= prevWritePos)
            dPos = writePos - prevWritePos;
        else
        {
            dPos = (sampleRate * bitPerSample / 8 - prevWritePos) + writePos;
        }
        
        ZeroMemory(buffer, sizeof(buffer));
        for(AudioEmitter* obj : objects)
        {
            obj->Advance(dPos);
            obj->Mix(
                (void*)buffer, 
                sizeof(buffer), 
                gfxm::vec3(-0.1075f, 0.0f, 0.0f),
                gfxm::vec3(0.1075f, 0.0f, 0.0f)
            );
        }
        prevWritePos = writePos;
        
        ZeroMemory(audio1, szAudio1);
        ZeroMemory(audio2, szAudio2);
        memcpy(audio1, buffer, szAudio1);
        memcpy(audio2, buffer + szAudio1, szAudio2);
        
        SecondaryBuffer->Unlock(
            audio1, szAudio1,
            audio2, szAudio2
        );
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
    this->sampleRate = sampleRate;
    this->bitPerSample = bitPerSample;
    this->nChannels = 2;
    
    HINSTANCE hInstance = GetModuleHandle(0);
  
    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"window";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        return false;
    }

    hWnd = CreateWindowExW(
        0,
        L"window",
        L"hiddenwindow",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hWnd, SW_HIDE);
    //ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    
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
    
    HRESULT hr;
    HMODULE DSoundLib = LoadLibraryA("dsound.dll");
    if(DSoundLib)
    {
        direct_sound_create* DirectSoundCreate = 
            (direct_sound_create*)GetProcAddress(DSoundLib, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && 
            SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            hr = DirectSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
            if(FAILED(hr))
            {
                std::cout << "SetCooperativeLevel failed: " << std::hex << hr << std::endl;
                return 1;
            }
            
            DSBUFFERDESC BuffDesc = { 0 };
            BuffDesc.dwSize = sizeof(BuffDesc);
            BuffDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
            
            hr = DirectSound->CreateSoundBuffer(&BuffDesc, &PrimaryBuffer, 0);
            if(FAILED(hr))
            {
                std::cout << "CreateSoundBuffer failed (primary): " << std::hex << hr << std::endl;
                return 1;
            }
            hr = PrimaryBuffer->SetFormat(&wfx);
            if(FAILED(hr))
            {
                std::cout << "SetFormat failed (primary): " << std::hex << hr << std::endl;
                return 1;
            }
            
            BuffDesc.dwFlags = DSBCAPS_GLOBALFOCUS;
            BuffDesc.dwBufferBytes = sampleRate * bitPerSample / 8;
            BuffDesc.lpwfxFormat = &wfx;
            hr = DirectSound->CreateSoundBuffer(&BuffDesc, &SecondaryBuffer, 0);
            if(FAILED(hr))
            {
                std::cout << "CreateSoundBuffer failed (secondary): " << std::hex << hr << std::endl;
                return 1;
            }
            
            void* audio1;
            DWORD szAudio1;
            void* audio2;
            DWORD szAudio2;
            hr = SecondaryBuffer->Lock(
                0, 0,
                &audio1, &szAudio1,
                &audio2, &szAudio2,
                DSBLOCK_ENTIREBUFFER 
            );
            ZeroMemory(audio1, szAudio1);
            ZeroMemory(audio2, szAudio2);
            SecondaryBuffer->Unlock(
                audio1, szAudio1,
                audio2, szAudio2
            );
            
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
        }
        else
        {
            std::cout << "DirectSoundCreate failed" << std::endl;
            return 1;
        }
    }
    
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
    mixer.Init(48000, 16);
    
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