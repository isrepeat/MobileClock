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
set(mobileclock_generated_root "${CMAKE_CURRENT_SOURCE_DIR}/../Build/MobileClock.Native/!Generated")
get_filename_component(mobileclock_generated_parent "${mobileclock_generated_root}" DIRECTORY)
get_filename_component(mobileclock_project_root "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
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
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../UtilityHelpersLib" UtilityHelpersLib)

# Единственная нативная библиотека приложения. Gradle лишь упаковывает готовый
# .so, а компиляция и линковка выполняются этим CMake-проектом.
file(GLOB_RECURSE mobileclock_xaml_source CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/*.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.UI/*.xaml"
)
file(GLOB_RECURSE mobileclock_xaml_generated CONFIGURE_DEPENDS
    "${mobileclock_generated_root}/Xaml/*.xaml.cpp"
)
file(GLOB_RECURSE mobileclock_xaml_generated_headers CONFIGURE_DEPENDS
    "${mobileclock_generated_root}/Xaml/*.xaml.h"
)
# XamlCompilerIgnore.json является единым источником правил для генератора и
# app-target. Генератор оставляет старые артефакты на диске, поэтому CMake
# дополнительно защищает библиотеку от их случайного подключения.
set(mobileclock_xaml_ignore_configuration
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/XamlCompilerIgnore.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${mobileclock_xaml_ignore_configuration}")
file(READ "${mobileclock_xaml_ignore_configuration}"
    mobileclock_xaml_ignore_configuration_json)

function(mobileclock_read_xaml_ignore_list property output)
    string(JSON count LENGTH "${mobileclock_xaml_ignore_configuration_json}" ${property})
    set(values)
    if (count GREATER 0)
        math(EXPR last_index "${count} - 1")
        foreach (index RANGE ${last_index})
            string(JSON value GET "${mobileclock_xaml_ignore_configuration_json}"
                ${property} ${index})
            list(APPEND values "${value}")
        endforeach()
    endif()
    set(${output} "${values}" PARENT_SCOPE)
endfunction()

mobileclock_read_xaml_ignore_list(directories mobileclock_xaml_ignored_directories)
mobileclock_read_xaml_ignore_list(fileSuffixes mobileclock_xaml_ignored_file_suffixes)
foreach (directory IN LISTS mobileclock_xaml_ignored_directories)
    list(FILTER mobileclock_xaml_source EXCLUDE REGEX "/${directory}/")
    list(FILTER mobileclock_xaml_generated EXCLUDE REGEX "/${directory}/")
    list(FILTER mobileclock_xaml_generated_headers EXCLUDE REGEX "/${directory}/")
endforeach()
foreach (suffix IN LISTS mobileclock_xaml_ignored_file_suffixes)
    list(FILTER mobileclock_xaml_source EXCLUDE REGEX "${suffix}\\.xaml$")
    list(FILTER mobileclock_xaml_generated EXCLUDE REGEX "${suffix}\\.xaml\\.cpp$")
    list(FILTER mobileclock_xaml_generated_headers EXCLUDE REGEX "${suffix}\\.xaml\\.h$")
endforeach()
file(GLOB_RECURSE mobileclock_header_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB_RECURSE mobileclock_icon_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/*"
)
file(GLOB_RECURSE mobileclock_shader_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/*"
)
set(mobileclock_resource_files
    ${mobileclock_icon_files}
    ${mobileclock_shader_files}
)
# Заголовки сабмодуля принадлежат его собственным CMake-целям. В цели
# приложения показываем только её собственные исходники.
list(FILTER mobileclock_header_files EXCLUDE REGEX "/UtilityHelpersLib/")
# CMake-кэш и промежуточные результаты сборки не являются исходниками UI.
list(FILTER mobileclock_header_files EXCLUDE REGEX "/Build/")
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
set(mobileclock_application_header_files ${mobileclock_header_files})
list(FILTER mobileclock_application_header_files EXCLUDE REGEX "/MobileClock\\.UI/")
list(FILTER mobileclock_application_header_files EXCLUDE REGEX "/MobileClock\\.UI\\.NativeBridge/")
set(mobileclock_ui_header_files ${mobileclock_application_header_files})
list(FILTER mobileclock_ui_header_files INCLUDE REGEX "/UI/")
set(mobileclock_page_xaml_source ${mobileclock_xaml_source})
list(FILTER mobileclock_page_xaml_source INCLUDE REGEX "/UI/")
set(mobileclock_ui_control_xaml_source ${mobileclock_xaml_source})
list(FILTER mobileclock_ui_control_xaml_source INCLUDE REGEX "/MobileClock\.UI/")
set(mobileclock_page_xaml_generated ${mobileclock_xaml_generated})
list(FILTER mobileclock_page_xaml_generated INCLUDE REGEX "/Pages/")
set(mobileclock_page_xaml_generated_headers ${mobileclock_xaml_generated_headers})
list(FILTER mobileclock_page_xaml_generated_headers INCLUDE REGEX "/Pages/")
set(mobileclock_ui_control_xaml_generated ${mobileclock_xaml_generated})
list(FILTER mobileclock_ui_control_xaml_generated INCLUDE REGEX "/Controls/")
set(mobileclock_ui_control_xaml_generated_headers ${mobileclock_xaml_generated_headers})
list(FILTER mobileclock_ui_control_xaml_generated_headers INCLUDE REGEX "/Controls/")
source_group("Resources\\Icons" FILES ${mobileclock_icon_files})
source_group("Resources\\Shaders" FILES ${mobileclock_shader_files})
source_group("Header Files" FILES ${mobileclock_application_header_files})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/UI" PREFIX "UI" FILES
    ${mobileclock_ui_header_files}
    ${mobileclock_page_xaml_source}
)
source_group("Resources\\Effects" FILES
    Resources/Effects/Effects.cpp
    Resources/Effects/Effects.h
    Resources/Effects/Shaders.cpp
    Resources/Effects/Shaders.h
)
source_group(TREE "${mobileclock_generated_root}/Xaml" PREFIX "!Generated\\Xaml" FILES
    ${mobileclock_xaml_generated}
    ${mobileclock_xaml_generated_headers}
)
source_group("!Generated\\Build" FILES "${mobileclock_build_version_header}")

set(mobileclock_ui_project_files
    ../MobileClock.UI/Controls/AlarmList.h
    ../MobileClock.UI/Controls/AlarmList.xaml
    ../MobileClock.UI/Controls/TimelineTabs.h
    ../MobileClock.UI/Controls/TimelineTabs.xaml
    ../MobileClock.UI/MobileClock.UI.vcxproj
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.UI" PREFIX "MobileClock.UI" FILES
    ${mobileclock_ui_project_files}
    ${mobileclock_ui_control_xaml_source}
)
add_library(mobileclock_ui STATIC
    ../MobileClock.UI/Controls/AlarmList.cpp
    ../MobileClock.UI/Controls/TimelineTabs.cpp
    ${mobileclock_ui_control_xaml_generated}
    ${mobileclock_ui_control_xaml_generated_headers}
)
target_sources(mobileclock_ui PRIVATE
    ${mobileclock_ui_project_files}
)
target_include_directories(mobileclock_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${mobileclock_project_root}"
    "${mobileclock_generated_parent}"
    "${mobileclock_generated_build_dir}"
)
set_target_properties(mobileclock_ui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(mobileclock_ui PUBLIC
    utility_helpers::xaml_runtime
)

add_library(mobileclock SHARED
    main.cpp
    NativeApplication.cpp
    Renderer/AnimationRenderers.cpp
    Renderer/AnimationShaders.cpp
    Resources/Effects/Effects.cpp
    Resources/Effects/Shaders.cpp
    Renderer/AssetsManager.cpp
    Renderer/AndroidApplicationActions.cpp
    Renderer/AndroidCommandDispatcher.cpp
    Renderer/NativeRenderer.cpp
    UI/Pages/MainPageViewModel.cpp
    UI/PageManager.cpp
    UI/Pages/SettingsPageViewModel.cpp
    UI/TouchHandler.cpp
    ${mobileclock_application_header_files}
    ${mobileclock_xaml_source}
    ${mobileclock_resource_files}
    ${mobileclock_page_xaml_generated}
    ${mobileclock_page_xaml_generated_headers}
)
target_include_directories(mobileclock PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${mobileclock_project_root}"
    "${mobileclock_generated_parent}"
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
            "${CMAKE_CURRENT_SOURCE_DIR}/../Build/MobileClock.Native/android/jniLibs/${mobileclock_android_abi}"
    )
endif()

find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv3-lib GLESv3)
find_library(log-lib log)

# Системные Android-библиотеки ищем через NDK, а Logcat-обёртку над spdlog
# берём из UtilityHelpersLib. Локальный spdlog в MobileClock больше не нужен.
target_link_libraries(mobileclock
    mobileclock_ui
    utility_helpers::open_gles_renderer
    utility_helpers::new
    utility_helpers::xaml_runtime
    # В CMake 4.3 Android-модуль не добавляет c++abi при этом standalone-NDK
    # preset. Линкуем рантайм статически, не требуя отдельный libc++_shared.so.
    c++_static
    c++abi
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    ${log-lib}
)