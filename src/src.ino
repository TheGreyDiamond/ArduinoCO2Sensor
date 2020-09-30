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
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WebServer.h>

#define SDA 21
#define SCL 22

#define FORMAT_SPIFFS_IF_FAILED false

Adafruit_SH1106 display(SDA, SCL);
Adafruit_BME280 bmp;
WiFiServer server(80);

/*if (SPIFFS.exists("/login.html"))
    {

      Serial.println("login.html exists!");

      AsyncResponseStream *response = request->beginResponseStream("text/html");

      Serial.println(request->url().c_str());

        File file = SPIFFS.open("/login.html");

        while (file.available())
        {

          response->write(file.read());
        }
      

      request->send(response);
    }
*/

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
    0x00, 0x00, 0xfe, 0x3f, 0x00, 0x00, 0xfc, 0x0f};

#if (SH1106_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SH1106.h!");
#endif



void setup()
{
  Serial.begin(9600);

  display.begin(SH1106_SWITCHCAPVCC, 0x3C);

  display.display();
  delay(400);
  display.clearDisplay();
  display.display();
  display.clearDisplay();
  display.drawXBitmap(48, 10, logoVersuch2_black_small_bits, 32, 32, WHITE);
  display.setCursor(16, 48);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Booting....");
  display.display();

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
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
  Serial.println("Server started!");

  // bool DNSstate = MDNS.begin("sensor");

  // server.on("/", handleRoot);

  // server.on("/inline", []() {
  //   server.send(200, "text/plain", "this works as well");
  //});

  //server.onNotFound(handleNotFound);
  server.begin();
  //MDNS.addService("http", "tcp", 80);

  delay(5000);

  bmp.begin(0x76);
}
int i = 0;

void loop()
{
  WiFiClient client = server.available();
  if (client)
  {
    Serial.println("Got client");
    while (client.connected() && !client.available())
    {
      delay(1);
    }
    String req = client.readStringUntil('\r');
    int addr_start = req.indexOf(' ');
    int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
        Serial.print("Invalid request: ");
        Serial.println(req);
        return;
    }
    req = req.substring(addr_start + 1, addr_end);
    Serial.print("Request: ");
    Serial.println(req);

    String s;
    if (req == "/")
    {
        IPAddress ip = WiFi.localIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\r\n\n<!DOCTYPE HTML>\r\n<html>Hello from ESP32 at ";
        s += ipStr;
        s += "</html>\r\n\r\n";
        Serial.println("Sending 200");
    }
    else
    {
        String errorString = "404 Not Found";
        if(SPIFFS.exists("/404.html")){
          Serial.println("Found 404 page");
          errorString = "";
          File file = SPIFFS.open("/404.html");

          while (file.available())
          {
            errorString+=file.readString();
          }
          Serial.println(errorString);
        }

        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n" + errorString + "\r\n\r\n";
        Serial.println("Sending 404");
    }
    client.print(s);

    client.stop();
  }
  if (i == 2000)
  {
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
    i = 0;
  }
  i++;
}
