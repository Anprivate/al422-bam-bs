#ifndef FPGA_LED_PANEL
#define FPGA_LED_PANEL

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#include <pins_arduino.h>
#endif

#include <libmaple/dma.h>
#include <Adafruit_GFX.h>

// clk_out is PB0. It is Timer 3 ch 3 PWM out
#define clk_out PB0
// rst_out is PB1
#define rst_out PB1
// data_port
#define data_port GPIOA

class LED_PANEL : public Adafruit_GFX {
  public:
    LED_PANEL (uint16_t width, uint16_t height, uint8_t scan_lines, uint8_t RGB_inputs, uint8_t we_pin, uint8_t in_bpc = 8, uint16_t in_prescaler = 8); // Constructor
    ~LED_PANEL();
    void begin(void);
    void clear(void);
    boolean show(void);
    void show(boolean WaitForFinish);
    void setPixelColor(uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void setPassThruColor(uint16_t r, uint16_t g, uint16_t b);
    void setPassThruColor(void);
    uint16_t Color(uint8_t r, uint8_t g, uint8_t b);
    boolean OutIsFree(void);
    uint8_t * GetArrayAddress(void);
    uint16_t GetArraySize(void);
    float CalculateEfficiency(void);
  private:
    const uint8_t header_size = 5;
    const uint8_t out_signals_phases = 0x20;
    uint16_t _width; //  width of panel (total)
    uint16_t _height; // height of panel (total)
    uint8_t segmentHeight; // height of one segment
    uint8_t segmentNum; // number of segments
    uint8_t _scan_lines; // lines in scan (8/16/32)
    uint8_t _RGB_inputs; // RGB inputs (1 or 2)
    uint8_t _we_pin; // we out pin
    uint8_t bpc;
    uint16_t numLEDs;   // number of LEDs on panel
    uint16_t bytes_in_load_line; // bytes in one load line (clear)
    uint16_t num_of_load_lines; // number of load lines
    uint16_t numBytes;  // number of bytes in buffer
    uint16_t oe_prescaler;
    uint16_t min_inactive = 2;
    uint16_t ptc_r, ptc_g, ptc_b;
    boolean  passThruFlag = false;
    float MaxEfficiency;
    uint8_t * pixels;    // pixels data
    // private functions
    void WriteRowHeaders(void);
    void WriteRowTails(void);
    // static variables and functions
    static boolean begun;
    static volatile boolean dma_is_free;
    static timer_dev * timer_clk_dev;
    static uint8_t timer_clk_ch;
    static uint8_t last_we_pin;
    static void static_begin(uint8_t we_pin);
    static void static_start_transfer(uint8_t we_pin, uint8_t * pixels_array, uint16_t data_size);
    static void static_on_full_transfer(void);
};

#endif // FPGA_LED_PANEL

