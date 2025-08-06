#include "AudioCaptureManager.h"
#include <iostream>
#include <comdef.h>
#include <algorithm>
#include <cmath>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <Mmreg.h>

AudioCaptureManager::AudioCaptureManager()
    : m_deviceEnumerator(nullptr)
    , m_device(nullptr)
    , m_audioClient(nullptr)
    , m_captureClient(nullptr)
    , m_waveFormat(nullptr)
    , m_bufferFrameCount(0)
    , m_dsCapture(nullptr)
    , m_dsCaptureBuffer(nullptr)
    , m_sampleRate(48000)
    , m_channelCount(2)
    , m_activeMethod(CaptureMethod::AUTO)
    , m_isCapturing(false)
    , m_shouldStop(false)
    , m_filePosition(0)
{
    ZeroMemory(&m_dsBufferDesc, sizeof(m_dsBufferDesc));
    ZeroMemory(&m_dsWaveFormat, sizeof(m_dsWaveFormat));
}

AudioCaptureManager::~AudioCaptureManager() {
    StopCapture();
    Cleanup();
}

bool AudioCaptureManager::Initialize(CaptureMethod method) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM: " << std::hex << hr << std::endl;
        return false;
    }

    if (method == CaptureMethod::AUTO) {
        // Try methods in order of preference
        std::cout << "Trying WASAPI Loopback..." << std::endl;
        if (InitializeWASAPILoopback()) {
            m_activeMethod = CaptureMethod::WASAPI_LOOPBACK;
            std::cout << "✅ WASAPI Loopback initialized successfully" << std::endl;
            return true;
        }

        std::cout << "Trying WASAPI Microphone..." << std::endl;
        if (InitializeWASAPIMicrophone()) {
            m_activeMethod = CaptureMethod::WASAPI_MICROPHONE;
            std::cout << "✅ WASAPI Microphone initialized successfully" << std::endl;
            return true;
        }

        std::cout << "Trying DirectSound..." << std::endl;
        if (InitializeDirectSound()) {
            m_activeMethod = CaptureMethod::DIRECTSOUND;
            std::cout << "✅ DirectSound initialized successfully" << std::endl;
            return true;
        }

        std::cout << "Using File Input (test mode)..." << std::endl;
        if (InitializeFileInput()) {
            m_activeMethod = CaptureMethod::FILE_INPUT;
            std::cout << "✅ File Input initialized successfully" << std::endl;
            return true;
        }

        std::cerr << "❌ All audio capture methods failed" << std::endl;
        return false;
    }

    // Try specific method
    switch (method) {
        case CaptureMethod::WASAPI_LOOPBACK:
            if (InitializeWASAPILoopback()) {
                m_activeMethod = method;
                return true;
            }
            break;
        case CaptureMethod::WASAPI_MICROPHONE:
            if (InitializeWASAPIMicrophone()) {
                m_activeMethod = method;
                return true;
            }
            break;
        case CaptureMethod::DIRECTSOUND:
            if (InitializeDirectSound()) {
                m_activeMethod = method;
                return true;
            }
            break;
        case CaptureMethod::FILE_INPUT:
            if (InitializeFileInput()) {
                m_activeMethod = method;
                return true;
            }
            break;
    }

    return false;
}

bool AudioCaptureManager::InitializeWASAPILoopback() {
    try {
        // Create device enumerator
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&m_deviceEnumerator);

        if (FAILED(hr)) {
            std::cerr << "Failed to create device enumerator: " << std::hex << hr << std::endl;
            return false;
        }

        // Get default render endpoint (for loopback)
        hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
        if (FAILED(hr)) {
            std::cerr << "Failed to get default render endpoint: " << std::hex << hr << std::endl;
            return false;
        }

        // Activate audio client
        hr = m_device->Activate(
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

        // Initialize the audio client for loopback
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK, // Capture system audio
            10000000, // 1 second buffer
            0,
            m_waveFormat,
            nullptr);

        if (FAILED(hr)) {
            std::cerr << "Failed to initialize audio client for loopback: " << std::hex << hr << std::endl;
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
    catch (...) {
        std::cerr << "Exception in InitializeWASAPILoopback" << std::endl;
        return false;
    }
}

bool AudioCaptureManager::InitializeWASAPIMicrophone() {
    try {
        // Create device enumerator
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&m_deviceEnumerator);

        if (FAILED(hr)) {
            std::cerr << "Failed to create device enumerator: " << std::hex << hr << std::endl;
            return false;
        }

        // Get default capture endpoint (microphone)
        hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &m_device);
        if (FAILED(hr)) {
            std::cerr << "Failed to get default capture endpoint: " << std::hex << hr << std::endl;
            return false;
        }

        // Activate audio client
        hr = m_device->Activate(
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

        std::cout << "Microphone format: " << m_sampleRate << " Hz, " << m_channelCount << " channels" << std::endl;

        // Initialize the audio client for microphone capture
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0, // No special flags for microphone
            10000000, // 1 second buffer
            0,
            m_waveFormat,
            nullptr);

        if (FAILED(hr)) {
            std::cerr << "Failed to initialize audio client for microphone: " << std::hex << hr << std::endl;
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
    catch (...) {
        std::cerr << "Exception in InitializeWASAPIMicrophone" << std::endl;
        return false;
    }
}

bool AudioCaptureManager::InitializeDirectSound() {
    try {
        // Create DirectSound capture object
        HRESULT hr = DirectSoundCaptureCreate(nullptr, &m_dsCapture, nullptr);
        if (FAILED(hr)) {
            std::cerr << "Failed to create DirectSound capture: " << std::hex << hr << std::endl;
            return false;
        }

        // Set up wave format
        m_dsWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
        m_dsWaveFormat.nChannels = 2;
        m_dsWaveFormat.nSamplesPerSec = 44100;
        m_dsWaveFormat.wBitsPerSample = 16;
        m_dsWaveFormat.nBlockAlign = (m_dsWaveFormat.nChannels * m_dsWaveFormat.wBitsPerSample) / 8;
        m_dsWaveFormat.nAvgBytesPerSec = m_dsWaveFormat.nSamplesPerSec * m_dsWaveFormat.nBlockAlign;
        m_dsWaveFormat.cbSize = 0;

        m_sampleRate = m_dsWaveFormat.nSamplesPerSec;
        m_channelCount = m_dsWaveFormat.nChannels;

        // Set up buffer description
        m_dsBufferDesc.dwSize = sizeof(DSCBUFFERDESC);
        m_dsBufferDesc.dwFlags = 0;
        m_dsBufferDesc.dwBufferBytes = m_dsWaveFormat.nAvgBytesPerSec; // 1 second buffer
        m_dsBufferDesc.dwReserved = 0;
        m_dsBufferDesc.lpwfxFormat = &m_dsWaveFormat;

        // Create capture buffer
        hr = m_dsCapture->CreateCaptureBuffer(&m_dsBufferDesc, &m_dsCaptureBuffer, nullptr);
        if (FAILED(hr)) {
            std::cerr << "Failed to create DirectSound capture buffer: " << std::hex << hr << std::endl;
            return false;
        }

        std::cout << "DirectSound format: " << m_sampleRate << " Hz, " << m_channelCount << " channels" << std::endl;
        return true;
    }
    catch (...) {
        std::cerr << "Exception in InitializeDirectSound" << std::endl;
        return false;
    }
}

bool AudioCaptureManager::InitializeFileInput() {
    // Generate a simple test tone for demonstration
    m_sampleRate = 44100;
    m_channelCount = 2;
    
    // Generate 5 seconds of test audio (sine waves at different frequencies)
    size_t sampleCount = m_sampleRate * 5; // 5 seconds
    m_fileAudioData.resize(sampleCount * m_channelCount);
    
    for (size_t i = 0; i < sampleCount; ++i) {
        float time = static_cast<float>(i) / m_sampleRate;
        
        // Left channel: 440 Hz sine wave (A note)
        float leftSample = 0.3f * sinf(2.0f * 3.14159f * 440.0f * time);
        
        // Right channel: 880 Hz sine wave (A note, one octave higher)
        float rightSample = 0.3f * sinf(2.0f * 3.14159f * 880.0f * time);
        
        // Add some variation to make it more interesting for haptics
        float modulation = 0.5f + 0.5f * sinf(2.0f * 3.14159f * 2.0f * time); // 2 Hz modulation
        
        m_fileAudioData[i * 2] = leftSample * modulation;
        m_fileAudioData[i * 2 + 1] = rightSample * modulation;
    }
    
    m_filePosition = 0;
    
    std::cout << "Test audio generated: " << m_sampleRate << " Hz, " << m_channelCount << " channels, " 
              << (m_fileAudioData.size() / m_channelCount / m_sampleRate) << " seconds" << std::endl;
    
    return true;
}

bool AudioCaptureManager::StartCapture() {
    if (m_isCapturing) {
        return true;
    }

    m_shouldStop = false;
    m_isCapturing = true;
    m_captureThread = std::thread(&AudioCaptureManager::CaptureThread, this);

    // Start the appropriate audio client
    if (m_activeMethod == CaptureMethod::WASAPI_LOOPBACK || 
        m_activeMethod == CaptureMethod::WASAPI_MICROPHONE) {
        if (m_audioClient) {
            HRESULT hr = m_audioClient->Start();
            if (FAILED(hr)) {
                std::cerr << "Failed to start audio client: " << std::hex << hr << std::endl;
                m_shouldStop = true;
                if (m_captureThread.joinable()) {
                    m_captureThread.join();
                }
                m_isCapturing = false;
                return false;
            }
        }
    }
    else if (m_activeMethod == CaptureMethod::DIRECTSOUND) {
        if (m_dsCaptureBuffer) {
            HRESULT hr = m_dsCaptureBuffer->Start(DSCBSTART_LOOPING);
            if (FAILED(hr)) {
                std::cerr << "Failed to start DirectSound capture: " << std::hex << hr << std::endl;
                m_shouldStop = true;
                if (m_captureThread.joinable()) {
                    m_captureThread.join();
                }
                m_isCapturing = false;
                return false;
            }
        }
    }

    std::cout << "Audio capture started using " << GetMethodName() << std::endl;
    return true;
}

void AudioCaptureManager::StopCapture() {
    if (!m_isCapturing) {
        return;
    }

    m_shouldStop = true;
    
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    // Stop the appropriate audio client
    if (m_audioClient) {
        m_audioClient->Stop();
    }
    if (m_dsCaptureBuffer) {
        m_dsCaptureBuffer->Stop();
    }

    m_isCapturing = false;
    std::cout << "Audio capture stopped" << std::endl;
}

void AudioCaptureManager::SetAudioCallback(AudioDataCallback callback) {
    m_audioCallback = callback;
}

std::string AudioCaptureManager::GetMethodName() const {
    switch (m_activeMethod) {
        case CaptureMethod::WASAPI_LOOPBACK: return "WASAPI Loopback (System Audio)";
        case CaptureMethod::WASAPI_MICROPHONE: return "WASAPI Microphone";
        case CaptureMethod::DIRECTSOUND: return "DirectSound";
        case CaptureMethod::FILE_INPUT: return "File Input (Test Mode)";
        default: return "Unknown";
    }
}

void AudioCaptureManager::CaptureThread() {
    switch (m_activeMethod) {
        case CaptureMethod::WASAPI_LOOPBACK:
        case CaptureMethod::WASAPI_MICROPHONE:
            WASAPICaptureLoop();
            break;
        case CaptureMethod::DIRECTSOUND:
            DirectSoundCaptureLoop();
            break;
        case CaptureMethod::FILE_INPUT:
            FileInputLoop();
            break;
    }
}

void AudioCaptureManager::WASAPICaptureLoop() {
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
                    // Convert from other formats to float (assumes 16-bit PCM)
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

        Sleep(10); // Small delay to prevent excessive CPU usage
    }
}

void AudioCaptureManager::DirectSoundCaptureLoop() {
    DWORD bufferSize = m_dsBufferDesc.dwBufferBytes;
    DWORD halfBuffer = bufferSize / 2;
    DWORD readPos = 0;
    std::vector<int16_t> buffer(halfBuffer / sizeof(int16_t));
    
    while (!m_shouldStop) {
        DWORD capturePos, readPosNew;
        HRESULT hr = m_dsCaptureBuffer->GetCurrentPosition(&capturePos, &readPosNew);
        if (FAILED(hr)) {
            std::cerr << "Failed to get DirectSound position: " << std::hex << hr << std::endl;
            break;
        }

        DWORD bytesAvailable;
        if (capturePos >= readPos) {
            bytesAvailable = capturePos - readPos;
        } else {
            bytesAvailable = bufferSize - readPos + capturePos;
        }

        if (bytesAvailable >= halfBuffer) {
            void* ptr1, *ptr2;
            DWORD bytes1, bytes2;
            
            hr = m_dsCaptureBuffer->Lock(readPos, halfBuffer, &ptr1, &bytes1, &ptr2, &bytes2, 0);
            if (SUCCEEDED(hr)) {
                // Copy data
                if (bytes1 > 0) {
                    memcpy(buffer.data(), ptr1, bytes1);
                }
                if (bytes2 > 0) {
                    memcpy(reinterpret_cast<char*>(buffer.data()) + bytes1, ptr2, bytes2);
                }
                
                m_dsCaptureBuffer->Unlock(ptr1, bytes1, ptr2, bytes2);
                
                // Convert to float and call callback
                if (m_audioCallback) {
                    std::vector<float> floatSamples(buffer.size());
                    for (size_t i = 0; i < buffer.size(); ++i) {
                        floatSamples[i] = static_cast<float>(buffer[i]) / 32768.0f;
                    }
                    m_audioCallback(floatSamples.data(), floatSamples.size(), m_channelCount);
                }
                
                readPos = (readPos + halfBuffer) % bufferSize;
            }
        }

        Sleep(10); // Small delay
    }
}

void AudioCaptureManager::FileInputLoop() {
    const size_t samplesPerCallback = 1024; // Process in chunks
    
    while (!m_shouldStop) {
        if (m_audioCallback && m_filePosition < m_fileAudioData.size()) {
            size_t samplesToRead = (std::min)(samplesPerCallback, m_fileAudioData.size() - m_filePosition);
            
            m_audioCallback(&m_fileAudioData[m_filePosition], samplesToRead, m_channelCount);
            
            m_filePosition += samplesToRead;
            
            // Loop the audio
            if (m_filePosition >= m_fileAudioData.size()) {
                m_filePosition = 0;
            }
        }
        
        // Simulate real-time playback
        Sleep(static_cast<DWORD>(1000.0 * samplesPerCallback / m_channelCount / m_sampleRate));
    }
}

void AudioCaptureManager::Cleanup() {
    // Clean up WASAPI
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

    // Clean up DirectSound
    if (m_dsCaptureBuffer) {
        m_dsCaptureBuffer->Release();
        m_dsCaptureBuffer = nullptr;
    }
    if (m_dsCapture) {
        m_dsCapture->Release();
        m_dsCapture = nullptr;
    }
}

// Static utility methods
std::vector<std::string> AudioCaptureManager::GetAvailableDevices() {
    std::vector<std::string> devices;
    
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&enumerator);
    
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &collection);
        
        if (SUCCEEDED(hr)) {
            UINT count;
            collection->GetCount(&count);
            
            for (UINT i = 0; i < count; i++) {
                IMMDevice* device = nullptr;
                if (SUCCEEDED(collection->Item(i, &device))) {
                    IPropertyStore* props = nullptr;
                    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        
                        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                            if (varName.vt == VT_LPWSTR) {
                                // Convert wide string to regular string
                                int len = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                                if (len > 0) {
                                    std::string deviceName(len - 1, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, &deviceName[0], len, nullptr, nullptr);
                                    devices.push_back(deviceName);
                                }
                            }
                        }
                        
                        PropVariantClear(&varName);
                        props->Release();
                    }
                    device->Release();
                }
            }
            collection->Release();
        }
        enumerator->Release();
    }
    
    return devices;
}

bool AudioCaptureManager::IsWASAPIAvailable() {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&enumerator);
    
    if (SUCCEEDED(hr)) {
        enumerator->Release();
        return true;
    }
    return false;
}

bool AudioCaptureManager::IsDirectSoundAvailable() {
    LPDIRECTSOUNDCAPTURE dsCapture = nullptr;
    HRESULT hr = DirectSoundCaptureCreate(nullptr, &dsCapture, nullptr);
    
    if (SUCCEEDED(hr) && dsCapture) {
        dsCapture->Release();
        return true;
    }
    return false;
}