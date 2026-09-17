# Native-адаптер MobileClock для независимого AndroidAppPreviewer.
# Результат сборки — пакет: DLL, Resources и runtime-библиотеки ANGLE рядом с ней.
file(GLOB mobileclock_preview_plugin_source_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB_RECURSE mobileclock_preview_plugin_resource_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/Resources/*"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${mobileclock_preview_plugin_source_files}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/Resources"
    PREFIX "Resources"
    FILES ${mobileclock_preview_plugin_resource_files}
)

add_library(mobileclock_preview_plugin SHARED
    ${mobileclock_preview_plugin_source_files}
    ${mobileclock_preview_plugin_resource_files}
)
set_target_properties(mobileclock_preview_plugin PROPERTIES
    OUTPUT_NAME MobileClock.PreviewPlugin
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/!VS_TMP/Build/$<CONFIG>/x64/MobileClock.PreviewPlugin"
)

set(ANDROID_APP_PREVIEWER_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../AndroidAppPreviewer"
    CACHE PATH "Path to the AndroidAppPreviewer repository")
set(mobileclock_android_app_previewer_project
    "${ANDROID_APP_PREVIEWER_ROOT}/AndroidAppPreviewer.WPF/AndroidAppPreviewer.WPF.csproj")
set(mobileclock_android_app_previewer_sdk_header
    "${ANDROID_APP_PREVIEWER_ROOT}/AndroidAppPreviewer.SDK/include/AndroidAppPreviewerPlugin.h")
if (NOT EXISTS "${mobileclock_android_app_previewer_project}")
    message(FATAL_ERROR
        "AndroidAppPreviewer.WPF was not found. Set ANDROID_APP_PREVIEWER_ROOT to the AndroidAppPreviewer repository.")
endif()
if (NOT EXISTS "${mobileclock_android_app_previewer_sdk_header}")
    message(FATAL_ERROR
        "AndroidAppPreviewer SDK contract was not found at ${mobileclock_android_app_previewer_sdk_header}")
endif()

target_compile_definitions(mobileclock_preview_plugin PRIVATE
    MOBILECLOCK_XAML_PREVIEWER=1
    MOBILECLOCK_PREVIEW_SOURCE_MARKUP_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI"
    MOBILECLOCK_PREVIEW_SOURCE_ENTRY_MARKUP_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI/Page/MainPage.xaml"
    MOBILECLOCK_PREVIEW_SOURCE_CONTROLS_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.UI/Control"
)
target_include_directories(mobileclock_preview_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${ANDROID_APP_PREVIEWER_ROOT}/AndroidAppPreviewer.SDK/include"
)
target_link_libraries(mobileclock_preview_plugin PRIVATE
    mobileclock_application
    mobileclock_presentation
    mobileclock_ui
    utility_helpers::new
    utility_helpers::xaml_runtime
    utility_helpers::logging
    utility_helpers::open_gles_renderer
)

# WPF-хост не входит в CMake-граф, поэтому вызываем его обычную MSBuild-сборку
# как зависимость plugin-а. Благодаря этому F5 актуализирует и DLL, и Previewer.
find_program(mobileclock_msbuild_executable
    NAMES MSBuild.exe
    HINTS
        "$ENV{VSINSTALLDIR}/MSBuild/Current/Bin"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin"
)
if (NOT mobileclock_msbuild_executable)
    message(FATAL_ERROR "MSBuild.exe is required to build AndroidAppPreviewer.WPF")
endif()
add_custom_target(mobileclock_android_app_previewer_host
    COMMAND "${mobileclock_msbuild_executable}"
        "${mobileclock_android_app_previewer_project}"
        /t:Build
        "/p:Configuration=$<CONFIG>"
        /p:Platform=x64
    USES_TERMINAL
)
add_dependencies(mobileclock_preview_plugin mobileclock_android_app_previewer_host)

# При F5 для CMake target-а Visual Studio не пытается выполнить DLL напрямую.
# Вместо этого она запускает общий WPF-хост и передаёт ему свежую DLL через --plugin.
# Аргумент имеет приоритет над сохранённым вручную выбором plugin-а в настройках Previewer.
set(mobileclock_android_app_previewer_output_directory
    "${ANDROID_APP_PREVIEWER_ROOT}/!VS_TMP/Build/$<CONFIG>/x64/AndroidAppPreviewer.WPF")
set_target_properties(mobileclock_preview_plugin PROPERTIES
    VS_DEBUGGER_COMMAND
        "${mobileclock_android_app_previewer_output_directory}/AndroidAppPreviewer.exe"
    VS_DEBUGGER_COMMAND_ARGUMENTS
        "--plugin \"$<TARGET_FILE:mobileclock_preview_plugin>\""
    VS_DEBUGGER_WORKING_DIRECTORY
        "${mobileclock_android_app_previewer_output_directory}"
)

# Плагин сообщает AndroidAppPreviewer resourceRootRelativePath = Resources.
# Поэтому после каждой сборки формируем этот каталог рядом с выбранной DLL.
add_custom_command(TARGET mobileclock_preview_plugin POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/Resources"
        "$<TARGET_FILE_DIR:mobileclock_preview_plugin>/Resources"
)

# ANGLE загружается Windows-лоадером из каталога DLL, поэтому кладём runtime туда же.
set(mobileclock_angle_runtime_directory
    "${XAML_RUNTIME_PACKAGE_ROOT}/runtimes/win-x64/native/${CMAKE_BUILD_TYPE}")
foreach (mobileclock_angle_runtime_file IN ITEMS libEGL.dll libGLESv2.dll z.dll)
    add_custom_command(TARGET mobileclock_preview_plugin POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${mobileclock_angle_runtime_directory}/${mobileclock_angle_runtime_file}"
            "$<TARGET_FILE_DIR:mobileclock_preview_plugin>/${mobileclock_angle_runtime_file}"
    )
endforeach()