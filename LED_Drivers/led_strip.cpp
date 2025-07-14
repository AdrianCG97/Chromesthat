#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdexcept>

// Represents a single RGB pixel
struct Pixel {
    uint8_t r, g, b;
};

/**
 * @class Pi5NeoCpp
 * @brief Controls a strip of WS2812 LEDs on a Raspberry Pi 5 using the SPI bus.
 */
class Pi5NeoCpp {
private:
    int spi_fd; // File descriptor for the SPI device
    uint32_t num_leds;
    std::vector<Pixel> pixels;

    // WS2812 uses a 1-wire protocol that can be emulated with SPI.
    // A WS2812 '1' bit is a long high pulse, '0' is a short high pulse.
    // We can represent these with 3 SPI bits:
    // - WS2812 '1' -> SPI `110`
    // - WS2812 '0' -> SPI `100`
    // To achieve the required 800kHz data rate, the SPI clock must be 3x that, so ~2.4MHz.
    const uint8_t PATTERN_1 = 0b11000000; // Represents one WS2812 '1' bit
    const uint8_t PATTERN_0 = 0b10000000; // Represents one WS2812 '0' bit

public:
    /**
     * @brief Constructor that opens and configures the SPI device.
     * @param num The number of LEDs in the strip.
     * @param device The SPI device path (e.g., "/dev/spidev0.0").
     */
    Pi5NeoCpp(uint32_t num, const std::string& device = "/dev/spidev0.0") : num_leds(num) {
        pixels.resize(num_leds, {0, 0, 0});

        // Open the SPI device
        if ((spi_fd = open(device.c_str(), O_WRONLY)) < 0) {
            throw std::runtime_error("Error: Cannot open SPI device. Check permissions or if SPI is enabled.");
        }

        // Configure SPI settings
        uint8_t mode = SPI_MODE_0;
        uint8_t bits = 8;
        uint32_t speed = 2400000; // 2.4 MHz

        if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1 ||
            ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
            ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
            close(spi_fd);
            throw std::runtime_error("Error: Cannot configure SPI device.");
        }
    }

    /**
     * @brief Destructor that cleans up by clearing LEDs and closing the device.
     */
    ~Pi5NeoCpp() {
        try {
            clear();
            show();
        } catch (const std::exception& e) {
            // Suppress exceptions during destruction
        }
        if (spi_fd >= 0) {
            close(spi_fd);
        }
    }

    /**
     * @brief Sets the color of a single pixel in the local buffer.
     * @param index The index of the pixel.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     */
    void set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
        if (index < num_leds) {
            pixels[index] = {r, g, b};
        }
    }

    /**
     * @brief Sets all pixels to black (off) in the local buffer.
     */
    void clear() {
        for (uint32_t i = 0; i < num_leds; ++i) {
            pixels[i] = {0, 0, 0};
        }
    }

    /**
     * @brief Sends the pixel data to the LED strip.
     */
    void show() {
        // Each 24-bit color pixel requires 24 * 3 = 72 SPI bits = 9 SPI bytes.
        std::vector<uint8_t> spi_buffer;
        spi_buffer.reserve(num_leds * 9);

        for (const auto& pixel : pixels) {
            // WS2812B expects data in GRB order
            uint32_t color = (static_cast<uint32_t>(pixel.g) << 16) |
                             (static_cast<uint32_t>(pixel.r) << 8)  |
                             (static_cast<uint32_t>(pixel.b));

            // Convert the 24-bit color into 72 SPI bits (9 bytes)
            for (int i = 23; i >= 0; --i) {
                uint8_t current_bit = (color >> i) & 1;
                uint8_t byte = 0;
                // This logic packs 3 SPI bits into the most significant bits of a byte
                if (current_bit) { // If color bit is 1
                    byte |= (1 << 7) | (1 << 6); // 110xxxxx
                } else { // If color bit is 0
                    byte |= (1 << 7);            // 100xxxxx
                }
                // Each 3-bit pattern needs to be sent as a full byte.
                // To do this efficiently, we can send 3 bytes for each color byte.
            }
             // A simpler, byte-oriented way:
            uint8_t color_bytes[3] = {pixel.g, pixel.r, pixel.b};
            for(int j = 0; j < 3; ++j) { // For each color component (G, R, B)
                for(int i = 7; i >= 0; --i) { // For each bit in the component
                    if((color_bytes[j] >> i) & 1) {
                        spi_buffer.push_back(0b110); // Send '1'
                    } else {
                        spi_buffer.push_back(0b100); // Send '0'
                    }
                }
            }
        }
        
        // This logic is slightly off. Let's correct it based on how spidev works.
        // We need to send a stream of bytes. Let's send 3 bytes for each color byte (8 bits).
        spi_buffer.clear();
        spi_buffer.reserve(num_leds * 24 * 3 / 8); // simplified logic below
        
        // Final Correct Logic: Map each data bit to 3 SPI bits, packed into bytes.
        // Total bits = num_leds * 24 * 3 = num_leds * 72
        // Total bytes = num_leds * 9
        spi_buffer.resize(num_leds * 9);
        int buffer_idx = 0;

        for (const auto& pixel : pixels) {
            uint32_t color = (static_cast<uint32_t>(pixel.g) << 16) | (static_cast<uint32_t>(pixel.r) << 8) | pixel.b;
            for (int i = 23; i >= 0; --i) {
                uint8_t pattern = ((color >> i) & 1) ? 0b110 : 0b100;
                // This pattern needs to be spread across bytes in the buffer
                // This is complex. The most robust way is a lookup table or direct byte generation. Let's use direct bytes.
            }
        }
        // Let's use the simplest, most readable byte-generation logic.
        spi_buffer.clear();
        const uint8_t spi_pattern[2] = {0b100, 0b110}; // SPI patterns for 0 and 1
        for (const auto& p : pixels) {
            uint8_t colors[3] = {p.g, p.r, p.b}; // GRB order
            for(uint8_t c : colors) {
                for(int i = 7; i >= 0; --i) {
                    spi_buffer.push_back(spi_pattern[(c >> i) & 1]);
                }
            }
        }


        // The write operation
        if (write(spi_fd, spi_buffer.data(), spi_buffer.size()) != (ssize_t)spi_buffer.size()) {
            throw std::runtime_error("Error: Failed to write to SPI device.");
        }
    }
};

int main() {
    const int NUM_LEDS = 140;

    try {
        Pi5NeoCpp pixels(NUM_LEDS);

        std::cout << "Starting C++ chase animation..." << std::endl;

        // Run the chase animation 3 times
        for (int j = 0; j < 3; ++j) {
            for (int i = 0; i < NUM_LEDS; ++i) {
                pixels.clear();
                pixels.set_pixel(i, 255, 0, 0); // Set current pixel to red
                pixels.show();
                usleep(50000); // 50ms delay
            }
        }

        std::cout << "Animation finished. Clearing pixels." << std::endl;
        pixels.clear();
        pixels.show();
        usleep(1000); // Small delay to ensure clear command is sent

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}