#pragma once

// GameInput API Version Configuration
// Set this to 2 to use GameInput 2.0 haptic APIs
// Set this to 1 to use GameInput 1.0 rumble APIs
// Leave undefined for automatic detection

#ifndef GAMEINPUT_API_VERSION
    // GameInput 2.0.26100.5334 supports the modern haptic API
    #define GAMEINPUT_API_VERSION 2
#endif

// API Version Information
#if GAMEINPUT_API_VERSION >= 2
    #define GAMEINPUT_SUPPORTS_HAPTICS 1
    #define GAMEINPUT_SUPPORTS_RUMBLE 1
    #define GAMEINPUT_VERSION_STRING "GameInput 2.0+"
#else
    #define GAMEINPUT_SUPPORTS_HAPTICS 0
    #define GAMEINPUT_SUPPORTS_RUMBLE 1
    #define GAMEINPUT_VERSION_STRING "GameInput 1.0"
#endif

// Feature macros for conditional compilation
#if GAMEINPUT_SUPPORTS_HAPTICS
    #define IF_HAPTICS_SUPPORTED(code) code
#else
    #define IF_HAPTICS_SUPPORTED(code)
#endif

#if GAMEINPUT_SUPPORTS_RUMBLE
    #define IF_RUMBLE_SUPPORTED(code) code
#else
    #define IF_RUMBLE_SUPPORTED(code)
#endif