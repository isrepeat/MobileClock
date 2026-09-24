# Native-адаптер MobileClock для независимого AndroidAppPreviewer.
# Результат сборки — пакет: DLL, Resources и runtime-библиотеки ANGLE рядом с ней.
file(GLOB_RECURSE mobileclock_preview_plugin_source_files CONFIGURE_DEPENDS
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
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_SOURCE_DIR}/Build/MobileClock.PreviewPlugin/Build/$<CONFIG>/x64/MobileClock.PreviewPlugin"
)
if (MSVC)
    target_compile_options(mobileclock_preview_plugin PRIVATE /utf-8)
endif()

set(ANDROID_APP_PREVIEWER_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../AndroidAppPreviewer"
    CACHE PATH "Path to the AndroidAppPreviewer repository")

target_compile_definitions(mobileclock_preview_plugin PRIVATE
    MOBILECLOCK_PREVIEW_SOURCE_MARKUP_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI"
    MOBILECLOCK_PREVIEW_SOURCE_ENTRY_MARKUP_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI/Page/MainPage.xaml"
    MOBILECLOCK_PREVIEW_SOURCE_CONTROLS_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.UI/Control"
)
target_include_directories(mobileclock_preview_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(mobileclock_preview_plugin PRIVATE
    mobileclock_application
    mobileclock_presentation
    mobileclock_ui
    utility_helpers::new
    AndroidAppPreviewerPlugin::SDK
    XamlRuntime::Runtime
    XamlRuntime::Logging
    XamlRuntime::OpenGLESRenderer
)

# WPF-хост — внешний готовый инструмент, а не build dependency плагина.
# При F5 Visual Studio запускает его и передаёт свежую DLL через --plugin.
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
# Берём каталог импортированной библиотеки: он принадлежит тому же NuGet-пакету,
# который CMake выбрал для линковки XamlRuntime::OpenGLESRenderer.
get_target_property(mobileclock_angle_runtime_library
    XamlRuntime::OpenGLESRenderer
    IMPORTED_LOCATION)
get_filename_component(mobileclock_angle_runtime_directory
    "${mobileclock_angle_runtime_library}"
    DIRECTORY)
set(mobileclock_angle_z_runtime_file z.dll)
set(mobileclock_angle_runtime_files libEGL.dll libGLESv2.dll)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(mobileclock_angle_z_runtime_file zd.dll)
    # Keep third-party ANGLE PDB deployment disabled until source-level ANGLE
    # debugging is explicitly needed.
    # list(APPEND mobileclock_angle_runtime_files libEGL.pdb libGLESv2.pdb zd.pdb)
endif()
list(APPEND mobileclock_angle_runtime_files ${mobileclock_angle_z_runtime_file})
foreach (mobileclock_angle_runtime_file IN LISTS mobileclock_angle_runtime_files)
    add_custom_command(TARGET mobileclock_preview_plugin POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${mobileclock_angle_runtime_directory}/${mobileclock_angle_runtime_file}"
            "$<TARGET_FILE_DIR:mobileclock_preview_plugin>/${mobileclock_angle_runtime_file}"
    )
endforeach()