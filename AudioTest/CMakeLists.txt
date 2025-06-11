cmake_minimum_required(VERSION 3.10)
project(MyAudioProject)

set(CMAKE_CXX_STANDARD 11) # Or newer

find_package(RtAudio REQUIRED)

add_executable(audioprobe audioprobe.cpp)
target_link_libraries(audioprobe PRIVATE RtAudio::rtaudio pthread)

add_executable(audio_in audio_in.cpp)
target_link_libraries(audio_in PRIVATE RtAudio::rtaudio)