cmake_minimum_required(VERSION 3.22.1)
project(MobileClock LANGUAGES C CXX)

add_subdirectory(MobileClock.AndroidHost)

if (NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    add_executable(RuntimeMarkupTests MobileClock.AndroidHost/Tests/RuntimeMarkup/main.cpp)
    target_link_libraries(RuntimeMarkupTests PRIVATE mobileclock_application)
    target_compile_features(RuntimeMarkupTests PRIVATE cxx_std_20)
    add_test(NAME RuntimeMarkup COMMAND RuntimeMarkupTests "${CMAKE_CURRENT_SOURCE_DIR}")
endif()