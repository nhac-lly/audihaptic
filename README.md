# Audio to Haptics Converter

A Windows application that captures system audio and converts it to haptic feedback using the GameInput SDK. Experience your music, games, and media through vibration patterns on your gamepad!

## Features

- **Real-time Audio Capture**: Uses Windows Core Audio (WASAPI) to capture system audio with low latency
- **Advanced Audio Processing**: Analyzes audio for volume, bass, treble, midrange, and dynamic range
- **GameInput Integration**: Supports both GameInput 1.0 (rumble) and 2.0 (haptics) APIs
- **Multiple Haptic Motors**: Supports traditional rumble motors and modern haptic actuators
- **Automatic API Detection**: Intelligently chooses the best available haptic API
- **Customizable Settings**: Adjust sensitivity and intensity for different frequency ranges
- **Live Visualization**: Real-time display of audio levels and haptic output
- **Device Management**: Automatic detection and management of multiple gamepads

## System Requirements

- **Operating System**: Windows 10 19H1 (May 2019 update) or later
- **Development Environment**: Visual Studio 2022 with C++ workload
- **Hardware**: Compatible gamepad with haptic feedback support (Xbox controllers recommended)
- **Audio**: Any audio output device supported by Windows

## Setup Instructions

### 1. Install Dependencies

The project uses NuGet packages that need to be restored:

```bash
# Install GameInput redistributable (run as Administrator)
winget install Microsoft.GameInput

# Or manually install the NuGet package in Visual Studio:
# Tools -> NuGet Package Manager -> Package Manager Console
# Install-Package Microsoft.GameInput
```

### 2. Build the Project

1. Open `AudioHaptics.sln` in Visual Studio 2022
2. Set the build configuration to `x64` (required for GameInput)
3. Build the solution (`Ctrl+Shift+B`)

### 3. Connect Your Gamepad

- Connect an Xbox controller or compatible gamepad
- Ensure it's recognized by Windows (check in Settings > Gaming > Xbox Game Bar)
- The application will automatically detect connected gamepads

## Usage

### Running the Application

1. Launch `AudioHaptics.exe` from the output directory
2. The application will initialize audio capture and gamepad detection
3. Start playing audio from any source (music, games, videos, etc.)
4. Feel the haptic feedback on your gamepad!

### Controls

While the application is running, use these keyboard shortcuts:

- **Q**: Quit the application
- **S**: Adjust audio sensitivity (Low/Normal/High/Very High/Extreme)
- **H**: Configure haptic settings (bass, treble, volume, dynamic intensities)
- **T**: Test haptic motors (verify gamepad functionality)
- **R**: Refresh connected devices

### Understanding the Haptic Mapping

The application maps different audio characteristics to different haptic motors:

- **Left Rumble Motor**: Bass frequencies and low-end content
- **Right Rumble Motor**: Treble frequencies and high-end content
- **Impulse Triggers**: Dynamic range, peaks, and transient sounds
- **Combined Effects**: Overall volume contributes to all motors

### Customization Options

#### Audio Sensitivity

- **Low (0.5x)**: Subtle haptic feedback, good for background music
- **Normal (1.0x)**: Balanced feedback for most content
- **High (1.5x)**: Enhanced feedback for gaming and movies
- **Very High (2.0x)**: Strong feedback for immersive experiences
- **Extreme (3.0x)**: Maximum feedback intensity

#### Haptic Intensities

- **Bass Intensity**: Controls low-frequency motor response (0.0-2.0)
- **Treble Intensity**: Controls high-frequency motor response (0.0-2.0)
- **Volume Intensity**: Controls overall volume contribution (0.0-2.0)
- **Dynamic Intensity**: Controls transient and peak response (0.0-2.0)

## Technical Details

### Architecture

The application consists of four main components:

1. **AudioCapture**: Uses WASAPI loopback mode to capture system audio
2. **AudioProcessor**: Analyzes audio signals for frequency content and dynamics
3. **HapticController**: Maps audio features to gamepad haptic motors
4. **Main Application**: Provides user interface and coordinates components

### Audio Processing

- **Sample Rate**: Supports various sample rates (typically 44.1kHz or 48kHz)
- **Channels**: Automatically handles mono and stereo audio
- **Frequency Analysis**: Uses time-domain filtering for bass/treble separation
- **Dynamic Range**: Calculates difference between RMS and peak levels
- **Smoothing**: Applies temporal smoothing to prevent abrupt haptic changes

### Haptic Feedback

- **Update Rate**: ~60 FPS (16ms updates) for smooth haptic response
- **Motor Types**: Supports traditional rumble and modern impulse triggers
- **Fade Transitions**: Smooth transitions between haptic intensities
- **Multi-device**: Can control multiple gamepads simultaneously

## Troubleshooting

### No Audio Detected

- Check that audio is playing from the default playback device
- Verify Windows audio mixer shows activity
- Try adjusting the sensitivity setting

### No Haptic Feedback

- Ensure gamepad is connected and recognized by Windows
- Test gamepad in other applications or Windows Game Bar
- Try the built-in haptic test function (press 'T')
- Check that gamepad batteries are charged

### Performance Issues

- Close unnecessary audio applications
- Reduce haptic update rate in settings
- Check Windows audio exclusive mode settings

### GameInput Errors

- Install the latest GameInput redistributable
- Run the application as Administrator if needed
- Update gamepad drivers through Device Manager

## Development Notes

### Building from Source

Required Visual Studio components:

- MSVC v143 - VS 2022 C++ x64/x86 build tools
- Windows 10/11 SDK (latest version)
- NuGet package manager

### Dependencies

- **GameInput SDK**: Microsoft's modern input API
- **Windows Core Audio**: For system audio capture
- **C++20 Standard**: Modern C++ features and libraries

### Project Structure

```
AudioHaptics/
├── main.cpp              # Main application and UI
├── AudioCapture.h/.cpp   # WASAPI audio capture
├── AudioProcessor.h/.cpp # Audio analysis and processing
├── HapticController.h/.cpp # GameInput haptic control (1.0 & 2.0)
├── GameInputConfig.h     # GameInput API version configuration
├── AudioHaptics.vcxproj  # Visual Studio project file
├── AudioHaptics.sln      # Visual Studio solution
├── packages.config       # NuGet dependencies
└── README.md             # This file
```

## License

This project is provided as-is for educational and personal use. The GameInput SDK is provided by Microsoft under their respective license terms.

## Contributing

Feel free to submit issues, feature requests, or pull requests to improve the application!

## Acknowledgments

- Microsoft for the GameInput SDK and Windows Core Audio APIs
- The audio processing algorithms are inspired by various DSP techniques
- Special thanks to the Windows gaming and audio development communities
