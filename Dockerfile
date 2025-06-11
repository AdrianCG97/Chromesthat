FROM ubuntu:24.04

# Set a working directory
WORKDIR /app

# Install build tools and RtAudio dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    vim \
    wget \
    pkg-config \
    libasound2-dev \
    libfftw3-dev \
    # Add other dependencies if needed, e.g., libjack-jackd2-dev or libpulse-dev
    && rm -rf /var/lib/apt/lists/*

# Clone and build RtAudio
# You can pin to a specific RtAudio version by checking out a tag
RUN git clone https://github.com/thestk/rtaudio.git /app/externals/rtaudio_src
WORKDIR /app/externals/rtaudio_src/build
RUN cmake .. -DRTAUDIO_API_ALSA=ON && \
    make -j $(nproc) && \ 
    make install

# Clone and build FFTW
WORKDIR /app/externals/fftw_src/
ENV FFTW_VERSION=3.3.10
RUN wget http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz
RUN tar -xzf fftw-${FFTW_VERSION}.tar.gz && \
    cd fftw-${FFTW_VERSION} && \
    ./configure --enable-shared && \
    make -j$(nproc) && \
    make install

# Go back to the main app directory
WORKDIR /app

# Copy your C++ project source code
# Assuming your Dockerfile is in the root of your project
COPY . .

# # Build your C++ project
# # Adjust this command based on your project's build system (CMake, Make, etc.)
# # Ensure your CMakeLists.txt or Makefile correctly finds and links RtAudio
# WORKDIR /AudioTest

# RUN mkdir build && cd build && \
#     cmake .. && \
#     make -j$(nproc)