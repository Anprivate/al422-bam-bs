/*
  packet:
  byte 0 - 4:0 - current row, bit 5 - oe inverted, bit 6 - lat inverted, bit 7 - clk inverted
  bytes 1-2 - OE active counter preload (16 bit, intel order)
  bytes 3-4 - OE passive counter preload (16 bit, intel order)
  bytes 5-n - output RGB data. 5:0 - rgb_data as is, 6 - end of block, 7 - end of frame (address counter reset)
*/

#include "LED_PANEL_BS.h"
#include "gamma.h"

boolean LED_PANEL::begun;
volatile boolean LED_PANEL::dma_is_free;
timer_dev * LED_PANEL::timer_clk_dev;
uint8_t LED_PANEL::timer_clk_ch;
uint8_t LED_PANEL::last_we_pin;

LED_PANEL::LED_PANEL(uint16_t width, uint16_t height, uint8_t scan_lines, uint8_t RGB_inputs, uint8_t we_pin) : Adafruit_GFX(width, height) {
  // copy data to local variables
  _width = width;
  _height = height;
  _scan_lines = scan_lines;
  _RGB_inputs = RGB_inputs;
  _we_pin = we_pin;

  oe_prescaler = 8;

  // calculations
  numLEDs = _width * _height;

  segmentHeight = _scan_lines * _RGB_inputs;
  segmentNum = _height / segmentHeight;

  bytes_in_load_line = _width * segmentNum;
  num_of_load_lines = (uint16_t) _scan_lines * bit_per_pixel;

  numBytes = (bytes_in_load_line + header_size) * num_of_load_lines;
  pixels = (uint8_t *) malloc(numBytes);

  WriteRowHeaders();
  WriteRowTails();
}

LED_PANEL::~LED_PANEL() {
  if (pixels)
    free(pixels);
}

// Expand 16-bit input color (Adafruit_GFX colorspace) to 24-bit (NeoPixel)
// (w/gamma adjustment)
static uint32_t expandColor(uint16_t color) {
  return ((uint32_t)pgm_read_byte(&gamma5[ color >> 11       ]) << 16) |
         ((uint32_t)pgm_read_byte(&gamma6[(color >> 5) & 0x3F]) <<  8) |
         pgm_read_byte(&gamma5[ color       & 0x1F]);
}

void LED_PANEL::WriteRowHeaders(void) {
  uint8_t * tmp_ptr = pixels;

  uint8_t cur_row = _scan_lines - 1;
  uint16_t max_weight = 0x0001 << (bit_per_pixel - 1);
  uint16_t cur_weight = max_weight;

  for (uint16_t i = 0; i < num_of_load_lines; i++) {
    *tmp_ptr++ = cur_row | out_signals_phases;

    uint16_t oe_active = oe_prescaler * cur_weight - 1;
    *tmp_ptr++ = oe_active;
    *tmp_ptr++ = oe_active >> 8;;

    uint16_t oe_inactive = 10;
    *tmp_ptr++ = oe_inactive;
    *tmp_ptr = oe_inactive >> 8;

    tmp_ptr += (bytes_in_load_line + 1);

    if (cur_weight == max_weight) {
      cur_row++;
      if (cur_row >= _scan_lines) cur_row = 0;
      cur_weight = 0x0001;
    } else {
      cur_weight <<= 1;
    }
  }
}

void LED_PANEL::WriteRowTails(void) {
  uint8_t * tmp_ptr = pixels - 1;
  for (uint16_t i = 0; i < num_of_load_lines; i++) {
    tmp_ptr += (bytes_in_load_line + header_size);
    uint8_t tmp_byte = * tmp_ptr;
    tmp_byte &= ~(1 << 7);
    tmp_byte |= (1 << 6);
    if (i == (num_of_load_lines - 1))
      tmp_byte |= (1 << 7);
    * tmp_ptr = tmp_byte;
  }
}

void LED_PANEL::clear(void) {
  memset(pixels, 0x00, numBytes);

  WriteRowHeaders();
  WriteRowTails();
}

void LED_PANEL::setPixelColor(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t tmp_color = (uint32_t) r << 16 + (uint32_t) g << 8 + (uint32_t) b;
  drawPixel(x, y, tmp_color);
}

void LED_PANEL::drawPixel(int16_t x, int16_t y, uint16_t color) {

  if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;

  uint8_t row_num = y % _scan_lines;
  uint16_t tmp_y = y / _scan_lines;

  boolean upper_color = false;
  if (_RGB_inputs == 2) {
    if (tmp_y & 0x0001) upper_color = true;
    tmp_y >>= 1;
  }

  uint16_t segment_num = tmp_y;

  uint8_t * tmp_ptr = pixels + x + _width * segment_num + (bytes_in_load_line + header_size) * row_num * bit_per_pixel +  header_size;

  uint32_t tmp_c = (passThruFlag) ? passThruColor : expandColor(color);
  uint8_t color_mask = (upper_color) ? 0b000111000 : 0b00000111;

  uint16_t max_weight = 0x0001 << bit_per_pixel;

  for (uint16_t weight = 0x0001; weight < max_weight; weight <<= 1) {
    uint8_t tmp_byte = 0x00;
    if (weight & tmp_c) tmp_byte = 0x04;
    if (weight & (tmp_c >> 8)) tmp_byte |= 0x02;
    if (weight & (tmp_c >> 16)) tmp_byte |= 0x01;

    if (upper_color) tmp_byte <<= 3;
    uint8_t out_byte = * tmp_ptr;
    out_byte &= ~color_mask;
    out_byte |= tmp_byte;
    * tmp_ptr = out_byte;
    tmp_ptr += (bytes_in_load_line + header_size);
  }
}

// Pass raw color value to set/enable passthrough
void LED_PANEL::setPassThruColor(uint32_t c) {
  passThruColor = c;
  passThruFlag  = true;
}

// Call without a value to reset (disable passthrough)
void LED_PANEL::setPassThruColor(void) {
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

