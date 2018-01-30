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

LED_PANEL::LED_PANEL(uint16_t width, uint16_t height, uint8_t scan_lines, uint8_t RGB_inputs, uint8_t we_pin, uint8_t in_bpc, uint16_t in_prescaler) : Adafruit_GFX(width, height) {
  // copy data to local variables
  _width = width;
  _height = height;
  _scan_lines = scan_lines;
  _RGB_inputs = RGB_inputs;
  _we_pin = we_pin;

  bpc = in_bpc;

  // calculations
  numLEDs = _width * _height;

  segmentHeight = _scan_lines * _RGB_inputs;
  segmentNum = _height / segmentHeight;

  bytes_in_load_line = _width * segmentNum;
  num_of_load_lines = (uint16_t) _scan_lines * bpc;

  if (in_prescaler == 0) {
    max_oe_prescaler = round ((4.0 * (float) bytes_in_load_line * (float) bpc) / (float) (0x0001 << bpc));
    cur_oe_prescaler = max_oe_prescaler;
  }

  numBytes = (bytes_in_load_line + header_size) * num_of_load_lines;
  pixels = (uint8_t *) malloc(numBytes);

  inactive_per_line = (uint16_t *) malloc(bpc * 2);

  for (uint8_t i = 0; i < bpc; i++)
    inactive_per_line[i] = min_inactive;

  MaxEfficiency = CalculateEfficiency();

  curr_brightness = 1.0;
}

LED_PANEL::~LED_PANEL() {
  if (pixels)
    free(pixels);
  if (inactive_per_line)
    free(inactive_per_line);
}

void LED_PANEL::WriteRowHeaders(void) {
  uint8_t * tmp_ptr = pixels;

  uint8_t cur_row = _scan_lines - 1;
  uint8_t cur_bit = bpc - 1;
  uint16_t max_weight = (0x0001 << (cur_bit));
  uint16_t cur_weight = max_weight;

  for (uint16_t i = 0; i < num_of_load_lines; i++) {
    *tmp_ptr++ = cur_row | out_signals_phases;

    uint16_t oe_active = cur_oe_prescaler * cur_weight - 1;
    *tmp_ptr++ = oe_active;
    *tmp_ptr++ = oe_active >> 8;;

    uint16_t oe_inactive = inactive_per_line[cur_bit] - 1;
    *tmp_ptr++ = oe_inactive;
    *tmp_ptr = oe_inactive >> 8;

    tmp_ptr += (bytes_in_load_line + 1);

    if (cur_weight == max_weight) {
      cur_row++;
      if (cur_row >= _scan_lines) cur_row = 0;
      cur_weight = 0x0001;
      cur_bit = 0x00;
    } else {
      cur_weight <<= 1;
      cur_bit++;
    }
  }
}

void LED_PANEL::WriteRowTails(void) {
  uint8_t * tmp_ptr = pixels - 1;

  for (uint16_t i = 0; i < num_of_load_lines; i++) {
    tmp_ptr += (bytes_in_load_line + header_size);
    uint8_t tmp_byte = * tmp_ptr;
    tmp_byte |= (1 << 6); // set eol flag
    if (i == (num_of_load_lines - 1))
      tmp_byte |= (1 << 7); // set eof flag
    else
      tmp_byte &= ~(1 << 7); // clear eof flag
    * tmp_ptr = tmp_byte;
  }
}

void LED_PANEL::clear(void) {
  memset(pixels, 0x00, numBytes);

  WriteRowHeaders();
  WriteRowTails();
}

void LED_PANEL::setPixelColor16(uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b) {
  if ((x >= _width) || (y >= _height)) return;

  uint8_t row_num = y % _scan_lines;
  uint16_t tmp_y = y / _scan_lines;

  boolean upper_color = false;
  if (_RGB_inputs == 2) {
    if (tmp_y & 0x0001) upper_color = true;
    tmp_y >>= 1;
  }

  uint16_t segment_num = tmp_y;

  uint8_t * tmp_ptr = pixels + header_size + x + _width * segment_num + (bytes_in_load_line + header_size) * row_num * bpc;

  uint8_t color_mask = (upper_color) ? 0b000111000 : 0b00000111;

  for (uint16_t weight = (0x0001 << (16 - bpc)); weight != 0x0000; weight <<= 1) {
    uint8_t tmp_byte = 0x00;
    if (weight & r) tmp_byte |= 0x01;
    if (weight & g) tmp_byte |= 0x02;
    if (weight & b) tmp_byte |= 0x04;

    if (upper_color) tmp_byte <<= 3;
    uint8_t out_byte = * tmp_ptr;
    out_byte &= ~color_mask;
    out_byte |= tmp_byte;
    * tmp_ptr = out_byte;
    tmp_ptr += (bytes_in_load_line + header_size);
  }
}

void LED_PANEL::setPixelColor8(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  if ((x >= _width) || (y >= _height)) return;
  setPixelColor16(x, y, GetGammaCorrected(r), GetGammaCorrected(g), GetGammaCorrected(b));
}

void LED_PANEL::PutPictureRGB565(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height, uint8_t * image) {
  if ((in_x >= _width) || (in_y >= _height)) return;

  uint16_t * in_ptr = (uint16_t *) image;

  for (uint16_t i = 0; i < in_height; i++) {
    uint16_t y = in_y + i;
    if (y >= _height) break;

    uint8_t row_num = y % _scan_lines;
    uint16_t tmp_y = y / _scan_lines;

    boolean upper_color = false;
    if (_RGB_inputs == 2) {
      if (tmp_y & 0x0001) upper_color = true;
      tmp_y >>= 1;
    }

    uint16_t segment_num = tmp_y;

    uint8_t color_mask = (upper_color) ? 0b000111000 : 0b00000111;

    uint8_t * out_ptr = pixels + header_size + in_x + _width * segment_num + (bytes_in_load_line + header_size) * row_num * bpc;

    for (uint16_t weight = (0x0001 << (16 - bpc)); weight != 0x0000; weight <<= 1) {
      uint16_t * in_line_ptr = in_ptr;
      uint8_t * out_line_ptr = out_ptr;

      for (uint16_t j = 0; j < in_width; j++) {
        if ((j + in_x) >= _width) break;

        uint16_t color = *in_line_ptr++;
        uint16_t r = GetGammaCorrected((color >> 8) & 0xF8);
        uint16_t g = GetGammaCorrected((color >> 3) & 0xFC);
        uint16_t b = GetGammaCorrected((color << 3) & 0xF8);

        uint8_t tmp_byte = 0x00;
        if (weight & r) tmp_byte |= 0x01;
        if (weight & g) tmp_byte |= 0x02;
        if (weight & b) tmp_byte |= 0x04;

        if (upper_color) tmp_byte <<= 3;
        uint8_t out_byte = * out_line_ptr;
        out_byte &= ~color_mask;
        out_byte |= tmp_byte;
        * out_line_ptr ++ = out_byte;
      }
      out_ptr += (bytes_in_load_line + header_size);
    }
    in_ptr += in_width;
  }
}

void LED_PANEL::PutPictureRGB888(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height, uint8_t * image) {
  if ((in_x >= _width) || (in_y >= _height)) return;

  uint8_t * in_ptr = image;

  for (uint16_t i = 0; i < in_height; i++) {
    uint16_t y = in_y + i;
    if (y >= _height) break;

    uint8_t row_num = y % _scan_lines;
    uint16_t tmp_y = y / _scan_lines;

    boolean upper_color = false;
    if (_RGB_inputs == 2) {
      if (tmp_y & 0x0001) upper_color = true;
      tmp_y >>= 1;
    }

    uint16_t segment_num = tmp_y;

    uint8_t color_mask = (upper_color) ? 0b000111000 : 0b00000111;

    uint8_t * out_ptr = pixels + header_size + in_x + _width * segment_num + (bytes_in_load_line + header_size) * row_num * bpc;

    for (uint16_t weight = (0x0001 << (16 - bpc)); weight != 0x0000; weight <<= 1) {
      uint8_t * in_line_ptr = in_ptr;
      uint8_t * out_line_ptr = out_ptr;

      for (uint16_t j = 0; j < in_width; j++) {
        if ((j + in_x) >= _width) break;
        uint16_t r = GetGammaCorrected(*in_line_ptr++);
        uint16_t g = GetGammaCorrected(*in_line_ptr++);
        uint16_t b = GetGammaCorrected(*in_line_ptr++);

        uint8_t tmp_byte = 0x00;
        if (weight & r) tmp_byte |= 0x01;
        if (weight & g) tmp_byte |= 0x02;
        if (weight & b) tmp_byte |= 0x04;

        if (upper_color) tmp_byte <<= 3;
        uint8_t out_byte = * out_line_ptr;
        out_byte &= ~color_mask;
        out_byte |= tmp_byte;
        * out_line_ptr ++ = out_byte;
      }
      out_ptr += (bytes_in_load_line + header_size);
    }
    in_ptr += (in_width * 3);
  }
}

inline uint16_t LED_PANEL::GetGammaCorrected(uint8_t c) {
  return pgm_read_word(&gamma8[c]);
}

void LED_PANEL::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;

  if (passThruFlag) {
    setPixelColor16(x, y, ptc_r, ptc_g, ptc_b);
  } else {
    uint8_t r = (color >> 8) & 0xF8;
    uint8_t g = (color >> 3) & 0xFC;
    uint8_t b = (color << 3) & 0xF8;
    setPixelColor8(x, y, r, g, b);
  }
}

// Pass raw color value to set/enable passthrough
void LED_PANEL::setPassThruColor16(uint16_t r, uint16_t g, uint16_t b) {
  ptc_r = r;
  ptc_g = g;
  ptc_b = b;
  passThruFlag  = true;
}

void LED_PANEL::setPassThruColor8(uint8_t r, uint8_t g, uint8_t b) {
  ptc_r = GetGammaCorrected(r);
  ptc_g = GetGammaCorrected(g);
  ptc_b = GetGammaCorrected(b);
  passThruFlag  = true;
}

// Call without a value to reset (disable passthrough)
void LED_PANEL::resetPassThruColor(void) {
  passThruFlag = false;
}

// Downgrade 24-bit color to 16-bit
uint16_t LED_PANEL::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
         (b >> 3);
}

void LED_PANEL::begin(void) {
  static_begin(_we_pin);
}


boolean LED_PANEL::OutIsFree(void) {
  return dma_is_free;
}

boolean LED_PANEL::show(void) {
  if (!dma_is_free) return false;
  static_start_transfer(_we_pin, pixels, numBytes);
  return true;
}

void LED_PANEL::show(boolean WaitForFinish) {
  while (!dma_is_free);
  static_start_transfer(_we_pin, pixels, numBytes);
  if (WaitForFinish) while (!dma_is_free);
}

uint8_t * LED_PANEL::GetArrayAddress(void) {
  return pixels;
}

uint16_t LED_PANEL::GetArraySize(void) {
  return numBytes;
}

float LED_PANEL::CalculateEfficiency(uint16_t in_prescaler, float * fps) {
  if (in_prescaler == 0) in_prescaler = cur_oe_prescaler;

  uint32_t total_oe_dur = ((0x00000001 << bpc) - 1) * in_prescaler;
  uint16_t data_cycle_dur = bytes_in_load_line * 2 + 7;

  uint32_t total_cycle_dur = 0;
  uint16_t weight = 0x0001;

  for (uint8_t i = 0; i < bpc; i++) {
    uint16_t oe_cycle_dur = (weight * in_prescaler) + inactive_per_line[i] + 6;
    uint16_t real_cycle_dur = (oe_cycle_dur > data_cycle_dur) ? oe_cycle_dur : data_cycle_dur;
    total_cycle_dur += real_cycle_dur;
    weight <<= 1;
  }
  float newEfficiency = (float) total_oe_dur / (float) total_cycle_dur;

  if (fps != NULL)
    * fps = 50.0E6 / ((float) total_cycle_dur * _scan_lines);

  return newEfficiency;
}

float LED_PANEL::GetMaxEfficiency(void) {
  return MaxEfficiency;
}

float LED_PANEL::GetMinFPS(void) {
  return minFPS;
}

void LED_PANEL::SetMinFPS(float in_fps) {
  minFPS = in_fps;
}

float LED_PANEL::CalculateMinBrightness(void) {
  uint32_t max_total_cycle_dur = round(50.0E6 / minFPS / _scan_lines);
  uint32_t total_oe_dur = (0x00000001 << bpc) - 1;
  float min_eff = (float) total_oe_dur / (float) max_total_cycle_dur;
  return min_eff / MaxEfficiency;
}

float LED_PANEL::GetBrightness(void) {
  return curr_brightness;
}

float LED_PANEL::SetBrightness(float in_brightness) {
  float minBrightness = CalculateMinBrightness();

  // reset inactive lines
  for (uint8_t i = 0; i < bpc; i++)
    inactive_per_line[i] = min_inactive;

  uint16_t work_prescaler;
  uint32_t total_oe_dur;
  uint32_t total_cycle_dur;

  if (in_brightness > 1.0) in_brightness = 1.0;

  if (in_brightness < minBrightness) {
    work_prescaler = 1;
    total_oe_dur = (uint32_t)((0x00000001 << bpc) - 1) * work_prescaler;
    total_cycle_dur = round(50.0E6 / minFPS / (float) _scan_lines);
  } else {
    float req_efficiency = in_brightness * MaxEfficiency;
    work_prescaler = max_oe_prescaler;
    for (uint16_t tmp_prescaler = max_oe_prescaler; tmp_prescaler > 0; tmp_prescaler--) {
      float calc_eff = CalculateEfficiency(tmp_prescaler);
      if (calc_eff < req_efficiency)
        break;
      work_prescaler = tmp_prescaler;
    }
    total_oe_dur = (uint32_t)((0x00000001 << bpc) - 1) * work_prescaler;
    total_cycle_dur = round((float)total_oe_dur / req_efficiency);
  }

  uint16_t data_cycle_dur = bytes_in_load_line * 2 + 7;

  uint32_t max_oe_dur = (uint32_t)(0x00000001 << (bpc - 1)) * work_prescaler;

  for (uint8_t i = bpc; i != 0; i--) {
    uint16_t curr_oe_driven_cycle = (0x0001 << (i - 1)) * work_prescaler + 8;
    uint16_t real_cycle_dur = (data_cycle_dur > curr_oe_driven_cycle) ? data_cycle_dur : curr_oe_driven_cycle;
    uint32_t tmp_req_cyc_dur = total_cycle_dur / i;
    if (real_cycle_dur > tmp_req_cyc_dur) {
      inactive_per_line[i - 1] = min_inactive;
      total_cycle_dur -= real_cycle_dur;
    } else {
      if (tmp_req_cyc_dur > curr_oe_driven_cycle)
        inactive_per_line[i - 1] = tmp_req_cyc_dur - curr_oe_driven_cycle;
      else
        inactive_per_line[i - 1] = min_inactive;
      total_cycle_dur -= (0x0001 << (i - 1)) * work_prescaler + inactive_per_line[i - 1] + 6;
    }
  }

  cur_oe_prescaler = work_prescaler;

  WriteRowHeaders();

  curr_brightness = CalculateEfficiency() / MaxEfficiency;
  return curr_brightness;
}

uint16_t LED_PANEL::GetPrescaler(void) {
  return cur_oe_prescaler;
}

void LED_PANEL::static_begin(uint8_t we_pin) {
  if (!begun) {
    timer_clk_dev = PIN_MAP[clk_out].timer_device;
    timer_clk_ch = PIN_MAP[clk_out].timer_channel;

    gpio_init(data_port);

    // A0..A7 as outputs
    data_port->regs->CRL = 0x33333333;

    digitalWrite(clk_out, 0);
    pinMode(clk_out, OUTPUT);
    digitalWrite(rst_out, 1);
    pinMode(rst_out, OUTPUT);

    dma_init(DMA1);

    begun = true;
    dma_is_free = true;
  }

  digitalWrite(we_pin, 1);
  pinMode(we_pin, OUTPUT);
}

void LED_PANEL::static_start_transfer(uint8_t we_pin, uint8_t * pixels_array, uint16_t data_size) {
  if (!dma_is_free) return;
  dma_is_free = false;

  const uint16_t arr_val = 24;
  // timer configuration
  timer_set_count(timer_clk_dev, 0);
  timer_set_reload(timer_clk_dev, arr_val - 1);
  timer_set_prescaler(timer_clk_dev, 0);
  timer_set_compare(timer_clk_dev, timer_clk_ch, arr_val / 2);
  timer_cc_set_pol(timer_clk_dev, timer_clk_ch, 1);

  dma_tube_config dma_cfg;

  dma_cfg.tube_dst = &(data_port->regs->ODR);
  dma_cfg.tube_dst_size = DMA_SIZE_8BITS;
  dma_cfg.tube_src = pixels_array;
  dma_cfg.tube_src_size = DMA_SIZE_8BITS;

  dma_cfg.tube_nr_xfers = data_size;
  dma_cfg.tube_flags = DMA_CFG_SRC_INC | DMA_CFG_CMPLT_IE;

  dma_cfg.tube_req_src = DMA_REQ_SRC_TIM3_CH3;
  dma_cfg.target_data = 0;

  dma_tube_cfg(DMA1, DMA_CH2, &dma_cfg);

  dma_set_priority(DMA1, DMA_CH2, DMA_PRIORITY_VERY_HIGH);

  dma_attach_interrupt(DMA1, DMA_CH2, static_on_full_transfer);

  timer_dma_enable_req(timer_clk_dev, timer_clk_ch);
  dma_enable(DMA1, DMA_CH2);

  // we=0
  PIN_MAP[we_pin].gpio_device->regs->BRR = (1U << PIN_MAP[we_pin].gpio_bit);
  // rst=0
  PIN_MAP[rst_out].gpio_device->regs->BRR = (1U << PIN_MAP[rst_out].gpio_bit);
  // clk=1
  PIN_MAP[clk_out].gpio_device->regs->BSRR = (1U << PIN_MAP[clk_out].gpio_bit);
  // clk=0
  PIN_MAP[clk_out].gpio_device->regs->BRR = (1U << PIN_MAP[clk_out].gpio_bit);
  // rst=1
  PIN_MAP[rst_out].gpio_device->regs->BSRR = (1U << PIN_MAP[rst_out].gpio_bit);

  pinMode(clk_out, PWM);
  timer_resume(timer_clk_dev);
  last_we_pin = we_pin;
}

void LED_PANEL::static_on_full_transfer(void) {
  dma_get_irq_cause(DMA1, DMA_CH2);
  dma_disable(DMA1, DMA_CH2);

  timer_pause(timer_clk_dev);

  PIN_MAP[clk_out].gpio_device->regs->BRR = (1U << PIN_MAP[clk_out].gpio_bit);
  pinMode(clk_out, OUTPUT);

  // rst=0
  PIN_MAP[rst_out].gpio_device->regs->BRR = (1U << PIN_MAP[rst_out].gpio_bit);
  // clk=1
  PIN_MAP[clk_out].gpio_device->regs->BSRR = (1U << PIN_MAP[clk_out].gpio_bit);
  // clk=0
  PIN_MAP[clk_out].gpio_device->regs->BRR = (1U << PIN_MAP[clk_out].gpio_bit);
  // rst=1
  PIN_MAP[rst_out].gpio_device->regs->BSRR = (1U << PIN_MAP[rst_out].gpio_bit);

  // we=1
  PIN_MAP[last_we_pin].gpio_device->regs->BSRR = (1U << PIN_MAP[last_we_pin].gpio_bit);

  dma_is_free = true;
}


