# Native-адаптер MobileClock для независимого XamlPreviewer.
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

# ANGLE остаётся NuGet-пакетом, но CMake использует его напрямую: без vcxproj.
if (DEFINED ENV{NUGET_PACKAGES})
    set(mobileclock_nuget_packages "$ENV{NUGET_PACKAGES}")
else()
    set(mobileclock_nuget_packages "$ENV{USERPROFILE}/.nuget/packages")
endif()
set(mobileclock_angle_directory
    "${mobileclock_nuget_packages}/angle.windows.x64/1.0.0")
if (NOT EXISTS "${mobileclock_angle_directory}/build/native/lib/x64/libEGL.lib")
    message(FATAL_ERROR
        "ANGLE.Windows.x64 1.0.0 is required at ${mobileclock_angle_directory}")
endif()

target_compile_definitions(mobileclock_preview_plugin PRIVATE
    MOBILECLOCK_XAML_PREVIEWER=1
    MOBILECLOCK_PREVIEW_SOURCE_MARKUP_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI"
    MOBILECLOCK_PREVIEW_SOURCE_ENTRY_MARKUP_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/UI/Page/MainPage.xaml"
    MOBILECLOCK_PREVIEW_SOURCE_CONTROLS_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.UI/Control"
)
target_include_directories(mobileclock_preview_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${mobileclock_angle_directory}/build/native/include"
)
# OpenGLESRenderer компилируется как самостоятельная static library и не
# наследует include-каталоги потребляющего plugin-а. На desktop его GLES3 API
# предоставляет ANGLE, поэтому задаём заголовки непосредственно этой цели.
target_include_directories(utility_helpers_open_gles_renderer PRIVATE
    "${mobileclock_angle_directory}/build/native/include"
)
target_link_libraries(mobileclock_preview_plugin PRIVATE
    mobileclock_application
    mobileclock_presentation
    mobileclock_ui
    utility_helpers::new
    utility_helpers::xaml_runtime
    utility_helpers::logging
    utility_helpers::open_gles_renderer
    "${mobileclock_angle_directory}/build/native/lib/x64/libEGL.lib"
    "${mobileclock_angle_directory}/build/native/lib/x64/libGLESv2.lib"
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
    message(FATAL_ERROR "MSBuild.exe is required to build XamlPreviewer.WPF")
endif()
add_custom_target(mobileclock_xaml_previewer_host
    COMMAND "${mobileclock_msbuild_executable}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../XamlPreviewer/XamlPreviewer.WPF/XamlPreviewer.WPF.csproj"
        /t:Build
        "/p:Configuration=$<CONFIG>"
        /p:Platform=x64
    USES_TERMINAL
)
add_dependencies(mobileclock_preview_plugin mobileclock_xaml_previewer_host)

# При F5 для CMake target-а Visual Studio не пытается выполнить DLL напрямую.
# Вместо этого она запускает общий WPF-хост и передаёт ему свежую DLL через --plugin.
# Аргумент имеет приоритет над сохранённым вручную выбором plugin-а в настройках Previewer.
set(mobileclock_xaml_previewer_output_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../XamlPreviewer/!VS_TMP/Build/$<CONFIG>/x64/XamlPreviewer.WPF")
set_target_properties(mobileclock_preview_plugin PROPERTIES
    VS_DEBUGGER_COMMAND
        "${mobileclock_xaml_previewer_output_directory}/XamlPreviewer.exe"
    VS_DEBUGGER_COMMAND_ARGUMENTS
        "--plugin \"$<TARGET_FILE:mobileclock_preview_plugin>\""
    VS_DEBUGGER_WORKING_DIRECTORY
        "${mobileclock_xaml_previewer_output_directory}"
)

# Плагин сообщает Previewer-у resourceRootRelativePath = Resources.
# Поэтому после каждой сборки формируем этот каталог рядом с выбранной DLL.
add_custom_command(TARGET mobileclock_preview_plugin POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../MobileClock.Application/Resources"
        "$<TARGET_FILE_DIR:mobileclock_preview_plugin>/Resources"
)

# ANGLE загружается Windows-лоадером из каталога DLL, поэтому кладём runtime туда же.
foreach (mobileclock_angle_runtime_file IN ITEMS libEGL.dll libGLESv2.dll z.dll)
    add_custom_command(TARGET mobileclock_preview_plugin POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${mobileclock_angle_directory}/runtimes/win-x64/native/${mobileclock_angle_runtime_file}"
            "$<TARGET_FILE_DIR:mobileclock_preview_plugin>/${mobileclock_angle_runtime_file}"
    )
endforeach()