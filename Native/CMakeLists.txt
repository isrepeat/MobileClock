cmake_minimum_required(VERSION 3.22.1)
project("mobileclock")

# Версия Android-пакета хранится в одном месте. CMake следит за этим файлом
# и генерирует заголовок для нативного UI при каждом изменении версии.
set(mobileclock_version_file "${CMAKE_CURRENT_SOURCE_DIR}/../version.properties")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${mobileclock_version_file}")
file(STRINGS "${mobileclock_version_file}" mobileclock_version_line
    REGEX "^VERSION_NAME=")
string(REGEX REPLACE "^VERSION_NAME=([^\r\n]+)$" "\\1"
    mobileclock_package_version "${mobileclock_version_line}")
if (NOT mobileclock_package_version)
    message(FATAL_ERROR "VERSION_NAME is missing in ${mobileclock_version_file}")
endif()
set(mobileclock_generated_root "${CMAKE_CURRENT_SOURCE_DIR}/!Generated")
set(mobileclock_generated_build_dir "${mobileclock_generated_root}/Build")
set(mobileclock_build_version_header "${mobileclock_generated_build_dir}/BuildVersion.h")
file(MAKE_DIRECTORY "${mobileclock_generated_build_dir}")
configure_file(BuildVersion.h.in
    "${mobileclock_build_version_header}" @ONLY)

# В пресетах выбран target-specific драйвер NDK, например
# x86_64-linux-android24-clang++.cmd. Поэтому и компилятор, и IntelliSense
# получают один и тот же Android target, sysroot и predefined macros напрямую
# от Clang — без ручного дублирования системных include-путей.
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

    # VS 2026 пока не разбирает часть overloadable/diagnose_if-обёрток Bionic
    # fortify. Отключаем их только в Debug; Release сохраняет защиту NDK.
    add_compile_options(
        $<$<CONFIG:Debug>:-U_FORTIFY_SOURCE>
    )
endif()

# Подключаем только переносимую часть сабмодуля. Windows/WinRT-компоненты
# UtilityHelpersLib сюда намеренно не попадают.
add_subdirectory(UtilityHelpersLib)

# Единственная нативная библиотека приложения. Gradle лишь упаковывает готовый
# .so, а компиляция и линковка выполняются этим CMake-проектом.
file(GLOB mobileclock_xaml_source CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.xaml"
)
file(GLOB mobileclock_xaml_generated CONFIGURE_DEPENDS
    "${mobileclock_generated_root}/Xaml/*.xaml.cpp"
)
file(GLOB mobileclock_xaml_generated_headers CONFIGURE_DEPENDS
    "${mobileclock_generated_root}/Xaml/*.xaml.h"
)
file(GLOB_RECURSE mobileclock_header_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB_RECURSE mobileclock_resource_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*"
)
# Заголовки сабмодуля принадлежат его собственным CMake-целям. В цели
# приложения показываем только её собственные исходники.
list(FILTER mobileclock_header_files EXCLUDE REGEX "/UtilityHelpersLib/")
# CMake-кэш и промежуточные результаты сборки не являются исходниками UI.
list(FILTER mobileclock_header_files EXCLUDE REGEX "/out/")
# Visual Studio может создавать локальный intellisense-probe; он не является
# частью приложения и не должен попадать в CMake source groups или reconfigure.
list(FILTER mobileclock_header_files EXCLUDE REGEX "/intellisense-probe/")
if (NOT mobileclock_xaml_source OR NOT mobileclock_xaml_generated
    OR NOT mobileclock_xaml_generated_headers)
    message(FATAL_ERROR
        "Generated XAML sources are missing. Run Scripts/PowerShell/build-android.ps1 first.")
endif()
set_source_files_properties(${mobileclock_xaml_source} PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(${mobileclock_resource_files} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("UI" FILES ${mobileclock_xaml_source})
source_group("Resources\\Icons" FILES ${mobileclock_resource_files})
source_group("Header Files" FILES ${mobileclock_header_files})
source_group("!Generated\\Xaml" FILES
    ${mobileclock_xaml_generated}
    ${mobileclock_xaml_generated_headers}
)
source_group("!Generated\\Build" FILES "${mobileclock_build_version_header}")

add_library(mobileclock SHARED
    main.cpp
    NativeApplication.cpp
    Renderer/NativeRenderer.cpp
    UI/MainPageViewModel.cpp
    ${mobileclock_header_files}
    ${mobileclock_xaml_source}
    ${mobileclock_resource_files}
    ${mobileclock_xaml_generated}
    ${mobileclock_xaml_generated_headers}
)
target_include_directories(mobileclock PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${mobileclock_generated_build_dir}"
)
# Явно фиксируем стандарт: это избавляет от расхождений между NDK, Ninja и IDE.
set_target_properties(mobileclock PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (ANDROID)
    set(mobileclock_android_abi "${ANDROID_ABI}")
    if (NOT mobileclock_android_abi)
        set(mobileclock_android_abi "${CMAKE_ANDROID_ARCH_ABI}")
    endif()

    # CMake — единственный компилятор библиотеки. Gradle забирает готовый .so
    # из jniLibs и не запускает вторую, независимую CMake-сборку.
    set_target_properties(mobileclock PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/jniLibs/${mobileclock_android_abi}"
    )
endif()

find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv3-lib GLESv3)
find_library(log-lib log)

# Системные Android-библиотеки ищем через NDK, а Logcat-обёртку над spdlog
# берём из UtilityHelpersLib. Локальный spdlog в MobileClock больше не нужен.
target_link_libraries(mobileclock
    utility_helpers::open_gles_renderer
    utility_helpers::xaml_runtime
    utility_helpers::android_logging
    # В CMake 4.3 Android-модуль не добавляет c++abi при этом standalone-NDK
    # preset. Линкуем рантайм статически, не требуя отдельный libc++_shared.so.
    c++_static
    c++abi
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    ${log-lib}
)