#include "AudioCapture.h"
#include <iostream>
#include <comdef.h>

AudioCapture::AudioCapture()
    : m_deviceEnumerator(nullptr)
    , m_device(nullptr)
    , m_audioClient(nullptr)
    , m_captureClient(nullptr)
    , m_waveFormat(nullptr)
    , m_bufferFrameCount(0)
    , m_sampleRate(0)
    , m_channelCount(0)
    , m_isCapturing(false)
    , m_shouldStop(false)
{
}

AudioCapture::~AudioCapture() {
    StopCapture();
    Cleanup();
}

bool AudioCapture::Initialize() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM: " << std::hex << hr << std::endl;
        return false;
    }

    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&m_deviceEnumerator);

    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator: " << std::hex << hr << std::endl;
        return false;
    }

    // Get default audio endpoint
    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &m_device);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint: " << std::hex << hr << std::endl;
        return false;
    }

    return InitializeAudioClient();
}

bool AudioCapture::InitializeAudioClient() {
    HRESULT hr = m_device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, (void**)&m_audioClient);

    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client: " << std::hex << hr << std::endl;
        return false;
    }

    // Get the default format
    hr = m_audioClient->GetMixFormat(&m_waveFormat);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format: " << std::hex << hr << std::endl;
        return false;
    }

    // Store format information
    m_sampleRate = m_waveFormat->nSamplesPerSec;
    m_channelCount = m_waveFormat->nChannels;

    std::cout << "Audio format: " << m_sampleRate << " Hz, " << m_channelCount << " channels" << std::endl;

    // Initialize the audio client
    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, // Capture system audio (loopback)
        10000000, // 1 second buffer
        0,
        m_waveFormat,
        nullptr);

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client: " << std::hex << hr << std::endl;
        return false;
    }

    // Get buffer size
    hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);
    if (FAILED(hr)) {
        std::cerr << "Failed to get buffer size: " << std::hex << hr << std::endl;
        return false;
    }

    // Get capture client
    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client: " << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

bool AudioCapture::StartCapture() {
    if (m_isCapturing) {
        return true;
    }

    if (!m_audioClient) {
        std::cerr << "Audio client not initialized" << std::endl;
        return false;
    }

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Failed to start audio client: " << std::hex << hr << std::endl;
        return false;
    }

    m_shouldStop = false;
    m_isCapturing = true;
    m_captureThread = std::thread(&AudioCapture::CaptureThread, this);

    std::cout << "Audio capture started" << std::endl;
    return true;
}

void AudioCapture::StopCapture() {
    if (!m_isCapturing) {
        return;
    }

    m_shouldStop = true;
    
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    if (m_audioClient) {
        m_audioClient->Stop();
    }

    m_isCapturing = false;
    std::cout << "Audio capture stopped" << std::endl;
}

void AudioCapture::SetAudioCallback(AudioDataCallback callback) {
    m_audioCallback = callback;
}

void AudioCapture::CaptureThread() {
    while (!m_shouldStop) {
        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        
        if (FAILED(hr)) {
            std::cerr << "Failed to get packet size: " << std::hex << hr << std::endl;
            break;
        }

        while (packetLength != 0) {
            BYTE* data;
            UINT32 framesAvailable;
            DWORD flags;

            hr = m_captureClient->GetBuffer(&data, &framesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer: " << std::hex << hr << std::endl;
                break;
            }

            if (m_audioCallback && framesAvailable > 0) {
                // Convert to float samples if needed
                if (m_waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                    (m_waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_waveFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                    
                    const float* samples = reinterpret_cast<const float*>(data);
                    m_audioCallback(samples, framesAvailable * m_channelCount, m_channelCount);
                }
                else {
                    // Convert from other formats to float (simplified - assumes 16-bit PCM)
                    std::vector<float> floatSamples(framesAvailable * m_channelCount);
                    const int16_t* int16Data = reinterpret_cast<const int16_t*>(data);
                    
                    for (size_t i = 0; i < floatSamples.size(); ++i) {
                        floatSamples[i] = static_cast<float>(int16Data[i]) / 32768.0f;
                    }
                    
                    m_audioCallback(floatSamples.data(), floatSamples.size(), m_channelCount);
                }
            }

            hr = m_captureClient->ReleaseBuffer(framesAvailable);
            if (FAILED(hr)) {
                std::cerr << "Failed to release buffer: " << std::hex << hr << std::endl;
                break;
            }

            hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                break;
            }
        }

        Sleep(1); // Small delay to prevent excessive CPU usage
    }
}

void AudioCapture::Cleanup() {
    if (m_captureClient) {
        m_captureClient->Release();
        m_captureClient = nullptr;
    }

    if (m_audioClient) {
        m_audioClient->Release();
        m_audioClient = nullptr;
    }

    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }

    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
        m_deviceEnumerator = nullptr;
    }

    if (m_waveFormat) {
        CoTaskMemFree(m_waveFormat);
        m_waveFormat = nullptr;
    }
}