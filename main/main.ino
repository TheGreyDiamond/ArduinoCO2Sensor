/*********************************************************************
  This is an example for our Monochrome OLEDs based on SH1106 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

  This example is for a 128x64 size display using I2C to communicate
  3 pins are required to interface (2 I2C and one reset)

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada  for Adafruit Industries.
  BSD license, check license.txt for more information
  All text above, and the splash screen must be included in any redistribution
*********************************************************************/
#include "FS.h"
#include "SPIFFS.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <DNSServer.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


DNSServer dnsServer;
AsyncWebServer server(80);
Adafruit_SH1106 display(21, 22);
Adafruit_BME280 bmp;

#define FORMAT_SPIFFS_IF_FAILED false

#define OLED_SDA 21
#define OLED_SCL 22

#define BMP_SDA 21
#define BMP_SCL 22

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {

    if (SPIFFS.exists("/login.html")) {

      Serial.println("login.html exists!");

      AsyncResponseStream *response = request->beginResponseStream("text/html");

      File file = SPIFFS.open("/login.html");
      Serial.println(request->url().c_str());

      while(file.available()){

          response->write(file.read());
      }

      request->send(response);
    }

    else{
      request->send(404);
    }

  }
};



#define logoVersuch2_black_small_width 32
#define logoVersuch2_black_small_height 32
static unsigned char logoVersuch2_black_small_bits[] = {
  0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00,
  0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00,
  0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00,
  0xc0, 0xf9, 0x03, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0xc0, 0xff, 0x1f, 0x00,
  0xc0, 0x0f, 0x1e, 0x00, 0xc0, 0x07, 0x08, 0x00, 0xc0, 0x03, 0x00, 0x00,
  0xc0, 0x01, 0x00, 0x00, 0xc0, 0xc1, 0xff, 0x01, 0xc0, 0xc1, 0xff, 0x1f,
  0xc0, 0xc1, 0xff, 0x3f, 0xc0, 0x03, 0x1e, 0x7f, 0xc0, 0x03, 0x1e, 0xf8,
  0xc0, 0x07, 0x1e, 0xf0, 0x80, 0x1f, 0x1f, 0xf0, 0x00, 0xff, 0x1f, 0xe0,
  0x00, 0xfe, 0x1f, 0xe0, 0x00, 0xf0, 0x1e, 0xe0, 0x00, 0x00, 0x1e, 0xf0,
  0x00, 0x00, 0x1e, 0xf0, 0x00, 0x00, 0x1e, 0xf8, 0x00, 0x00, 0x1e, 0x7f,
  0x00, 0x00, 0xfe, 0x3f, 0x00, 0x00, 0xfc, 0x0f
};



#if (SH1106_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SH1106.h!");
#endif

void setup()   {
  Serial.begin(115200);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.

  display.display();
  delay(400);
  display.clearDisplay();
  display.display();
  display.clearDisplay();

  ////display.drawBitmap(30, 16,  logo16_glcd_bmp, 16, 16, 1);

  /*uint8_t icons[NUMFLAKES][3];
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo16_glcd_bmp, 16, 16, WHITE);
    }*/
  display.drawXBitmap(48, 10, logoVersuch2_black_small_bits, 32, 32, WHITE);
  display.setCursor(16, 48);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Booting....");
  display.display();
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        display.clearDisplay();
        display.drawXBitmap(48, 10, logoVersuch2_black_small_bits, 32, 32, WHITE);
        display.setCursor(16, 48);
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.println("SPIFFS Mount failed!");
        display.display();
        //Serial.println("SPIFFS Mount Failed");
        return;
    }

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.softAP("IoT CO2 Sensor");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  server.begin();

    
  delay(10000);

  bmp.begin(0x76);
  /*if (!bmp.begin(0x76)) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Fehler.");
    display.display();
    } else {*/

  //}
  // text display tests

  //delay(20000);
}


void loop() {
  dnsServer.processNextRequest();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Es sind");
  display.println("");

  display.setTextSize(2);
  display.setTextColor(WHITE);
  String str = String(bmp.readTemperature());
  str += " Grad";
  display.println(str);
  display.display();
  delay(500);
}


void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  uint8_t icons[NUMFLAKES][3];
  for (uint8_t f = 0; f < NUMFLAKES; f++) {
    //display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo16_glcd_bmp, w, h, WHITE);
  }
  display.display();

  // initialize
  /*for (uint8_t f = 0; f < NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 1;

    Serial.print("x: ");
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(" y: ");
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(" dy: ");
    Serial.println(icons[f][DELTAY], DEC);
    }

    while (1) {
    // draw each icon
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo16_glcd_bmp, w, h, WHITE);
    }
    display.display();
    delay(200);

    // then erase it + move it
    for (uint8_t f = 0; f < NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS],  logo16_glcd_bmp, w, h, BLACK);
      // move it
      icons[f][YPOS] += icons[f][DELTAY];
      // if its gone, reinit
      if (icons[f][YPOS] > display.height()) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 1;
      }
    }
    }*/
}


void testdrawchar(void) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  for (uint8_t i = 0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
}

void testdrawcircle(void) {
  for (int16_t i = 0; i < display.height(); i += 2) {
    display.drawCircle(display.width() / 2, display.height() / 2, i, WHITE);
    display.display();
  }
}

void testfillrect(void) {
  uint8_t color = 1;
  for (int16_t i = 0; i < display.height() / 2; i += 3) {
    // alternate colors
    display.fillRect(i, i, display.width() - i * 2, display.height() - i * 2, color % 2);
    display.display();
    color++;
  }
}

void testdrawtriangle(void) {
  for (int16_t i = 0; i < min(display.width(), display.height()) / 2; i += 5) {
    display.drawTriangle(display.width() / 2, display.height() / 2 - i,
                         display.width() / 2 - i, display.height() / 2 + i,
                         display.width() / 2 + i, display.height() / 2 + i, WHITE);
    display.display();
  }
}

void testfilltriangle(void) {
  uint8_t color = WHITE;
  for (int16_t i = min(display.width(), display.height()) / 2; i > 0; i -= 5) {
    display.fillTriangle(display.width() / 2, display.height() / 2 - i,
                         display.width() / 2 - i, display.height() / 2 + i,
                         display.width() / 2 + i, display.height() / 2 + i, WHITE);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
  }
}

void testdrawroundrect(void) {
  for (int16_t i = 0; i < display.height() / 2 - 2; i += 2) {
    display.drawRoundRect(i, i, display.width() - 2 * i, display.height() - 2 * i, display.height() / 4, WHITE);
    display.display();
  }
}

void testfillroundrect(void) {
  uint8_t color = WHITE;
  for (int16_t i = 0; i < display.height() / 2 - 2; i += 2) {
    display.fillRoundRect(i, i, display.width() - 2 * i, display.height() - 2 * i, display.height() / 4, color);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
  }
}

void testdrawrect(void) {
  for (int16_t i = 0; i < display.height() / 2; i += 2) {
    display.drawRect(i, i, display.width() - 2 * i, display.height() - 2 * i, WHITE);
    display.display();
  }
}

void testdrawline() {
  for (int16_t i = 0; i < display.width(); i += 4) {
    display.drawLine(0, 0, i, display.height() - 1, WHITE);
    display.display();
  }
  for (int16_t i = 0; i < display.height(); i += 4) {
    display.drawLine(0, 0, display.width() - 1, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i = 0; i < display.width(); i += 4) {
    display.drawLine(0, display.height() - 1, i, 0, WHITE);
    display.display();
  }
  for (int16_t i = display.height() - 1; i >= 0; i -= 4) {
    display.drawLine(0, display.height() - 1, display.width() - 1, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i = display.width() - 1; i >= 0; i -= 4) {
    display.drawLine(display.width() - 1, display.height() - 1, i, 0, WHITE);
    display.display();
  }
  for (int16_t i = display.height() - 1; i >= 0; i -= 4) {
    display.drawLine(display.width() - 1, display.height() - 1, 0, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i = 0; i < display.height(); i += 4) {
    display.drawLine(display.width() - 1, 0, 0, i, WHITE);
    display.display();
  }
  for (int16_t i = 0; i < display.width(); i += 4) {
    display.drawLine(display.width() - 1, 0, i, display.height() - 1, WHITE);
    display.display();
  }
  delay(250);
}

void testscrolltext(void) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.clearDisplay();
  display.println("scroll");
  display.display();

  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
}
