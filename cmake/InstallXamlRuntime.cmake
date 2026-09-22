include("${CMAKE_CURRENT_LIST_DIR}/NuGetSource.cmake")

function(fn_mobileclock_find_latest_xaml_runtime_package packages_root package_name output_variable)
    # Пакеты NuGet располагаются в отдельных папках <имя>.<версия>.
    # Берём наиболее новую только при наличии стандартного CMake-контракта.
    file(GLOB mobileclock_xaml_runtime_package_candidates LIST_DIRECTORIES true
        "${packages_root}/${package_name}.*")
    list(SORT mobileclock_xaml_runtime_package_candidates COMPARE NATURAL ORDER DESCENDING)
    foreach (mobileclock_xaml_runtime_package_candidate IN LISTS mobileclock_xaml_runtime_package_candidates)
        set(mobileclock_xaml_runtime_config_directory
            "${mobileclock_xaml_runtime_package_candidate}/build/native/cmake")
        if (EXISTS "${mobileclock_xaml_runtime_config_directory}/${package_name}Config.cmake")
            set(${output_variable} "${mobileclock_xaml_runtime_config_directory}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${output_variable} "" PARENT_SCOPE)
endfunction()

function(fn_mobileclock_install_xaml_runtime)
    set(mobileclock_xaml_runtime_package_name XamlRuntime)
    set(mobileclock_xaml_runtime_packages_root "${CMAKE_SOURCE_DIR}/Build/MobileClock/NuGetPackages")
    # Без -Version NuGet устанавливает последнюю доступную версию пакета.
    # Выполняем install при каждой конфигурации: уже скачанная старая версия
    # не должна блокировать получение нового пакета из локального feed-а.
    find_program(mobileclock_nuget_executable NAMES nuget.exe REQUIRED)
    execute_process(
        COMMAND "${mobileclock_nuget_executable}" install "${mobileclock_xaml_runtime_package_name}"
            -Source "${MOBILECLOCK_NUGET_SOURCE}"
            -OutputDirectory "${mobileclock_xaml_runtime_packages_root}"
            -NonInteractive
        COMMAND_ERROR_IS_FATAL ANY
    )
    fn_mobileclock_find_latest_xaml_runtime_package(
        "${mobileclock_xaml_runtime_packages_root}"
        "${mobileclock_xaml_runtime_package_name}"
        mobileclock_xaml_runtime_config_directory)

    if (NOT mobileclock_xaml_runtime_config_directory)
        message(FATAL_ERROR "NuGet installation did not provide ${mobileclock_xaml_runtime_package_name}Config.cmake.")
    endif()
    # Не позволяем значению от предыдущей конфигурации выбрать другую копию пакета.
    unset(${mobileclock_xaml_runtime_package_name}_DIR CACHE)
    find_package(${mobileclock_xaml_runtime_package_name} CONFIG REQUIRED
        PATHS "${mobileclock_xaml_runtime_config_directory}"
        NO_DEFAULT_PATH
        # The NuGet package is on the Windows host, not in the Android NDK
        # sysroot.  Without this CMake prepends the sysroot during an Android
        # cross-compile and therefore misses the existing Config.cmake file.
        NO_CMAKE_FIND_ROOT_PATH)
endfunction()