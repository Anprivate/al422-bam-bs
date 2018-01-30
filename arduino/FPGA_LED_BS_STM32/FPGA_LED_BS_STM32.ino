#include "LED_PANEL_BS.h"
#include <libmaple/dma.h>
#include <SPI.h>
#include <SdFat.h>

#define width 64
#define height 32
// bits per color (2..15)
#define bpc 8

#define scan_lines  16
#define RGB_inputs  2
#define WE_out_pin      PB10

LED_PANEL led_panel = LED_PANEL(width, height, scan_lines, RGB_inputs, WE_out_pin, bpc);

SdFat sd2(2);
const uint8_t SD2_CS = PB12;   // chip select for sd2
bool no_sd;

#define file_name "dump.bin"
File dump_file;

#define video_file_name "video888.raw"
File video_file;
#define ms_per_frame  (1000/24)
uint8_t line_buffer[width * 3];
unsigned long last_frame_indicated_at;

void setup() {
  Serial.begin(115200);
  //  while (!Serial) {};

  pinMode(PC13, OUTPUT);

  led_panel.begin();
  led_panel.clear();

  no_sd = false;
  if (!sd2.begin(SD2_CS, SD_SCK_MHZ(18))) {
    Serial.println("sd init failed");
    no_sd = true;
  }

  video_file = sd2.open(video_file_name, FILE_READ);
  if (!video_file) {
    Serial.println("open failed");
    no_sd = true;
  }

  led_panel.SetBrightness(0.1);
  last_frame_indicated_at = millis();
}

void loop() {
  digitalWrite(PC13, LOW);

  led_panel.clear();

  for (uint8_t j = 0; j < 4; j++)
    for (uint8_t i = 0; i < 64; i++) {
      uint8_t tmp_c = i + j * 64;
      led_panel.setPixelColor8(i, j, 0, 0, tmp_c);
      led_panel.setPixelColor8(i, j + 4, 0, tmp_c, 0);
      led_panel.setPixelColor8(i, j + 8, tmp_c, 0, 0);
      led_panel.setPixelColor8(i, j + 16, tmp_c, tmp_c, tmp_c);
    }

  led_panel.setPassThruColor8(0xFF, 0xFF, 0xFF);
  led_panel.setCursor(3, 23);
  led_panel.setTextSize(1);
  led_panel.setTextWrap(false);
  led_panel.print(led_panel.GetBrightness() * 100.0, 1);
  /*
    uint8_t br = 0xFF;

    led_panel.setPixelColor8(0, 0, 0, 0, br);

    led_panel.setPassThruColor8(0, 0, br);
    led_panel.drawCircle(16, 16, 10, 0x000);

    led_panel.setPassThruColor8(0, br, 0);
    led_panel.setCursor(1, 1);
    led_panel.setTextSize(1);
    led_panel.setTextWrap(false);
    led_panel.print("Test");

    led_panel.setPassThruColor8(br, 0, 0);
    led_panel.setCursor(4, 11);
    led_panel.print("Best");

    led_panel.setPassThruColor8(br, br, br);
    led_panel.setCursor(7, 21);
    led_panel.print("Case");
  */
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
  /*
    if (no_sd) {
      led_panel.setCursor(1, 1);
      led_panel.setTextColor(led_panel.Color(80, 80, 0));
      led_panel.setTextSize(1);
      led_panel.setTextWrap(false);
      led_panel.print("SD error");
    } else {
      for (uint16_t y = 0; y < height; y++) {
        int bytes_readed = video_file.read(line_buffer, width * 3);
        if (bytes_readed < width * 3) {
          video_file.rewind();
          break;
        }
        led_panel.PutPictureRGB888(0, y, width, 1, line_buffer);
      }
    }

    unsigned long from_last_frame = millis() - last_frame_indicated_at;
    if (from_last_frame < ms_per_frame)
      delay(ms_per_frame - from_last_frame);

    led_panel.show(false);
    last_frame_indicated_at = millis();
  */
  led_panel.show(true);

  digitalWrite(PC13, HIGH);


  static boolean br_dir = true;

  float fps, eff;

  eff = led_panel.CalculateEfficiency(0, &fps);
  Serial.flush();
  Serial.print("Efficiency =");
  Serial.print(eff, 5);
  Serial.print(", FPS =");
  Serial.print(fps, 5);
  Serial.print(", Min br =");
  Serial.print(led_panel.CalculateMinBrightness(), 5);
  Serial.print(", prescaler =");
  Serial.print(led_panel.GetPrescaler());

  float new_br;
  if (br_dir) {
    new_br = led_panel.GetBrightness() * 0.95;
    if (new_br < led_panel.CalculateMinBrightness()) {
      br_dir = false;
      new_br = led_panel.CalculateMinBrightness();
    }
  } else {
    new_br = led_panel.GetBrightness() * 1.05;
    if (new_br > 1.0) {
      br_dir = true;
      new_br = 1.0;
    }
  }
  Serial.print(", br =");
  Serial.print(led_panel.SetBrightness(new_br), 5);
  Serial.print("/");
  Serial.println(new_br, 5);
  Serial.flush();

  delay(10);
}



