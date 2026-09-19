# Runs before project(), including CMake compiler probes. No state under .vs
# is needed. An explicit CMAKE_ANDROID_NDK always takes precedence.
set(CMAKE_SYSTEM_NAME Android)
if(NOT CMAKE_ANDROID_NDK)
    set(_mobileclock_ndk_candidates "$ENV{ANDROID_NDK_HOME}" "$ENV{ANDROID_NDK_ROOT}")
    foreach(_mobileclock_sdk "$ENV{ANDROID_SDK_ROOT}" "$ENV{ANDROID_HOME}" "$ENV{LOCALAPPDATA}/Android/Sdk")
        file(GLOB _mobileclock_sdk_ndks LIST_DIRECTORIES true "${_mobileclock_sdk}/ndk/*")
        list(SORT _mobileclock_sdk_ndks COMPARE NATURAL ORDER DESCENDING)
        list(APPEND _mobileclock_ndk_candidates ${_mobileclock_sdk_ndks})
    endforeach()
    file(GLOB _mobileclock_vs_ndks LIST_DIRECTORIES true
        "$ENV{ProgramFiles}/Android/AndroidNDK/*"
        "$ENV{ProgramFiles} (x86)/Android/AndroidNDK/*")
    list(SORT _mobileclock_vs_ndks COMPARE NATURAL ORDER DESCENDING)
    list(APPEND _mobileclock_ndk_candidates ${_mobileclock_vs_ndks})
    foreach(_mobileclock_ndk IN LISTS _mobileclock_ndk_candidates)
        if(EXISTS "${_mobileclock_ndk}/source.properties" AND
           EXISTS "${_mobileclock_ndk}/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe")
            file(TO_CMAKE_PATH "${_mobileclock_ndk}" CMAKE_ANDROID_NDK)
            set(CMAKE_ANDROID_NDK "${CMAKE_ANDROID_NDK}" CACHE PATH "Android NDK installation")
            break()
        endif()
    endforeach()
endif()
if(NOT EXISTS "${CMAKE_ANDROID_NDK}/source.properties")
    message(FATAL_ERROR "Android NDK was not found. Install the NDK or set ANDROID_NDK_HOME / CMAKE_ANDROID_NDK to its directory.")
endif()