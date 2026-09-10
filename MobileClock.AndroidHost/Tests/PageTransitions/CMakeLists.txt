cmake_minimum_required(VERSION 3.22.1)
project(PageTransitionsTests LANGUAGES CXX)
set(helpers "${CMAKE_CURRENT_LIST_DIR}/../../../UtilityHelpersLib")
add_subdirectory("${helpers}/3rdParty/Spdlog/Spdlog.Shared" spdlog)
add_subdirectory("${helpers}/Helpers/Helpers.Logging" logging)
add_subdirectory("${helpers}/NugetProjects/XamlRuntime/Nuget/XamlRuntime" runtime)
add_subdirectory("${helpers}/NugetProjects/XamlRuntime/Nuget/XamlCompiler" compiler)
set(example "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Application/UI/Pages/Examples/Pages/PageTransitions.xaml")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/PageTransitions.xaml.cpp")
add_custom_command(OUTPUT "${generated}" "${CMAKE_CURRENT_BINARY_DIR}/PageTransitions.xaml.h"
    COMMAND XamlCompiler "${example}" "${generated}"
    DEPENDS XamlCompiler "${example}"
    VERBATIM)
set(navigation_example "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Application/UI/Pages/Examples/Pages/NavigationTransition.xaml")
set(navigation_generated "${CMAKE_CURRENT_BINARY_DIR}/NavigationTransition.xaml.cpp")
add_custom_command(OUTPUT "${navigation_generated}" "${CMAKE_CURRENT_BINARY_DIR}/NavigationTransition.xaml.h"
    COMMAND XamlCompiler "${navigation_example}" "${navigation_generated}"
    DEPENDS XamlCompiler "${navigation_example}"
    VERBATIM)
set(panel_example "${CMAKE_CURRENT_LIST_DIR}/../../../MobileClock.Application/UI/Pages/Examples/Pages/PanelHeightAnimation.xaml")
set(panel_generated "${CMAKE_CURRENT_BINARY_DIR}/PanelHeightAnimation.xaml.cpp")
add_custom_command(OUTPUT "${panel_generated}" "${CMAKE_CURRENT_BINARY_DIR}/PanelHeightAnimation.xaml.h"
    COMMAND XamlCompiler "${panel_example}" "${panel_generated}"
    DEPENDS XamlCompiler "${panel_example}"
    VERBATIM)
add_executable(PageTransitionsTests main.cpp "${generated}" "${navigation_generated}" "${panel_generated}")
target_include_directories(PageTransitionsTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../.." "${CMAKE_CURRENT_LIST_DIR}/../..")
target_sources(PageTransitionsTests PRIVATE "../../../MobileClock.Presentation/AnimationRenderers.cpp" "../../../MobileClock.Presentation/Effects/Effects.cpp")
target_link_libraries(PageTransitionsTests PRIVATE utility_helpers::xaml_runtime)
target_compile_features(PageTransitionsTests PRIVATE cxx_std_20)
enable_testing()
add_test(NAME PageTransitions COMMAND PageTransitionsTests)