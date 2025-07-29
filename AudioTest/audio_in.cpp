#include "RtAudio.h"
#include <iostream>
#include <vector>
#include <cstdlib> // For std::exit
#include <csignal> // For signal handling (Ctrl+C)
#include <mutex>
#include <algorithm>
#include <fstream>      // For std::ofstream

// Global RtAudio object and flag to keep running
RtAudio adc;
bool keepRunning = true;
volatile int new_data = 0;
std::mutex buffer_mutex;
std::vector<float> buffer(2048);


// Ctrl+C signal handler
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

// This is your audio callback function.
// It's called by RtAudio whenever a new buffer of audio data is available.
// ! IMPORTANT: Do not do heavy processing or blocking operations directly in this callback.
// ! For complex processing (like FFT), signal another thread or add data to a queue.
int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData) {
    if (status) {
        std::cerr << "Stream overflow detected!" << std::endl;
    }

    // Cast the input buffer to the data type you expect
    // Common formats: float (RTAUDIO_FLOAT32), short (RTAUDIO_SINT16)
    // Make sure this matches the format you open the stream with.
    float *input = static_cast<float*>(inputBuffer);

    // --- Your audio processing would go here ---
    // std::cout << "Received " << nBufferFrames << " frames. First sample: " << input[0] << std::endl;

    // Copy the data to a shared buffer
    buffer_mutex.lock();
    std::copy(&input[0], &input[nBufferFrames], buffer.begin());
    buffer_mutex.unlock();

    new_data = 1;

    if (!keepRunning) {
        return 1; // Signal RtAudio to stop the stream from the callback
    }
    return 0; // Continue streaming
}

int main() {

    signal(SIGINT, signalHandler);

    unsigned int num_devs = adc.getDeviceCount();
    if (num_devs < 1) {
        std::cerr << "No audio devices found!" << std::endl;
        return 1;
    }

    // Get the list of device IDs
    std::vector< unsigned int > ids = adc.getDeviceIds();
    if ( ids.size() == 0 ) {
        std::cout << "No devices found." << std::endl;
        return 0;
    }

    std::cout << "Available audio devices:\n";
    RtAudio::DeviceInfo info;
    for (unsigned int n=0; n<ids.size(); n++ ) {
        info = adc.getDeviceInfo(ids[n]);
        std::cout << "Device ID " << n << ": " << info.name;
        if (info.isDefaultInput) std::cout << " (Default Input)";
        if (info.isDefaultOutput) std::cout << " (Default Output)";
        std::cout << " - Input Channels: " << info.inputChannels;
        std::cout << " - Output Channels: " << info.outputChannels << std::endl;
    }

    // Select Input device
    unsigned int in_dev;
    std::cout << "\nEnter the Device ID for your microphone: ";
    std::cin >> in_dev;

    if (in_dev >= num_devs) {
        std::cerr << "Invalid device ID." << std::endl;
        return 1;
    }

    RtAudio::DeviceInfo selectedDeviceInfo;

    selectedDeviceInfo = adc.getDeviceInfo(ids[in_dev]);
    if (selectedDeviceInfo.inputChannels == 0) {
        std::cerr << "Selected device has no input channels!" << std::endl;
        return 1;
    }

    RtAudio::StreamParameters parameters;
    parameters.deviceId = ids[in_dev];
    parameters.nChannels = 1;        // We'll ask for 1 channel (mono)
    parameters.firstChannel = 0;     // Start with the first channel on the device

    unsigned int sampleRate   = 44100; // Common sample rate
    unsigned int bufferFrames = 2048; // Number of frames per buffer (chunk size)
                                     // Smaller = lower latency, higher CPU
                                     // Larger = higher latency, lower CPU

    // User data can be a pointer to anything you want to access in the callback
    // For example, a struct or class instance holding your processing buffers/state.
    // For now, we'll pass nullptr.
    void *userData = nullptr;
    // std::vector<float> myDataBuffer; // Example if you want to pass a buffer
    // userData = &myDataBuffer;

    // Creata file to store data
    const char* filename = "floats_binary.txt";
    std::ofstream outfile(filename);

    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "' for writing." << std::endl;
        return false;
    }

    // Open the stream
    // Important: Choose a sampleFormat that your device supports.
    // RTAUDIO_FLOAT32 is often good, but RTAUDIO_SINT16 is also common.
    // You might need to query device capabilities if you encounter issues.
    adc.openStream(nullptr,         // Output parameters (nullptr for input-only)
                    &parameters,     // Input parameters
                    RTAUDIO_FLOAT32, // Sample format (try RTAUDIO_SINT16 if float doesn't work)
                    sampleRate,
                    &bufferFrames,   // RtAudio might adjust this to a supported size
                    &audioCallback,
                    userData);       // User data passed to callback

    std::cout << "Streaming audio from: " << selectedDeviceInfo.name << std::endl;
    std::cout << "Actual buffer size: " << bufferFrames << " frames." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    adc.startStream();

    while (keepRunning) {
        
        if(new_data){

            buffer_mutex.lock();

            for (size_t i = 0; i < bufferFrames; ++i) {
                outfile << buffer[i] << "\n";
            }
            buffer_mutex.unlock();
            new_data = 0;
        }

    }

    if (outfile.fail()) {
        std::cerr << "Error: A failure occurred while writing data to '" << filename << "'." << std::endl;
        // outfile.close(); // Destructor will attempt to close on error anyway
        return false;
    }

    // Explicitly closing is good if you want to check its success,
    // but RAII handles it automatically when outfile goes out of scope.
    outfile.close();

    if (outfile.fail()) { // Check if close itself failed (e.g., flushing buffers)
        std::cerr << "Error: A failure occurred while closing file '" << filename << "'." << std::endl;
        return false;
    }

    if (adc.isStreamRunning()) {
        adc.stopStream();
    }

    if (adc.isStreamOpen()) {
        adc.closeStream();
    }

    std::cout << "Program finished." << std::endl;
    return 0;
}