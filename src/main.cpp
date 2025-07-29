#include "RtAudio.h"
#include <fftw3.h>
#include <iostream>
#include <vector>
#include <cstdlib> // For std::exit
#include <csignal> // For signal handling (Ctrl+C)
#include <mutex>
#include <algorithm>
#include <fstream>      // For std::ofstream
#include <memory>
#include <cmath>

#include "led_strip.h"


// Global RtAudio object and flag to keep running
const int FRAMES_PER_BUF = 2048;
const int SAMPLE_RATE = 44100;
RtAudio adc;
bool keepRunning = true;
volatile int new_data = 0;
std::mutex buffer_mutex;
std::vector<float> audio_buffer(FRAMES_PER_BUF);

/* FFT */
double *fft_in;
fftw_complex *fft_out;
fftw_plan plan;
int N_out = FRAMES_PER_BUF / 2 + 1;
std::unique_ptr<double[]> fft_magnitude;

#define MIN_MAGNITUDE 45

/* LED STRIP */
const int num_leds = 48;

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
    std::copy(&input[0], &input[nBufferFrames], audio_buffer.begin());
    buffer_mutex.unlock();

    new_data = 1;

    if (!keepRunning) {
        return 1; // Signal RtAudio to stop the stream from the callback
    }
    return 0; // Continue streaming
}

int fft_init(){
    /* Allocate memory for FFTW arrays */
    fft_magnitude = std::make_unique<double[]>(N_out);

    // For a real input, the input array is of type double
    fft_in = (double*) fftw_malloc(sizeof(double) * FRAMES_PER_BUF);
    if (!fft_in) {
        std::cerr << "Error: fftw_malloc for input array failed." << std::endl;
        return 1;
    }

    // For a real-to-complex transform (DFT_R2C), the output array size is N/2 + 1 complex numbers
    // fftw_complex is typically double[2] (real, imag)
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_out);
    if (!fft_out) {
        std::cerr << "Error: fftw_malloc for output array failed." << std::endl;
        fftw_free(fft_in); // Free previously allocated memory
        return 1;
    }

   /* Create an FFTW Plan */
    // A plan is a precomputed set of steps FFTW will take to compute the transform.
    // For a 1D real-to-complex transform:
    // - N: The logical size of the real input array.
    // - in: Pointer to the input array.
    // - out: Pointer to the output array.
    // - FFTW_ESTIMATE: A flag indicating how much effort to spend on finding an optimal plan.
    //                  FFTW_MEASURE is slower to plan but often faster to execute.
    //                  FFTW_PATIENT or FFTW_EXHAUSTIVE are even more so.
    plan = fftw_plan_dft_r2c_1d(FRAMES_PER_BUF, fft_in, fft_out, FFTW_MEASURE);
    if (!plan) {
        std::cerr << "Error: fftw_plan_dft_r2c_1d failed." << std::endl;
        fftw_free(fft_in);
        fftw_free(fft_out);
        return 1;
    }

    return 1;
}

int fft_calculate_magnitudes(){

    /* Populate input array with data from file */
    buffer_mutex.lock();
    for(int i = 0; i < FRAMES_PER_BUF; i++){
        fft_in[i] = audio_buffer[i];
    }
    buffer_mutex.unlock();

    /* Execute the FFT Plan */
    fftw_execute(plan);

    /* Calculate magnitudes */

    int max_mag = 0;
    int max_idx = 0;

    int min_freq = 70;
    int min_index = (min_freq * FRAMES_PER_BUF) / SAMPLE_RATE;
    
    for (int i = min_index; i < N_out; ++i) {
        double real_part = fft_out[i][0];
        double imag_part = fft_out[i][1];
        fft_magnitude[i] = std::sqrt(real_part * real_part + imag_part * imag_part);

        if(fft_magnitude[i] > max_mag){
            max_mag = fft_magnitude[i];
            max_idx = i;
        }
    }

    if(max_mag < MIN_MAGNITUDE){
        return 0;
    }

    return max_idx;
}

unsigned int selectAudioDevice(){
    
    unsigned int num_devs = adc.getDeviceCount();
    if (num_devs < 1) {
        std::cerr << "No audio devices found!" << std::endl;
        return 0;
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
        return 0;
    }

    return ids[in_dev];
}

int magnitude_to_leds(Pi5NeoCpp &pixels){

    // Find frequency at index with max magnitude


    return 1;

}

const uint8_t notes_RGB[12][3] = {
    {0,   0,   255}, // C
    {0,   128, 255}, // G
    {0,   255, 255}, // D
    {0,   255, 128}, // A
    {0,   255, 0},   // E
    {128, 255, 0},   // B
    {255, 255, 0},   // Gb
    {255, 128, 0},   // Db
    {255, 0,   0},   // Ab
    {255, 0,   128}, // Eb
    {255, 0,   255}, // Bb
    {128, 0,   255}  // F
};

int freq_to_leds(Pi5NeoCpp &pixels, float frequency){

    if(frequency == 0.0){
        return 1;
    }

    // A vector of note names in an octave.
    const std::vector<std::string> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    // The reference frequency for the A4 note.
    const double A4_FREQUENCY = 440.0;
    
    // A4 is MIDI note number 69.
    const int A4_MIDI_NUMBER = 69;

    // Calculate the number of semitones away from A4.
    // The formula is derived from: freq = A4 * 2^(semitones/12)
    double semitones_from_a4 = 12.0 * log2(frequency / A4_FREQUENCY);

    // Calculate the closest MIDI note number.
    int midi_note = static_cast<int>(round(semitones_from_a4)) + A4_MIDI_NUMBER;

    // Determine the octave and the index of the note within the octave.
    // In the MIDI standard, middle C (C4) is note 60. Octave number changes at C.
    int octave = (midi_note / 12) - 1;
    int note_index = midi_note % 12;

    std::string note = noteNames[note_index] + std::to_string(octave);
    std::cout << "Note Detected: " << note << "(freq=" << frequency << ")" << std::endl;

    for (int i = 0; i < num_leds; ++i) {
        pixels.set_pixel(i, notes_RGB[note_index][0], notes_RGB[note_index][1], notes_RGB[note_index][2]);
    }
    pixels.show();
    return 1;

}

int fft_idx_to_note(int fft_idx){

    /* Calculate frequency */
    float freq = (fft_idx * SAMPLE_RATE) / FRAMES_PER_BUF;

    // A vector of note names in an octave.
    const std::vector<std::string> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    // The reference frequency for the A4 note.
    const double A4_FREQUENCY = 440.0;
    
    // A4 is MIDI note number 69.
    const int A4_MIDI_NUMBER = 69;

    // Calculate the number of semitones away from A4.
    // The formula is derived from: freq = A4 * 2^(semitones/12)
    double semitones_from_a4 = 12.0 * log2(freq / A4_FREQUENCY);

    // Calculate the closest MIDI note number.
    int midi_note = static_cast<int>(round(semitones_from_a4)) + A4_MIDI_NUMBER;

    // Determine the octave and the index of the note within the octave.
    // In the MIDI standard, middle C (C4) is note 60. Octave number changes at C.
    int octave = (midi_note / 12) - 1;
    int note_index = midi_note % 12;

    return note_index;

}

int detect_notes(Pi5NeoCpp &pixels){

    bool   notes_detected[12] = {false};
    double notes_magnitude[12] = {0};

    for(int i = 0; i < 12; i++){
        notes_detected[i]  = 0;
        notes_magnitude[i] = 0;
    }

    pixels.clear();

    int min_freq = 70; // 70Hz
    int min_index = (min_freq * FRAMES_PER_BUF) / SAMPLE_RATE;

    // Find what notes are present
    for(int i = min_index; i < N_out; i++){
        if(fft_magnitude[i] > MIN_MAGNITUDE){
            int note_idx = fft_idx_to_note(i);
            notes_detected[note_idx] = true;
            notes_magnitude[note_idx] = fft_magnitude[i];
        }
    }

    // Turn on LEDs
    int leds_per_note = 4;

    for(int note_idx = 0; note_idx < 12; note_idx++){
        if(notes_detected[note_idx]){
            for(int i = 0; i < leds_per_note; i ++){
                pixels.set_pixel((note_idx*leds_per_note)+i, notes_RGB[note_idx][0], notes_RGB[note_idx][1], notes_RGB[note_idx][2]);
            }
        }
    }
    pixels.show();

    return 1;

}

int main() {

    signal(SIGINT, signalHandler);

    unsigned int dev_id = selectAudioDevice();

    // Initialize FFT
    fft_init();

    // Create LED Strip object
    const std::string device = "/dev/spidev0.0";
    Pi5NeoCpp pixels(num_leds, device);

    // Initialize audio Capture
    RtAudio::DeviceInfo selectedDeviceInfo;
    selectedDeviceInfo = adc.getDeviceInfo(dev_id);
    if (selectedDeviceInfo.inputChannels == 0) {
        std::cerr << "Selected device has no input channels!" << std::endl;
        return 1;
    }

    RtAudio::StreamParameters parameters;
    parameters.deviceId = dev_id;
    parameters.nChannels = 1;        // We'll ask for 1 channel (mono)
    parameters.firstChannel = 0;     // Start with the first channel on the device

    unsigned int bufferFrames = FRAMES_PER_BUF; // Number of frames per buffer (chunk size)
                                     // Smaller = lower latency, higher CPU
                                     // Larger = higher latency, lower CPU

    // User data can be a pointer to anything you want to access in the callback
    // For example, a struct or class instance holding your processing buffers/state.
    // For now, we'll pass nullptr.
    void *userData = nullptr;
    // std::vector<float> myDataBuffer; // Example if you want to pass a buffer
    // userData = &myDataBuffer;

    // Open the stream
    // Important: Choose a sampleFormat that your device supports.
    // RTAUDIO_FLOAT32 is often good, but RTAUDIO_SINT16 is also common.
    // You might need to query device capabilities if you encounter issues.
    adc.openStream(nullptr,         // Output parameters (nullptr for input-only)
                    &parameters,     // Input parameters
                    RTAUDIO_FLOAT32, // Sample format (try RTAUDIO_SINT16 if float doesn't work)
                    SAMPLE_RATE,
                    &bufferFrames,   // RtAudio might adjust this to a supported size
                    &audioCallback,
                    userData);       // User data passed to callback

    std::cout << "Streaming audio from: " << selectedDeviceInfo.name << std::endl;
    std::cout << "Actual buffer size: " << bufferFrames << " frames." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    adc.startStream();
    
    std::cout << "Listening to audio..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();
    int cycles_count = 0;
    while (keepRunning) {
        
        if(new_data){

            int max_mag_idx =  fft_calculate_magnitudes();
            detect_notes(pixels);
            // if(max_mag_idx > 0){
            //     new_data = 0;
            //     cycles_count++;

            //     // Find frequency at index with max magnitude
            //     float freq = (max_mag_idx * SAMPLE_RATE) / FRAMES_PER_BUF;
            //     // std::cout << "(Freq, Mag): " << freq << ", " << fft_magnitude[max_mag_idx] << std::endl;
            //     freq_to_leds(pixels, freq);
            // }
            
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_duration = current_time - start_time;
        long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_duration).count();
        if(milliseconds > 1000){
            std::cout << "Cycles per second: " << cycles_count << std::endl;
            cycles_count = 0;
            start_time = current_time;
        }

    }

    if (adc.isStreamRunning()) {
        adc.stopStream();
    }

    if (adc.isStreamOpen()) {
        adc.closeStream();
    }

    pixels.clear();
    pixels.show();
    usleep(1000); // Small delay to ensure clear command is sent

    std::cout << "Program finished." << std::endl;
    return 0;
}