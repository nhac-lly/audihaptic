#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

class AudioCapture {
public:
    using AudioDataCallback = std::function<void(const float* samples, size_t sampleCount, size_t channels)>;

    AudioCapture();
    ~AudioCapture();

    bool Initialize();
    bool StartCapture();
    void StopCapture();
    void SetAudioCallback(AudioDataCallback callback);

    bool IsCapturing() const { return m_isCapturing; }
    UINT32 GetSampleRate() const { return m_sampleRate; }
    UINT32 GetChannelCount() const { return m_channelCount; }

private:
    void CaptureThread();
    bool InitializeAudioClient();
    void Cleanup();

    // COM interfaces
    IMMDeviceEnumerator* m_deviceEnumerator;
    IMMDevice* m_device;
    IAudioClient* m_audioClient;
    IAudioCaptureClient* m_captureClient;

    // Audio format
    WAVEFORMATEX* m_waveFormat;
    UINT32 m_bufferFrameCount;
    UINT32 m_sampleRate;
    UINT32 m_channelCount;

    // Threading
    std::thread m_captureThread;
    std::atomic<bool> m_isCapturing;
    std::atomic<bool> m_shouldStop;

    // Callback
    AudioDataCallback m_audioCallback;
};