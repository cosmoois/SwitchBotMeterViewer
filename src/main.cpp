// #define USE_TIME
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <stdint.h>
#define U8G2_FONT_SECTION(name)
#include "FreeSansBoldOblique92pt7b.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#ifdef USE_TIME
#include <WiFi.h>
const char *ssid = "XXXXXXXX";
const char *password = "XXXXXXXX";
#endif

BLEUUID serviceUUID     = BLEUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
BLEUUID serviceDataUUID = BLEUUID("00000d00-0000-1000-8000-00805f9b34fb");

#define DISP_SCL 10
#define DISP_SDA 8
#define DISP_RES 7
#define DISP_DC  6
#define DISP_CS  -1
#define DISP_BLK 5
#include "LGFX_ESP32C3_ST7789_SPI.hpp"
LGFX_ESP32C3_ST7789_SPI display(240, 240, DISP_SCL, DISP_SDA, DISP_RES, DISP_DC, DISP_CS, DISP_BLK, -1, -1);
LGFX_Sprite canvas(&display);

static const lgfx::U8g2font u8g2font92(u8g2_font_FreeSansBoldOblique92pt7b);

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

float temperature = 0;
int humidity = -99;
int battery = 0;

void DrawParameter(int line, float val, int format, int warn_col);

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if(!advertisedDevice.haveServiceUUID()) return;
      if(!advertisedDevice.getServiceUUID().equals(serviceUUID)) return;

      if(!advertisedDevice.haveServiceData()) return;
      std::string s = advertisedDevice.getServiceData();
      if(!advertisedDevice.getServiceDataUUID().equals(serviceDataUUID)) return;

      const char* servicedata = s.c_str();
      battery = servicedata[2] & 0b01111111;
      bool isTemperatureAboveFreezing = servicedata[4] & 0b10000000;
      temperature = ( servicedata[3] & 0b00001111 ) / 10.0 + ( servicedata[4] & 0b01111111 );
      if(!isTemperatureAboveFreezing){
        temperature = -temperature;
      }
      humidity = servicedata[5] & 0b01111111;

      printf("----\n");
      printf("address:     %s\n",   advertisedDevice.getAddress().toString().c_str());
      printf("battery:     %d\n",   battery);
      printf("temperature: %.1f\n", temperature);
      printf("humidity:    %d\n",   humidity);
      printf("\n");
    }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning...");

  display.init();
  display.setRotation(0);
  display.setColorDepth(4);
  // display.setBrightness(255); // 100%
  display.setBrightness(8); // 主に夜間利用のため輝度を落とす

#ifdef USE_TIME
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE);
  display.setFont( &fonts::Font4);
  display.setCursor(0, 0);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    display.print(".");
  }
  display.println();
  display.print(WiFi.localIP());

  configTzTime("JST-9", "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
#endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  canvas.setColorDepth(4);  // 4ビット(16色)パレットモード
  canvas.createSprite(240, 240);
  canvas.setPaletteColor(1, 255,   0,   0); // RED
  canvas.setPaletteColor(2,   0, 255,   0); // GREEN
  canvas.setPaletteColor(3,   0,   0, 255); // BLUE
  canvas.setPaletteColor(4, 255, 255,   0); // YELLOW
// カラー定義を16色パレットモードに割り付け直す
#undef TFT_RED
#define TFT_RED     1
#undef TFT_GREEN
#define TFT_GREEN   2
#undef TFT_BLUE
#define TFT_BLUE    3
#undef TFT_YELLOW
#define TFT_YELLOW  4
}

void loop()
{
  char str[8];
  int warn_col; // 警告表示文字色

  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  canvas.clear();

  if (humidity != -99)
  {
    // 温度
    warn_col = TFT_WHITE;
    if ((temperature < 18) || (temperature > 28))  warn_col = TFT_YELLOW;
    if ((temperature < 17) || (temperature > 29))  warn_col = TFT_RED;
    if (warn_col == TFT_RED)  canvas.fillRect(0, 0, 240, 120, TFT_WHITE);
    DrawParameter(0, temperature, 1, warn_col);

    // 湿度
    // 室内で快適に過ごせる湿度は夏場で45～60%、冬場は55～65%程度と言われている
    warn_col = TFT_WHITE;
    if ((humidity < 45) || (humidity > 65))  warn_col = TFT_YELLOW;
    if ((humidity < 40) || (humidity > 70))  warn_col = TFT_RED;
    if (warn_col == TFT_RED)	canvas.fillRect(98, 120, 240, 240, TFT_WHITE);
    DrawParameter(1, (float)humidity, 0, warn_col);
  }

  // バッテリー
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setFont( &fonts::FreeSansBoldOblique18pt7b);
  canvas.setTextDatum( textdatum_t::top_left);
  canvas.drawString(String(battery) + "%", 0, 140);

#ifdef USE_TIME
  // 現在時刻取得
  time_t timeNow = time(NULL);
  struct tm* tmNow = localtime(&timeNow);

  if (tmNow->tm_year != 70) {
    // 日付
    char szDate[32];
    // strftime(szDate, sizeof(szDate), "%y/%m/%d(%a) %S", tmNow);
    strftime(szDate, sizeof(szDate), "%m/%d", tmNow);

    // 時刻
    char szTime[32];
    // strftime(szTime, sizeof(szTime), "%H:%M:%S", tmNow);
    strftime(szTime, sizeof(szTime), "%H:%M", tmNow);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setFont( &fonts::FreeSansBoldOblique18pt7b);
    canvas.setTextDatum( textdatum_t::top_left);
    canvas.drawString(szDate, 0, 170);
    canvas.drawString(szTime, 0, 200);
  }
#endif

  canvas.pushSprite(0, 0);

  delay(10 * 1000);
}

void DrawParameter(int line, float val, int format, int warn_col)
{
  String Str_set;
  char str[8];
  int num_size;   // 数字フォント基準幅（フォント毎に固定：調整不可）
  int dot_size;   // 小数点フォント幅（フォント毎に固定：調整不可）
  int x_adj;      // x軸右寄せ調整値（フォント毎に固定：調整不可）

  int rx;         // 右寄せ位置
  int line_space; // 行間隔
  int y_offset;   // Y軸オフセット

  if (format == 0) {
    Str_set = String((int)val);
  } else {
    Str_set = String(val, format);
  }
  Str_set.toCharArray(str, sizeof(str));

  canvas.setTextColor(warn_col);
	canvas.setFont(&u8g2font92);
  canvas.setTextDatum( textdatum_t::top_left);
  canvas.setFont(&u8g2font92); num_size = 65;  dot_size = 32;  x_adj = 17;
  rx = 240;
  line_space = 120;
  y_offset = 16;
  canvas.drawString(str, rx - (strlen(str) * num_size) - (format ? 0 : dot_size) + x_adj, (line * line_space) + y_offset);
}
