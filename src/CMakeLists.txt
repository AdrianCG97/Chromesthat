cmake_minimum_required(VERSION 3.10)
project(ChromesthatProject)

set(CMAKE_CXX_STANDARD 14) # Or newer

find_package(RtAudio REQUIRED)

# INCLUDE FFTW3
find_package(PkgConfig REQUIRED)
pkg_search_module(FFTW REQUIRED fftw3 IMPORTED_TARGET)
include_directories(PkgConfig::FFTW)
link_libraries(PkgConfig::FFTW)

add_executable(chromesthat main.cpp led_strip.cpp)
target_link_libraries(chromesthat PRIVATE RtAudio::rtaudio pthread)
