
#include <iostream>
#include <vector>
#include <windows.h>
#include <dsound.h>

#include "gfxm.h"

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

class AudioBuffer
{
public:
    void Upload(void* data, size_t bytes, int bps, int channels)
    {
        this->data = malloc(bytes);
        this->bytes = bytes;
        this->bps = bps;
        this->channels = channels;
        
        memcpy(this->data, data, bytes);
    }
    
    void CopyData(void* data, size_t bytes, int cursor, gfxm::vec3 leftEarPos, gfxm::vec3 rightEarPos, gfxm::vec3 sourcePos)
    {
        char* clip = (char*)data;
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
            ((short*)clip)[i] += ((short*)clip1)[i] * leftVol;
            ((short*)clip)[i + 1] += ((short*)clip1)[i + 1] * rightVol;
        }
        for(unsigned i = 0; i < szAudio2 /2; i += 2)
        {
            ((short*)clip)[szAudio1 + i] += ((short*)clip2)[i] * leftVol;
            ((short*)clip)[szAudio1 + i + 1] += ((short*)clip2)[i + 1] * rightVol;
        }
    }
private:
    void* data;
    size_t bytes;
    int bps;
    int channels;
};

struct AudioObject
{
    AudioBuffer* buffer = 0;
    int cursor = 0;
    gfxm::vec3 position;
};

class AudioMixer3D
{
public:
    int Init()
    {
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
        
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0 };
        
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
                BuffDesc.dwBufferBytes = 44100 * 2;
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
    
    void Cleanup();
    
    void EarDistance(float meters) { earDistance = meters; }
    float EarDistance() { return earDistance; }
    
    MSG msg;
    void Update()
    {
        while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
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
            writePos, 8820,
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
            dPos = (44100 * 2 - prevWritePos) + writePos;
        }
        
        ZeroMemory(buffer, sizeof(buffer));
        //audioModule.Update((void*)buffer, 4410, dPos / 2);
        for(AudioObject* obj : objects)
        {
            obj->buffer->CopyData(
                (void*)buffer, 
                4410, 
                obj->cursor += dPos,
                gfxm::vec3(-0.1075f, 0.0f, 0.0f),
                gfxm::vec3(0.1075f, 0.0f, 0.0f),
                obj->position
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
        AudioBuffer* buf = new AudioBuffer();
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
    
    AudioObject* CreateInstance()
    {
        AudioObject* obj = new AudioObject();
        objects.push_back(obj);
        return obj;
    }
    void DestroyObject(AudioObject* obj)
    {
        for(unsigned i = 0; i < objects.size(); ++i)
            if(objects[i] == obj)
            {
                delete obj;
                objects.erase(objects.begin() + i);
            }
    }
    
private:
    float earDistance = 0.215f;
    std::vector<AudioBuffer*> buffers;
    std::vector<AudioObject*> objects;

    short buffer[88200];
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    LPDIRECTSOUNDBUFFER SecondaryBuffer;
    DWORD prevWritePos = 0;
    
    HWND hWnd;
};

extern "C"{
#include "stb_vorbis.h"
}

int main()
{
    AudioMixer3D mixer;
    mixer.Init();
    
    int channels = 2;
    int sampleRate = 44100;
    short* decoded;
    int len;
    len = stb_vorbis_decode_filename("test.ogg", &channels, &sampleRate, &decoded);
    
    AudioBuffer* buffer = mixer.CreateBuffer();
    buffer->Upload((void*)decoded, len * sizeof(short), sampleRate, channels);
    AudioObject* obj = mixer.CreateInstance();
    obj->buffer = buffer;
    obj->position = gfxm::vec3(0.2f, 0.2f, -0.65f);
    free(decoded);
    
    len = stb_vorbis_decode_filename("test2.ogg", &channels, &sampleRate, &decoded);
    buffer = mixer.CreateBuffer();
    buffer->Upload((void*)decoded, len * sizeof(short), sampleRate, channels);
    obj = mixer.CreateInstance();
    obj->buffer = buffer;
    obj->position = gfxm::vec3(-0.2f, -0.2f, 0.65f);
    free(decoded);
    
    while(1)
    {
        mixer.Update();
        Sleep(10);
    }
    return 0;
}