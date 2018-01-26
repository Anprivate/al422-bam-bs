#include "LED_PANEL_BS.h"
#include <libmaple/dma.h>
#include <SPI.h>
#include "SdFat.h"

#define width 64
#define height 32
#define bpp 2

#define scan_lines  16
#define RGB_inputs  2
#define WE_out_pin      PB10
#define WE_out_pin2      PB11

LED_PANEL led_panel = LED_PANEL(width, height, scan_lines, RGB_inputs, WE_out_pin);

#define file_name "dump.bin"
SdFat sd2(2);
const uint8_t SD2_CS = PB12;   // chip select for sd2
File dump_file;
bool no_sd;

uint16_t x_coord = 0;
uint16_t y_coord = 0;
uint16_t preloader = 0;
uint8_t what_write = 0x08;

void setup() {
  Serial.begin(115200);
  pinMode(PC13, OUTPUT);

  led_panel.begin();
  led_panel.clear();

  if (!sd2.begin(SD2_CS, SD_SCK_MHZ(18))) {
    Serial.println("sd init failed");
    no_sd = true;
  }

}

void loop() {
  digitalWrite(PC13, LOW);

  uint8_t br = 0x08;

  led_panel.clear();

  for (uint8_t i = 0; i < 64; i++) {
    led_panel.drawPixel(i, 0, led_panel.Color(0, 0, i * 4));
    led_panel.drawPixel(i, 1, led_panel.Color(0, i * 4, 0));
    led_panel.drawPixel(i, 2, led_panel.Color(i * 4, 0, 0));


  }

  /*
    static uint16_t tmp_x = 0;
    static uint16_t tmp_y = 0;

    led_panel.drawPixel(tmp_x, tmp_y, led_panel.Color(0, 0, 255));

    tmp_x++;
    if (tmp_x >= 64) {
    tmp_x = 0;
    tmp_y++;
    if (tmp_y >= 32) tmp_y = 0;
    }
  */
  /*
      led_panel.drawCircle(16, 16, 10, led_panel.Color(0, 0, br));

      led_panel.setCursor(1, 1);
      led_panel.setTextColor(led_panel.Color(br, 0, 0));
      led_panel.setTextSize(1);
      led_panel.setTextWrap(false);
      led_panel.print("Test");

      led_panel.setCursor(4, 11);
      led_panel.setTextColor(led_panel.Color(0, br, 0));
      led_panel.print("Best");

      led_panel.setCursor(7, 21);
      led_panel.setTextColor(led_panel.Color(br, br, br));
      led_panel.print("Case");
    /*
    /*
      uint8_t * tmp_ptr = led_panel.GetArrayAddress();
      uint16_t tmp_size = led_panel.GetArraySize();

      if (!sd2.exists(file_name)) {
      dump_file = sd2.open(file_name, FILE_WRITE);
      if (!dump_file) {
        Serial.println("open failed");
        no_sd = true;
      }

      dump_file.write(tmp_ptr, tmp_size);
      dump_file.close();
      Serial.println("dump writed");
      }

  */
  led_panel.show(true);

  digitalWrite(PC13, HIGH);

  delay(10);
  /*
    delay(5000);

    led_panel.clear();
    for (uint16_t i = 0; i < width; i++) {
      uint8_t c = i << 2;
      uint8_t c2 = c + 128;

      led_panel.drawPixel(i, 0, led_panel.Color(0, 0, c));
      led_panel.drawPixel(i, 1, led_panel.Color(0, 0, c2));
      led_panel.drawPixel(i, 2, led_panel.Color(0, c, 0));
      led_panel.drawPixel(i, 3, led_panel.Color(0, c2, 0));
      led_panel.drawPixel(i, 4, led_panel.Color(c, 0, 0));
      led_panel.drawPixel(i, 5, led_panel.Color(c2, 0, 0));
      led_panel.drawPixel(i, 6, led_panel.Color(c, c, c));
      led_panel.drawPixel(i, 7, led_panel.Color(c2, c2, c2));
      led_panel.drawPixel(i, 8, led_panel.Color(0, 255 - c, c));
      led_panel.drawPixel(i, 9, led_panel.Color(0, 255 - c2, c2));
      led_panel.drawPixel(i, 10, led_panel.Color(255 - c, 0, c));
      led_panel.drawPixel(i, 11, led_panel.Color(255 - c2, 0, c2));
      led_panel.drawPixel(i, 12, led_panel.Color(255 - c, c, 0));
      led_panel.drawPixel(i, 13, led_panel.Color(255 - c2, c2, 0));
    }

    led_panel.show();

    delay(5000); */
}


