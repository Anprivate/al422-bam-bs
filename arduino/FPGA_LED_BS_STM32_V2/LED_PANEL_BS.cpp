/*
  packet:
  byte 0 - 4:0 - current row, bit 5 - oe inverted, bit 6 - lat inverted, bit 7 - clk inverted
  bytes 1-2 - OE active counter preload (16 bit, intel order)
  bytes 3-4 - OE passive counter preload (16 bit, intel order)
  bytes 5-n - output RGB data. 5:0 - rgb_data as is, 6 - end of block, 7 - end of frame (address counter reset)
*/

#include "LED_PANEL_BS.h"
#include "gamma_8_16.h"

boolean LED_PANEL::begun;
volatile boolean LED_PANEL::dma_is_free;
timer_dev * LED_PANEL::timer_clk_dev;
uint8_t LED_PANEL::timer_clk_ch;
uint8_t LED_PANEL::last_we_pin;
uint8_t * LED_PANEL::dma_buffer;

LED_PANEL::LED_PANEL(uint16_t in_width, uint16_t in_height, uint8_t in_scan_lines, uint8_t in_RGB_inputs, uint8_t in_we_pin, uint8_t in_bpp, uint8_t in_bpc) : Adafruit_GFX(in_width, in_height) {
  // copy data to local variables
  _width = in_width;
  _height = in_height;
  _scan_lines = in_scan_lines;
  _RGB_inputs = in_RGB_inputs;
  _we_pin = in_we_pin;

  _bpc = in_bpc; // bits per color
  _bpp = in_bpp; // bytes per pixel

  // main array calculations
  numLEDs = _width * _height;

  numBytes = numLEDs * _bpp;
  pixels = (uint8_t *) malloc(numBytes);

  segmentHeight = _scan_lines * _RGB_inputs;
  segmentNum = _height / segmentHeight;

  leds_in_load_line = _width * segmentNum;

  max_oe_prescaler = round ((4.0 * (float) leds_in_load_line * (float) _bpc) / (float) (0x0001 << _bpc));
  cur_oe_prescaler = max_oe_prescaler;

  inactive_per_line = (uint16_t *) malloc(_bpc * 2);

  for (uint8_t i = 0; i < _bpc; i++)
    inactive_per_line[i] = min_inactive;

  // MaxEfficiency = CalculateEfficiency();

  curr_brightness = 1.0;
}

LED_PANEL::~LED_PANEL() {
  if (pixels)
    free(pixels);
  if (inactive_per_line)
    free(inactive_per_line);
}

void LED_PANEL::setPixelColor8(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  if ((x >= _width) || (y >= _height)) return;
  
  uint8_t * tmp_ptr = pixels + (x + y * _height) * _bpc;
  *tmp_ptr++ = r;
  *tmp_ptr++ = g;
  *tmp_ptr++ = b;
}

void LED_PANEL::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;

  if (passThruFlag) {
    setPixelColor8(x, y, ptc_r, ptc_g, ptc_b);
  } else {
    uint8_t r = (color >> 8) & 0xF8;
    uint8_t g = (color >> 3) & 0xFC;
    uint8_t b = (color << 3) & 0xF8;
    setPixelColor8(x, y, r, g, b);
  }

}


