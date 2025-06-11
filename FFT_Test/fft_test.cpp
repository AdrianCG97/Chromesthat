#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <random>


#define N        10000
#define SAMPLE_RATE     1000.0
std::vector<double> signal(N);

int main(){

    // 1. Set up the random number generator
    std::random_device rd{};
    unsigned seed = 1;
    std::mt19937 gen(seed); // Mersenne Twister engine

    // 2. Create a normal distribution
    double mean = 0;
    double stddev = 1;
    std::normal_distribution<double> dist(mean, stddev);

    // Populate buffer
    for(int i = 0; i < N; i++){

        // Add 460Hz wave
        signal[i] = 700 * sin(2 * M_PI * 460 * i);

        // Add 58 wave
        signal[i] += 500 * sin(2 * M_PI * 58 * i / SAMPLE_RATE);

        // Add noise
        signal[i] +=  10 * dist(gen);
    }

    double *in;
    fftw_complex *out;
    fftw_plan p;

    // Allocate memory for FFTW arrays
    // For a real input, the input array is of type double
    in = (double*) fftw_malloc(sizeof(double) * N);
    if (!in) {
        std::cerr << "Error: fftw_malloc for input array failed." << std::endl;
        return 1;
    }

    // For a real-to-complex transform (DFT_R2C), the output array size is N/2 + 1 complex numbers
    // fftw_complex is typically double[2] (real, imag)
    int N_out = N / 2 + 1;
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N_out);
    if (!out) {
        std::cerr << "Error: fftw_malloc for output array failed." << std::endl;
        fftw_free(in); // Free previously allocated memory
        return 1;
    }

    // Copy data from std::vector to FFTW's input array
    for (int i = 0; i < N; ++i) {
        in[i] = signal[i];
    }

    // --- 3. Create an FFTW Plan ---
    // A plan is a precomputed set of steps FFTW will take to compute the transform.
    // For a 1D real-to-complex transform:
    // - N: The logical size of the real input array.
    // - in: Pointer to the input array.
    // - out: Pointer to the output array.
    // - FFTW_ESTIMATE: A flag indicating how much effort to spend on finding an optimal plan.
    //                  FFTW_MEASURE is slower to plan but often faster to execute.
    //                  FFTW_PATIENT or FFTW_EXHAUSTIVE are even more so.
    fftw_plan plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);
    if (!plan) {
        std::cerr << "Error: fftw_plan_dft_r2c_1d failed." << std::endl;
        fftw_free(in);
        fftw_free(out);
        return 1;
    }

    // --- 4. Execute the FFT Plan ---
    fftw_execute(plan);

    // --- 5. Process and Display the Output ---
    // The output 'out' contains N/2 + 1 complex numbers.
    // out[i][0] is the real part of the i-th component.
    // out[i][1] is the imaginary part of the i-th component.
    std::cout << "FFT Output (Complex Numbers):" << std::endl;
    for (int i = 0; i < N_out; ++i) {
        std::cout << "Frequency bin " << i << ": "
                  << out[i][0] << " + " << out[i][1] << "i" << std::endl;
    }
    std::cout << std::endl;

    // Optional: Calculate and display magnitudes (and phases if needed)
    std::cout << "FFT Output Magnitudes:" << std::endl;
    for (int i = 0; i < N_out; ++i) {
        double real_part = out[i][0];
        double imag_part = out[i][1];
        double magnitude = std::sqrt(real_part * real_part + imag_part * imag_part);
        std::cout << "Magnitude bin " << i << ": " << magnitude << std::endl;
    }

    // --- 6. Clean Up ---
    // Destroy the plan and free the memory allocated by FFTW
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    std::cout << "\nFFTW execution complete." << std::endl;
}