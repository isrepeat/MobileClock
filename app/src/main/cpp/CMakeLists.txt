cmake_minimum_required(VERSION 3.22.1)
project("mobileclock")

add_library(mobileclock SHARED native_renderer.cpp)
set_target_properties(mobileclock PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# The NDK toolchain already supplies this sysroot to Clang.  Listing it on the
# target also lets Visual Studio IntelliSense discover Android/EGL/JNI headers.
if (ANDROID)
    set(ANDROID_NDK_SYSROOT
        "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot"
    )

    # VS 2026's deprecated Android IntelliSense ignores CMake File API's
    # `sysroot` field.  Keep the redundant -I flag so the NDK headers also
    # appear in the target's ordinary compile-command fragments.
    target_compile_options(mobileclock PRIVATE
        "-I${ANDROID_NDK_SYSROOT}/usr/include"
        "-I${ANDROID_NDK_SYSROOT}/usr/include/aarch64-linux-android"
        "-I${ANDROID_NDK_SYSROOT}/usr/include/c++/v1"
    )
endif()

find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv3-lib GLESv3)
find_library(log-lib log)

target_link_libraries(mobileclock
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    ${log-lib}
)
