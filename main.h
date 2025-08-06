#pragma once

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <iomanip>
#include <conio.h>
#include "AudioCaptureManager.h"
#include "AudioProcessor.h"
#include "HapticController.h"

class AudioHapticsApp {
public:
    AudioHapticsApp() = default;
    ~AudioHapticsApp() = default;

    bool Initialize();
    void Run();
    void RunService(); // Non-blocking version for service mode
    void Shutdown();

private:
    void OnAudioData(const float* samples, size_t sampleCount, size_t channels);
    void DisplayLiveStats();
    bool HandleKeyPress(char key);
    void AdjustSensitivity();
    void ConfigureHaptics();
    void ConfigureHapticMode();
    void TestHaptics();
    void RefreshDevices();

    AudioCaptureManager m_audioCapture;
    AudioProcessor m_audioProcessor;
    HapticController m_hapticController;

    std::mutex m_featuresMutex;
    AudioProcessor::AudioFeatures m_latestFeatures{};
    bool m_running = false;
}; 