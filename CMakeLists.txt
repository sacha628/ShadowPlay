cmake_minimum_required(VERSION 3.10)
project(ShadowPlay LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "ShadowPlay is a native Windows application.")
endif()

add_executable(ShadowPlay WIN32 src/main.cpp)
target_compile_definitions(ShadowPlay PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600 WINVER=0x0600)
target_compile_features(ShadowPlay PRIVATE cxx_std_11)
target_link_libraries(ShadowPlay PRIVATE shell32 ole32 user32 gdi32)
