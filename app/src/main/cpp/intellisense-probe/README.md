# MobileClock IntelliSense probe

This is an independent Android CMake project used to isolate Visual Studio
IntelliSense issues. It deliberately has no dependency on the parent CMake
project or `UtilityHelpersLib`. It reuses only the NDK compiler drivers from
the adjacent `cmake/vs-android-drivers` directory: Visual Studio cannot query
standard-library defaults through the NDK's target-specific `.cmd` wrappers.

Open **this folder** in Visual Studio, select `android-arm64-debug`, and run
**Configure CMake**. `probe.cpp` must resolve the C++ standard-library headers
before adding any project dependency here.
