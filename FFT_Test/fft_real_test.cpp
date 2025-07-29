
#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <fstream>
#include <iomanip>


#define N               2048
#define SAMPLE_RATE     16384

int main(int argc, char* argv[]){

    // Open file
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file_path>" << std::endl;
        return 1; // Indicate an error
    }

    std::string filePath = argv[1];

    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
       std::cerr << "Error opening file." << std::endl;
       return 1; // Or handle the error appropriately
    }

    // --- Critical Performance Optimizations ---
    // 2. Disable synchronization with C standard streams
    std::ios_base::sync_with_stdio(false);
    // 3. Untie cin from cout. For file streams, this is less critical
    //    but good practice in I/O intensive apps.
    inputFile.tie(nullptr);
    // -----------------------------------------

    /* FFT */
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

    // --- 3. Create an FFTW Plan ---
    // A plan is a precomputed set of steps FFTW will take to compute the transform.
    // For a 1D real-to-complex transform:
    // - N: The logical size of the real input array.
    // - in: Pointer to the input array.
    // - out: Pointer to the output array.
    // - FFTW_ESTIMATE: A flag indicating how much effort to spend on finding an optimal plan.
    //                  FFTW_MEASURE is slower to plan but often faster to execute.
    //                  FFTW_PATIENT or FFTW_EXHAUSTIVE are even more so.
    fftw_plan plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_MEASURE);
    if (!plan) {
        std::cerr << "Error: fftw_plan_dft_r2c_1d failed." << std::endl;
        fftw_free(in);
        fftw_free(out);
        return 1;
    }

    // Populate input array with data from file
    double current_number;

    for(int i = 0; i < N; i++){
        inputFile >> current_number;
        in[i] = current_number;

        // std::cout << std::fixed << std::setprecision(5) << "INPUT " << i << ", " << in[i] << std::endl;
    }

    // --- 4. Execute the FFT Plan ---
    for(int i = 0; i < 50; i++){
        std::cout<< "INPUT: " << in[i] <<std::endl;
    }

    fftw_execute(plan);

    // Creata file to store data
    std::string filename = "magnitude" + filePath;
    std::ofstream outfile(filename);

    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "' for writing." << std::endl;
        return false;
    }

    // --- 5. Process and Display the Output ---
    // The output 'out' contains N/2 + 1 complex numbers.
    // out[i][0] is the real part of the i-th component.
    // out[i][1] is the imaginary part of the i-th component.
    std::cout << "FFT Output (Complex Numbers):" << std::endl;
    for (int i = 0; i < 1000; ++i) {
        std::cout << std::fixed << std::setprecision(5) << "Frequency bin " << i << ": "
                  << out[i][0] << " + " << out[i][1] << "i" << std::endl;
    }
    std::cout << std::endl;

    // Calculate and display magnitudes (and phases if needed)
    double* magnitude = new double[N_out];
    std::cout << "FFT Output Magnitudes:" << std::endl;
    for (int i = 0; i < N_out; ++i) {
        double real_part = out[i][0];
        double imag_part = out[i][1];
        magnitude[i] = std::sqrt(real_part * real_part + imag_part * imag_part);

        //Print Output
        // std::cout << std::fixed << std::setprecision(5) << "Magnitude bin " << i << ", " << magnitude[i] << std::endl;

        //Log Output
        outfile << std::fixed << std::setprecision(5) << magnitude[i] << "\n";
    }

    outfile.close();


    // --- 6. Clean Up ---
    // Destroy the plan and free the memory allocated by FFTW
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    delete[] magnitude;

    std::cout << "\nFFTW execution complete." << std::endl;
}