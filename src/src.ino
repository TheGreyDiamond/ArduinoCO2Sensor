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
#include "RTClib.h"
#include "AiEsp32RotaryEncoder.h"

#define SDA 21
#define SCL 22

#define ROTARY_ENCODER_A_PIN 15
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN -1

#define FORMAT_SPIFFS_IF_FAILED false

int menuPage = 0;
int menuPageMax = 0;
int menuPageMin = 0;

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN);
Adafruit_SH1106 display(SDA, SCL);
Adafruit_BME280 bmp;
RTC_DS3231 rtc;

WebServer server(80);

int test_limits = 2;
void rotary_onButtonClick()
{
  //rotaryEncoder.reset();
  //rotaryEncoder.disable();
  //rotaryEncoder.setBoundaries(-test_limits, test_limits, false);
  //test_limits *= 2;
  Serial.println("CLICK");
}

void rotary_loop()
{
  //first lets handle rotary encoder button click
  if (rotaryEncoder.currentButtonState() == BUT_RELEASED)
  {
    //we can process it here or call separate function like:
    rotary_onButtonClick();
  }

  //lets see if anything changed
  int16_t encoderDelta = rotaryEncoder.encoderChanged();

  //optionally we can ignore whenever there is no change
  if (encoderDelta == 0)
    return;

  //for some cases we only want to know if value is increased or decreased (typically for menu items)
  if (encoderDelta > 0)
    Serial.print("+");
  if (menuPage <= menuPageMax)
  {
    menuPage++;
  }

  if (encoderDelta < 0)
    if (menuPage >= menuPageMin)
    {
      menuPage--;
    }
  Serial.print("-");

  //for other cases we want to know what is current value. Additionally often we only want if something changed
  //example: when using rotary encoder to set termostat temperature, or sound volume etc

  //if value is changed compared to our last read
  if (encoderDelta != 0)
  {
    //now we need current value
    int16_t encoderValue = rotaryEncoder.readEncoder();
    //process new value. Here is simple output.
    Serial.print("Value: ");
    Serial.println(encoderValue);
  }
}

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
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

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

void handle_NotFound()
{
  String errorString = "404 - Not Found";
  if (SPIFFS.exists("/404.html"))
  {

    Serial.println("login.html exists!");
    File file = SPIFFS.open("/404.html");
    errorString = "";
    while (file.available())
    {
      errorString += file.readString();
    }
  }
  //String errorString = "<!DOCTYPE html><html lang='en'><head>    <meta charset='utf-8'>    <title>404</title>    <meta name='viewport' content='width=device-width, initial-scale=1'>	<!--<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css'>-->    <style>        * {            line-height: 1.2;            margin: 0;        }        html {            color: #888;            display: table;            font-family: sans-serif;            height: 100%;            text-align: center;            width: 100%;        }        body {            display: table-cell;            vertical-align: middle;            margin: 2em auto;        }        h1 {            color: #555;            font-size: 2em;            font-weight: 400;        }				h0 {			color: #555;			font-size: 5em;				}        p {            margin: 0 auto;            width: 280px;        }        @media only screen and (max-width: 280px) {            body, p {                width: 95%;            }            h1 {                font-size: 1.5em;                margin: 0 0 0.3em;            }        }    </style></head><body>	<!--<h0><i class='fa fa-times'></i></h0>-->    <h1>404</h1>    <p>Die angeforderte Datei konnte nicht gefunden werden.</p></body></html><!-- IE needs 512+ bytes: https://blogs.msdn.microsoft.com/ieinternals/2010/08/18/friendly-http-error-pages/ -->";
  server.send(404, "text/html", errorString);
}

String getTimeAndStuff()
{
  DateTime now = rtc.now();
  String out = "";
  out += now.day();
  out += ".";
  out += now.month();
  out += ".";
  out += now.year();
  out += " ";

  out += now.hour();
  out += ":";
  out += now.minute();
  out += ":";
  out += now.second();
  return (out);
}

String SendHTML()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>LED Control</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #3498db;}\n";
  ptr += ".button-on:active {background-color: #2980b9;}\n";
  ptr += ".button-off {background-color: #34495e;}\n";
  ptr += ".button-off:active {background-color: #2c3e50;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>IoT CO2 Sensor</h1>\n";
  ptr += "<h3>" + getTimeAndStuff() + "</h3>\n";
  ptr += "</body></html>";
  return ptr;
}

void handleRoot()
{
  server.send(200, "text/html", SendHTML());
}

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
  

  // bool DNSstate = MDNS.begin("sensor");

  server.on("/", handleRoot);

  ///erver.on("/inline", []() {
  //   server.send(200, "text/plain", "this works as well");
  //});

  server.onNotFound(handle_NotFound);
  server.begin();
  //MDNS.addService("http", "tcp", 80);

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    //abort();
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // September 30, 2020 at 3am you would call:
    //rtc.adjust(DateTime(2020, 9, 30, 21, 37, 30));
  }

  
  rotaryEncoder.begin();
  rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
  //optionally we can set boundaries and if values should cycle or not
  rotaryEncoder.setBoundaries(0, 10, true); //minValue, maxValue, cycle values (when max go to min and vice versa)

  bmp.begin(0x76);
  delay(5000);
  Serial.println("Server started!");
}
int i = 0;

void loop()
{
  if (i == 2000)
  {
    Serial.println("\n\n!!!!");
    Serial.println(menuPage);
    if (menuPage == 0)
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
  }
  i++;
  server.handleClient();
  rotary_loop();

  delay(50);
  if (millis() > 20000){ rotaryEncoder.enable();}
}
