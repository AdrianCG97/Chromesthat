cmake_minimum_required(VERSION 3.10)
project(FFT_Test)

set(CMAKE_CXX_STANDARD 11) # Or newer

# INCLUDE FFTW3
find_package(PkgConfig REQUIRED)
pkg_search_module(FFTW REQUIRED fftw3 IMPORTED_TARGET)
include_directories(PkgConfig::FFTW)
link_libraries(PkgConfig::FFTW)


# add_executable(FFT_Test fft_test.cpp)

# # INCLUDE CMATH
# find_library(MATH_LIBRARY m)
# if(MATH_LIBRARY)
#     target_link_libraries(FFT_Test ${MATH_LIBRARY})
# endif()

# Find the FFTW3 package
# This works because libfftw3-dev installs the necessary CMake find scripts.
# add_subdirectory(fftw_src)
# find_package(FFTW3 REQUIRED)

#add_executable(FFT_Test fft_test.cpp) # Add your source files

# Link your executable against the FFTW library
# FFTW3::fftw3 is the target for the double-precision library.
# Use FFTW3::fftw3f for single-precision floats if you need it.
#target_link_libraries(FFT_Test PRIVATE FFTW3::fftw3)

# You may need to explicitly add the math library link
# target_link_libraries(FFT_Test PRIVATE m)
