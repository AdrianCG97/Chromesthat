#ifndef _LED_STRIP_H_
#define _LED_STRIP_H_

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
    Pi5NeoCpp(uint32_t num, const std::string& device);

    /**
     * @brief Destructor that cleans up by clearing LEDs and closing the device.
     */
    ~Pi5NeoCpp();

    /**
     * @brief Sets the color of a single pixel in the local buffer.
     * @param index The index of the pixel.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     */
    void set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Sets all pixels to black (off) in the local buffer.
     */
    void clear();
    /**
     * @brief Sends the pixel data to the LED strip.
     */
    void show();
};

#endif // _LED_STRIP_H_