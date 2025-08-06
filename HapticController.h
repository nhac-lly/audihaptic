#pragma once

#include "GameInputConfig.h"
#include <GameInput.h>
#include <memory>
#include <vector>
#include <chrono>
#include "AudioProcessor.h"

// Use appropriate GameInput namespace
#if GAMEINPUT_API_VERSION >= 2
using namespace GameInput::v2;
#elif GAMEINPUT_API_VERSION >= 1
using namespace GameInput::v1;
#endif

class HapticController {
public:
    enum class HapticMode {
        Auto,       // Automatically detect best available mode
        Rumble,     // Use traditional rumble API (GameInput 1.0)
        Haptic,     // Use modern haptic API (GameInput 2.0)
        Hybrid      // Use both if available
    };

    struct HapticSettings {
        float bassIntensity = 1.0f;      // Intensity multiplier for bass (0.0 - 2.0)
        float trebleIntensity = 0.8f;    // Intensity multiplier for treble (0.0 - 2.0)
        float volumeIntensity = 0.6f;    // Intensity multiplier for overall volume (0.0 - 2.0)
        float dynamicIntensity = 0.4f;   // Intensity multiplier for dynamic range (0.0 - 2.0)
        
        // Motor assignments (which motors to use for different frequency ranges)
        bool useLowFrequencyMotor = true;   // Use low-frequency motor for bass
        bool useHighFrequencyMotor = true;  // Use high-frequency motor for treble
        bool useImpulseMotor = true;        // Use impulse triggers for dynamics
        bool useRumbleMotors = true;        // Use traditional rumble motors
        
        // Timing settings
        uint32_t updateRateMs = 16;         // Update rate in milliseconds (~60 FPS)
        uint32_t fadeTimeMs = 100;          // Fade time for smooth transitions
        
        // API preference
        HapticMode preferredMode = HapticMode::Auto;  // Preferred haptic mode
    };

    HapticController();
    ~HapticController();

    bool Initialize();
    void Shutdown();

    // Device management
    bool FindGamepads();
    size_t GetGamepadCount() const { return m_gamepads.size(); }
    
    // Haptic feedback
    void ProcessAudioFeatures(const AudioProcessor::AudioFeatures& features);
    void SetHapticSettings(const HapticSettings& settings) { m_settings = settings; }
    const HapticSettings& GetHapticSettings() const { return m_settings; }
    
    // Manual control
    void SetRumble(float leftMotor, float rightMotor, float leftTrigger = 0.0f, float rightTrigger = 0.0f);
    void StopAllHaptics();
    
    // Status
    bool IsInitialized() const { return m_gameInput != nullptr; }
    void UpdateDevices(); // Call periodically to detect new/removed devices
    HapticMode GetActiveHapticMode() const { return m_activeMode; }
    const char* GetHapticModeString() const;

private:
    struct GamepadInfo {
        IGameInputDevice* device;
        std::chrono::steady_clock::time_point lastUpdate;
        
        // Device capabilities
        bool supportsRumble;
        bool supportsHaptics;
        uint32_t hapticMotorCount;
        uint32_t rumbleMotorCount;
        
        // Current haptic state
        float currentLeftMotor;
        float currentRightMotor;
        float currentLeftTrigger;
        float currentRightTrigger;
        
        GamepadInfo() : device(nullptr), supportsRumble(false), supportsHaptics(false),
                       hapticMotorCount(0), rumbleMotorCount(0),
                       currentLeftMotor(0), currentRightMotor(0),
                       currentLeftTrigger(0), currentRightTrigger(0) {}
    };

    void CleanupDevices();
    void UpdateGamepadHaptics(GamepadInfo& gamepad, const AudioProcessor::AudioFeatures& features);
    float SmoothTransition(float current, float target, float deltaTime);
    
    // Device capability detection
    void DetectDeviceCapabilities(GamepadInfo& gamepad);

    
    // GameInput
    IGameInput* m_gameInput;
    std::vector<GamepadInfo> m_gamepads;
    
    // Settings
    HapticSettings m_settings;
    HapticMode m_activeMode;
    
    // Timing
    std::chrono::steady_clock::time_point m_lastUpdate;
};