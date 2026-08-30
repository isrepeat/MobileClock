cmake_minimum_required(VERSION 3.22.1)
project("mobileclock")

if (ANDROID)
    # NDK Clang передаёт эти каталоги неявно, но VS 2026 не переносит его
    # implicit include list в CMake IntelliSense. Публикуем их на уровне
    # директории, чтобы это действовало и для подключаемого UtilityHelpersLib,
    # и для всех будущих C++-целей этого Android-проекта.
    set(mobileclock_ndk_toolchain
        "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64")
    set(mobileclock_intellisense_abi "${CMAKE_ANDROID_ARCH_ABI}")
    if (NOT mobileclock_intellisense_abi)
        set(mobileclock_intellisense_abi "${ANDROID_ABI}")
    endif()

    if (mobileclock_intellisense_abi STREQUAL "arm64-v8a")
        set(mobileclock_ndk_sysroot_arch_include "aarch64-linux-android")
    elseif (mobileclock_intellisense_abi STREQUAL "armeabi-v7a")
        set(mobileclock_ndk_sysroot_arch_include "arm-linux-androideabi")
    elseif (mobileclock_intellisense_abi STREQUAL "x86")
        set(mobileclock_ndk_sysroot_arch_include "i686-linux-android")
    elseif (mobileclock_intellisense_abi STREQUAL "x86_64")
        set(mobileclock_ndk_sysroot_arch_include "x86_64-linux-android")
    else()
        message(FATAL_ERROR
            "Unsupported Android ABI for NDK IntelliSense: "
            "${mobileclock_intellisense_abi}")
    endif()

    add_compile_options(
        "SHELL:-I \"${mobileclock_ndk_toolchain}/sysroot/usr/include/c++/v1\""
        "SHELL:-I \"${mobileclock_ndk_toolchain}/lib/clang/18/include\""
        "SHELL:-I \"${mobileclock_ndk_toolchain}/sysroot/usr/include/${mobileclock_ndk_sysroot_arch_include}\""
        "SHELL:-I \"${mobileclock_ndk_toolchain}/sysroot/usr/include\""
    )
endif()
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

    # CMake is the only compiler of this library.  Gradle later packages this
    # ready-made .so from jniLibs instead of launching a second CMake build.
    set_target_properties(mobileclock PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}"
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
