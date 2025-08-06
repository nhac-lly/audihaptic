#include "HapticController.h"
#include <iostream>
#include <algorithm>

HapticController::HapticController()
    : m_gameInput(nullptr)
    , m_activeMode(HapticMode::Auto)
    , m_lastUpdate(std::chrono::steady_clock::now())
{
}

HapticController::~HapticController() {
    Shutdown();
}

bool HapticController::Initialize() {
    HRESULT hr = GameInputCreate(&m_gameInput);
    if (FAILED(hr)) {
        std::cerr << "Failed to create GameInput: " << std::hex << hr << std::endl;
        return false;
    }

    std::cout << "GameInput initialized successfully" << std::endl;
    
    // Determine the best haptic mode based on API version and settings
    switch (m_settings.preferredMode) {
        case HapticMode::Auto:
            // Try haptic first (GameInput 2.0), fall back to rumble (GameInput 1.0)
            #if GAMEINPUT_API_VERSION >= 2
                m_activeMode = HapticMode::Haptic;
                std::cout << "Using GameInput 2.0 Haptic API (auto-detected)" << std::endl;
            #else
                m_activeMode = HapticMode::Rumble;
                std::cout << "Using GameInput 1.0 Rumble API (auto-detected)" << std::endl;
            #endif
            break;
        case HapticMode::Haptic:
            m_activeMode = HapticMode::Haptic;
            std::cout << "Using GameInput 2.0 Haptic API (forced)" << std::endl;
            break;
        case HapticMode::Rumble:
            m_activeMode = HapticMode::Rumble;
            std::cout << "Using GameInput 1.0 Rumble API (forced)" << std::endl;
            break;
        case HapticMode::Hybrid:
            m_activeMode = HapticMode::Hybrid;
            std::cout << "Using Hybrid mode (both APIs)" << std::endl;
            break;
    }
    
    // Find initial gamepads
    FindGamepads();
    
    return true;
}

void HapticController::Shutdown() {
    StopAllHaptics();
    CleanupDevices();
    
    if (m_gameInput) {
        m_gameInput->Release();
        m_gameInput = nullptr;
    }
}

bool HapticController::FindGamepads() {
    if (!m_gameInput) {
        return false;
    }

    // Clean up existing devices first
    CleanupDevices();

    // Enumerate gamepad devices
    IGameInputReading* reading = nullptr;
    HRESULT hr = m_gameInput->GetCurrentReading(GameInputKindGamepad, nullptr, &reading);
    
    if (SUCCEEDED(hr) && reading) {
        IGameInputDevice* device = nullptr;
        reading->GetDevice(&device);
        if (device) {
            // Check if we already have this device
            bool found = false;
            for (const auto& gamepad : m_gamepads) {
                if (gamepad.device == device) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                GamepadInfo info;
                info.device = device;
                info.device->AddRef(); // Keep reference
                info.lastUpdate = std::chrono::steady_clock::now();
                
                // Detect device capabilities
                DetectDeviceCapabilities(info);
                
                m_gamepads.push_back(info);
                
                std::cout << "Found gamepad device - Rumble: " << (info.supportsRumble ? "Yes" : "No") 
                         << ", Haptics: " << (info.supportsHaptics ? "Yes" : "No") << std::endl;
            }
        }
        reading->Release();
    }

    std::cout << "Total gamepads found: " << m_gamepads.size() << std::endl;
    return !m_gamepads.empty();
}

void HapticController::UpdateDevices() {
    // Periodically check for new devices
    auto now = std::chrono::steady_clock::now();
    static auto lastDeviceCheck = now;
    
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDeviceCheck).count() > 1000) {
        FindGamepads();
        lastDeviceCheck = now;
    }
}

void HapticController::ProcessAudioFeatures(const AudioProcessor::AudioFeatures& features) {
    if (m_gamepads.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastUpdate).count();
    m_lastUpdate = now;

    // Update haptics for all connected gamepads
    for (auto& gamepad : m_gamepads) {
        UpdateGamepadHaptics(gamepad, features);
    }
}

void HapticController::UpdateGamepadHaptics(GamepadInfo& gamepad, const AudioProcessor::AudioFeatures& features) {
    if (!gamepad.device) {
        return;
    }

    // Calculate target intensities based on audio features
    float targetLeftMotor = 0.0f;
    float targetRightMotor = 0.0f;
    float targetLeftTrigger = 0.0f;
    float targetRightTrigger = 0.0f;

    if (m_settings.useRumbleMotors) {
        // Map bass to left motor, treble to right motor
        if (m_settings.useLowFrequencyMotor) {
            targetLeftMotor = features.bass * m_settings.bassIntensity;
        }
        
        if (m_settings.useHighFrequencyMotor) {
            targetRightMotor = features.treble * m_settings.trebleIntensity;
        }
        
        // Add overall volume contribution to both motors
        float volumeContribution = features.volume * m_settings.volumeIntensity * 0.5f;
        targetLeftMotor += volumeContribution;
        targetRightMotor += volumeContribution;
    }

    if (m_settings.useImpulseMotor) {
        // Use trigger motors for dynamic range and peaks
        float dynamicContribution = features.dynamic_range * m_settings.dynamicIntensity;
        targetLeftTrigger = dynamicContribution;
        targetRightTrigger = dynamicContribution;
        
        // Add peak information to triggers
        float peakContribution = features.peak * 0.3f;
        targetLeftTrigger += peakContribution;
        targetRightTrigger += peakContribution;
    }

    // Clamp values to valid range [0, 1]
    targetLeftMotor = std::clamp(targetLeftMotor, 0.0f, 1.0f);
    targetRightMotor = std::clamp(targetRightMotor, 0.0f, 1.0f);
    targetLeftTrigger = std::clamp(targetLeftTrigger, 0.0f, 1.0f);
    targetRightTrigger = std::clamp(targetRightTrigger, 0.0f, 1.0f);

    // Apply smooth transitions
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - gamepad.lastUpdate).count();
    gamepad.lastUpdate = now;

    gamepad.currentLeftMotor = SmoothTransition(gamepad.currentLeftMotor, targetLeftMotor, deltaTime);
    gamepad.currentRightMotor = SmoothTransition(gamepad.currentRightMotor, targetRightMotor, deltaTime);
    gamepad.currentLeftTrigger = SmoothTransition(gamepad.currentLeftTrigger, targetLeftTrigger, deltaTime);
    gamepad.currentRightTrigger = SmoothTransition(gamepad.currentRightTrigger, targetRightTrigger, deltaTime);

    // Apply haptic feedback using GameInput 2.0 API
    GameInputRumbleParams params = {};
    params.lowFrequency = gamepad.currentLeftMotor;
    params.highFrequency = gamepad.currentRightMotor;
    params.leftTrigger = gamepad.currentLeftTrigger;
    params.rightTrigger = gamepad.currentRightTrigger;
    
    gamepad.device->SetRumbleState(&params);
}

float HapticController::SmoothTransition(float current, float target, float deltaTime) {
    if (m_settings.fadeTimeMs == 0) {
        return target;
    }
    
    float fadeRate = 1000.0f / static_cast<float>(m_settings.fadeTimeMs); // Convert ms to rate per second
    float maxChange = fadeRate * deltaTime;
    
    float difference = target - current;
    if (std::abs(difference) <= maxChange) {
        return target;
    }
    
    return current + (difference > 0 ? maxChange : -maxChange);
}

void HapticController::SetRumble(float leftMotor, float rightMotor, float leftTrigger, float rightTrigger) {
    leftMotor = std::clamp(leftMotor, 0.0f, 1.0f);
    rightMotor = std::clamp(rightMotor, 0.0f, 1.0f);
    leftTrigger = std::clamp(leftTrigger, 0.0f, 1.0f);
    rightTrigger = std::clamp(rightTrigger, 0.0f, 1.0f);

    for (auto& gamepad : m_gamepads) {
        if (gamepad.device) {
            GameInputRumbleParams params = {};
            params.lowFrequency = leftMotor;
            params.highFrequency = rightMotor;
            params.leftTrigger = leftTrigger;
            params.rightTrigger = rightTrigger;
            
            gamepad.device->SetRumbleState(&params);
            
            gamepad.currentLeftMotor = leftMotor;
            gamepad.currentRightMotor = rightMotor;
            gamepad.currentLeftTrigger = leftTrigger;
            gamepad.currentRightTrigger = rightTrigger;
        }
    }
}

void HapticController::StopAllHaptics() {
    for (auto& gamepad : m_gamepads) {
        if (gamepad.device) {
            GameInputRumbleParams params = {};
            params.lowFrequency = 0.0f;
            params.highFrequency = 0.0f;
            params.leftTrigger = 0.0f;
            params.rightTrigger = 0.0f;
            
            gamepad.device->SetRumbleState(&params);
            
            gamepad.currentLeftMotor = 0.0f;
            gamepad.currentRightMotor = 0.0f;
            gamepad.currentLeftTrigger = 0.0f;
            gamepad.currentRightTrigger = 0.0f;
        }
    }
}

void HapticController::CleanupDevices() {
    for (auto& gamepad : m_gamepads) {
        if (gamepad.device) {
            // Stop all haptic feedback before cleanup
            GameInputRumbleParams params = {};
            gamepad.device->SetRumbleState(&params);
            gamepad.device->Release();
        }
    }
    m_gamepads.clear();
}

const char* HapticController::GetHapticModeString() const {
    switch (m_activeMode) {
        case HapticMode::Auto: return "Auto";
        case HapticMode::Rumble: return "Rumble (GameInput 1.0)";
        case HapticMode::Haptic: return "Haptic (GameInput 2.0)";
        case HapticMode::Hybrid: return "Hybrid (Both APIs)";
        default: return "Unknown";
    }
}

void HapticController::DetectDeviceCapabilities(GamepadInfo& gamepad) {
    if (!gamepad.device) return;

    const GameInputDeviceInfo* deviceInfo = nullptr;
    HRESULT hr = gamepad.device->GetDeviceInfo(&deviceInfo);

    if (SUCCEEDED(hr) && deviceInfo) {
        // GameInput 2.0 supports rumble via SetRumbleState
        gamepad.supportsRumble = true;
        gamepad.rumbleMotorCount = 4; // Low/High frequency + Left/Right triggers
        
        // Check for haptic support
        GameInputHapticInfo hapticInfo = {};
        hr = gamepad.device->GetHapticInfo(&hapticInfo);
        if (SUCCEEDED(hr)) {
            gamepad.supportsHaptics = true;
            gamepad.hapticMotorCount = hapticInfo.locationCount;
        } else {
            gamepad.supportsHaptics = false;
            gamepad.hapticMotorCount = 0;
        }
        
        std::cout << "Device capabilities - Rumble: " << (gamepad.supportsRumble ? "Yes" : "No")
                  << ", Haptic motors: " << gamepad.hapticMotorCount << std::endl;
    } else {
        std::cerr << "Failed to get device info: " << std::hex << hr << std::endl;
        // Default to basic rumble support
        gamepad.supportsRumble = true;
        gamepad.rumbleMotorCount = 4;
        gamepad.supportsHaptics = false;
        gamepad.hapticMotorCount = 0;
    }
}

