#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <dsound.h>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

class AudioCaptureManager {
public:
    enum class CaptureMethod {
        WASAPI_LOOPBACK,    // System audio (speakers/headphones output)
        WASAPI_MICROPHONE,  // Microphone input
        DIRECTSOUND,        // DirectSound capture (fallback)
        FILE_INPUT,         // File-based input (for testing)
        AUTO                // Try methods in order until one works
    };

    using AudioDataCallback = std::function<void(const float* samples, size_t sampleCount, size_t channels)>;

    AudioCaptureManager();
    ~AudioCaptureManager();

    bool Initialize(CaptureMethod method = CaptureMethod::AUTO);
    bool StartCapture();
    void StopCapture();
    void SetAudioCallback(AudioDataCallback callback);

    bool IsCapturing() const { return m_isCapturing; }
    UINT32 GetSampleRate() const { return m_sampleRate; }
    UINT32 GetChannelCount() const { return m_channelCount; }
    CaptureMethod GetActiveMethod() const { return m_activeMethod; }
    std::string GetMethodName() const;

    // Static utility methods
    static std::vector<std::string> GetAvailableDevices();
    static bool IsWASAPIAvailable();
    static bool IsDirectSoundAvailable();

private:
    bool InitializeWASAPILoopback();
    bool InitializeWASAPIMicrophone();
    bool InitializeDirectSound();
    bool InitializeFileInput();

    void CaptureThread();
    void WASAPICaptureLoop();
    void DirectSoundCaptureLoop();
    void FileInputLoop();
    void Cleanup();

    // WASAPI members
    IMMDeviceEnumerator* m_deviceEnumerator;
    IMMDevice* m_device;
    IAudioClient* m_audioClient;
    IAudioCaptureClient* m_captureClient;
    WAVEFORMATEX* m_waveFormat;
    UINT32 m_bufferFrameCount;

    // DirectSound members
    LPDIRECTSOUNDCAPTURE m_dsCapture;
    LPDIRECTSOUNDCAPTUREBUFFER m_dsCaptureBuffer;
    DSCBUFFERDESC m_dsBufferDesc;
    WAVEFORMATEX m_dsWaveFormat;

    // Audio format
    UINT32 m_sampleRate;
    UINT32 m_channelCount;
    CaptureMethod m_activeMethod;

    // Threading
    std::thread m_captureThread;
    std::atomic<bool> m_isCapturing;
    std::atomic<bool> m_shouldStop;

    // Callback
    AudioDataCallback m_audioCallback;

    // File input (for testing)
    std::string m_testAudioFile;
    std::vector<float> m_fileAudioData;
    size_t m_filePosition;
};