cmake_minimum_required(VERSION 3.22.1)
project(PageTransitionsTests LANGUAGES CXX)
set(helpers "${CMAKE_CURRENT_LIST_DIR}/../../../UtilityHelpersLib")
add_subdirectory("${helpers}/3rdParty/Spdlog/Spdlog.Shared" spdlog)
add_subdirectory("${helpers}/Helpers/Helpers.Logging" logging)
add_subdirectory("${helpers}/NugetProjects/XamlRuntime/Nuget/XamlRuntime" runtime)
add_subdirectory("${helpers}/NugetProjects/XamlRuntime/Nuget/XamlCompiler" compiler)
file(GLOB_RECURSE page_transition_test_source_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
)
file(GLOB_RECURSE page_transition_test_xaml_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Application/UI/Page/Example/Page/*.xaml"
)
set(page_transition_test_generated_files)
foreach (page_transition_test_xaml_file IN LISTS page_transition_test_xaml_files)
    get_filename_component(page_transition_test_name "${page_transition_test_xaml_file}" NAME)
    set(page_transition_test_generated_file "${CMAKE_CURRENT_BINARY_DIR}/${page_transition_test_name}.cpp")
    add_custom_command(OUTPUT "${page_transition_test_generated_file}"
        "${CMAKE_CURRENT_BINARY_DIR}/${page_transition_test_name}.h"
        COMMAND XamlCompiler "${page_transition_test_xaml_file}" "${page_transition_test_generated_file}"
            --xaml-namespace "urn:mobileclock:xaml"
        DEPENDS XamlCompiler "${page_transition_test_xaml_file}"
        VERBATIM)
    list(APPEND page_transition_test_generated_files
        "${page_transition_test_generated_file}"
        "${CMAKE_CURRENT_BINARY_DIR}/${page_transition_test_name}.h"
    )
endforeach()
file(GLOB_RECURSE page_transition_presentation_source_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Presentation/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Presentation/*.h"
)
add_executable(PageTransitionsTests
    ${page_transition_test_source_files}
    ${page_transition_test_generated_files}
)
target_include_directories(PageTransitionsTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../.." "${CMAKE_CURRENT_LIST_DIR}/../..")
target_sources(PageTransitionsTests PRIVATE ${page_transition_presentation_source_files})
target_link_libraries(PageTransitionsTests PRIVATE utility_helpers::xaml_runtime)
target_compile_features(PageTransitionsTests PRIVATE cxx_std_20)
enable_testing()
add_test(NAME PageTransitions COMMAND PageTransitionsTests)