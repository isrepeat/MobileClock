cmake_minimum_required(VERSION 3.22.1)
project(MobileClock LANGUAGES C CXX)

include(cmake/InstallXamlRuntime.cmake)
fn_mobileclock_install_xaml_runtime()

# Короткое имя сохраняет пути к промежуточным объектам ниже MAX_PATH, не
# меняя имя исходной папки MobileClock.AndroidHost.
add_subdirectory(MobileClock.AndroidHost MC.AHost)

# Desktop preview-plugin относится к MobileClock, а не к решению WPF-хоста.
# На Android он не нужен: там используется mobileclock_android_host.
if (NOT ANDROID)
    add_subdirectory(MobileClock.PreviewPlugin)
endif()

if (NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    file(GLOB_RECURSE mobileclock_runtime_markup_test_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/MobileClock.AndroidHost/Tests/RuntimeMarkup/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/MobileClock.AndroidHost/Tests/RuntimeMarkup/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/MobileClock.AndroidHost/Tests/RuntimeMarkup/*.xaml"
    )
    # Тесты собираются явно при работе с ними, но не входят в обычную desktop
    # сборку: F5 Previewer-а не должен зависеть от их отдельного состояния.
    add_executable(RuntimeMarkupTests EXCLUDE_FROM_ALL ${mobileclock_runtime_markup_test_files})
    target_link_libraries(RuntimeMarkupTests PRIVATE mobileclock_application)
    target_compile_features(RuntimeMarkupTests PRIVATE cxx_std_20)
    add_test(NAME RuntimeMarkup COMMAND RuntimeMarkupTests "${CMAKE_CURRENT_SOURCE_DIR}")
endif()