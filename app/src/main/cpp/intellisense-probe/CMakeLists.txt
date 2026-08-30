cmake_minimum_required(VERSION 3.22.1)

# Isolated baseline for checking Visual Studio Android IntelliSense.
# Keep this target dependency-free; add project components one at a time only
# after the standard-library headers resolve in this project.
project(mobileclock_intellisense_probe LANGUAGES C CXX)

add_library(intellisense_probe STATIC probe.cpp)

set_target_properties(intellisense_probe PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# NDK's toolchain supplies these directories to Clang implicitly. VS 18's
# CMake IntelliSense does not import that implicit list, so publish it once on
# the target. This covers every Android/libc++ header without declaring any
# std symbol manually.
set(android_ndk_toolchain
    "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64")
target_compile_options(intellisense_probe PRIVATE
    "SHELL:-I \"${android_ndk_toolchain}/sysroot/usr/include/c++/v1\""
    "SHELL:-I \"${android_ndk_toolchain}/lib/clang/18/include\""
    "SHELL:-I \"${android_ndk_toolchain}/sysroot/usr/include/aarch64-linux-android\""
    "SHELL:-I \"${android_ndk_toolchain}/sysroot/usr/include\""
)
