/*********************************************************************
  IoT CO2 Smart Sensor by TheGreydiamond
  Thanks to adafruit:
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
#include <Adafruit_NeoPixel.h>
#include "icons.c"

#define SDA 21
#define SCL 22

#define ROTARY_ENCODER_A_PIN 15
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN 18

#define VERSION "V1.2.2"

#define FORMAT_SPIFFS_IF_FAILED false

#define LED_PIN 19
// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 16
// NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 50

#define DISPLAY_TIMOUT 8000

int menuPage = 0;
int menuPageMax = 2;
int menuPageMin = 0;
int timeSinceShow = 0;

bool isPagePressable = false;
int subMenu = 0;

bool enableLogging = false;
int lastLog = millis();

String infoText = VERSION;
int infoIcon = -1;

long timeSinceLastAction = 0;
bool doneInactivityHandler = false;
bool doneActivityHandler = false;
int test_limits = 2;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_SH1106 display(SDA, SCL);
Adafruit_BME280 bmp;
RTC_DS3231 rtc;

WebServer server(80);

String getTimeInLogFormat()
{
  DateTime now = rtc.now();
  String out = "";
  out += now.day();
  out += ".";
  out += now.month();
  out += ".";
  out += now.year();
  out += "-";
  if (String(now.hour()).length() == 1)
  {
    out += "0";
    out += now.hour();
  }
  else
  {
    out += now.hour();
  }

  out += ":";
  if (String(now.minute()).length() == 1)
  {
    out += "0";
    out += now.minute();
  }
  else
  {
    out += now.minute();
  }
  out += ":";
  if (String(now.second()).length() == 1)
  {
    out += "0";
    out += now.second();
  }
  else
  {
    out += now.second();
  }
  return out;
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

  if (String(now.hour()).length() == 1)
  {
    out += "0";
    out += now.hour();
  }
  else
  {
    out += now.hour();
  }

  out += ":";
  if (String(now.minute()).length() == 1)
  {
    out += "0";
    out += now.minute();
  }
  else
  {
    out += now.minute();
  }
  out += ":";
  if (String(now.second()).length() == 1)
  {
    out += "0";
    out += now.second();
  }
  else
  {
    out += now.second();
  }
  return (out);
}

String getDateOnly()
{
  DateTime now = rtc.now();
  String out = "";
  out += now.day();
  out += ".";
  out += now.month();
  out += ".";
  out += now.year();
  return (out);
}

String getTimeOnly()
{
  DateTime now = rtc.now();
  String out = "";

  if (String(now.hour()).length() == 1)
  {
    out += "0";
    out += now.hour();
  }
  else
  {
    out += now.hour();
  }

  out += ":";
  if (String(now.minute()).length() == 1)
  {
    out += "0";
    out += now.minute();
  }
  else
  {
    out += now.minute();
  }
  out += ":";
  if (String(now.second()).length() == 1)
  {
    out += "0";
    out += now.second();
  }
  else
  {
    out += now.second();
  }
  return (out);
}

void rotary_onButtonClick()
{
  //rotaryEncoder.reset();
  //rotaryEncoder.disable();
  //rotaryEncoder.setBoundaries(-test_limits, test_limits, false);
  //test_limits *= 2;
  if (isPagePressable)
  {
    if (subMenu == 0)
    {
      if (menuPage == 2) // Time to open settings
      {
        subMenu = 1;
      }
    }
  }
}

void handleActivity()
{
  if (timeSinceLastAction + DISPLAY_TIMOUT >= millis() && doneInactivityHandler == false)
  {
    display.dim(false);
    doneInactivityHandler = true;
    doneActivityHandler = false;
    Serial.println("!!!! Active");
  }

  if (timeSinceLastAction + DISPLAY_TIMOUT <= millis() && doneActivityHandler == false)
  { // Inactive
    display.dim(true);
    doneInactivityHandler = false;
    doneActivityHandler = true;
    Serial.println("!!!! Inactive");
  }
}

void activity()
{
  timeSinceLastAction = millis();
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
  activity();

  //for some cases we only want to know if value is increased or decreased (typically for menu items)
  if (encoderDelta > 0)
    Serial.print("+");
  if (menuPage < menuPageMax)
  {
    menuPage++;
  }

  if (encoderDelta < 0)
    if (menuPage > menuPageMin)
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

void makeInfoWindow(String text, int icon)
{
  Serial.println("[LOG] Made info window");
  timeSinceShow = millis();
  infoText = text;
  infoIcon = icon;
  activity();
}

boolean handleWindows()
{
  if (timeSinceShow + 5000 >= millis())
  {
    Serial.println("[LOG] Showing info window");
    display.clearDisplay();
    display.invertDisplay(false);
    display.setTextColor(WHITE);
    display.setCursor(20, 50);
    display.setTextColor(WHITE);
    display.println(infoText);
    if (infoIcon == 1)
    {
      display.drawXBitmap(57, 20, warningSign_bits, warningSign_height, warningSign_width, WHITE);
    }

    display.display();
    return (false);
  }
  else
  {
    return (true);
  }
}

void executeLogAction()
{
  File fileToAppend = SPIFFS.open("/log.txt", FILE_APPEND);
  if (!fileToAppend)
  {
    Serial.println("There was an error opening the file for appending");
    makeInfoWindow("Unable to open log file", 1);
    return;
  }
  String logLine = getTimeInLogFormat();
  logLine += ";";
  logLine += String(bmp.readTemperature()) + ";" + String(bmp.readHumidity()) + ";" + String(bmp.readPressure()) + "\n";
  if (fileToAppend.println("APPENDED LINE"))
  {
    Serial.println("File content was appended");
  }
  else
  {
    Serial.println("File append failed");
    makeInfoWindow("Log saveing failed", 1);
  }
}

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

String SendHTML()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>IoT CO2 Sensor</title>\n";
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
  ptr += "<h2>Live data</h2>\n";
  ptr += "Update time: " + getTimeAndStuff() + "<br>\n";
  ptr += "Temperatur: " + String(bmp.readTemperature()) + "&deg;C<br>\n";
  ptr += "Luftfeuchte: " + String(bmp.readHumidity()) + "%<br>\n";
  ptr += "Luftdruck: " + String(bmp.readPressure()) + "Pa<br>\n";
  ptr += "</body></html>";
  return ptr;
}

void handleRoot()
{
  server.send(200, "text/html", SendHTML());
}

void colorWipe(uint32_t color, int wait)
{
  for (int i = 0; i < strip.numPixels(); i++)
  {                                // For each pixel in strip...
    strip.setPixelColor(i, color); //  Set pixel's color (in RAM)
    strip.show();                  //  Update strip to match
    delay(wait);                   //  Pause for a moment
  }
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
  display.drawXBitmap(48, 10, tgd_logo_bits, tgd_logo_height, tgd_logo_width, WHITE);
  display.setCursor(16, 48);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Booting....");
  display.display();

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
    display.clearDisplay();
    display.drawXBitmap(48, 10, tgd_logo_bits, tgd_logo_height, tgd_logo_width, WHITE);
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
  server.on("/", handleRoot);
  server.onNotFound(handle_NotFound);
  server.begin();
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    //rtc.adjust(DateTime(2020, 9, 30, 21, 37, 30));
  }

  rotaryEncoder.begin();
  rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
  //optionally we can set boundaries and if values should cycle or not
  rotaryEncoder.setBoundaries(0, menuPageMax * 2, true); //minValue, maxValue, cycle values (when max go to min and vice versa)

  bmp.begin(0x76);
  strip.begin();
  strip.show();
  strip.setBrightness(BRIGHTNESS);
  colorWipe(strip.Color(0, 0, 0, 255), 50);
  delay(500);

  activity();
  Serial.println("Start");
}
int i = 2000;

void loop()
{
  if (i >= 2000)
  {
    Serial.println(menuPage);
    if (handleWindows())
    {
      if (subMenu == 0)
      {
        if (menuPage == 0) // Just temperature
        {
          isPagePressable = false;
          display.clearDisplay();
          display.invertDisplay(false);
          display.setTextColor(WHITE);
          display.setCursor(0, 0);
          display.setTextSize(2);
          display.setTextColor(WHITE);
          String str = String(bmp.readTemperature());
          str += " C";
          display.println(str);
          str = String(bmp.readHumidity());
          str += " %";
          display.println(str);
          str = String(bmp.readPressure());
          display.print(str);
          display.setTextSize(1.5);
          display.println(" Pa");
          display.display();
          i = 1990;
        }
        else if (menuPage == 1)
        { // Just time
          isPagePressable = false;
          display.clearDisplay();
          display.invertDisplay(false);
          display.setTextSize(2);
          display.setCursor(16, 20);
          display.setTextColor(WHITE);
          display.println(getTimeOnly());
          display.setCursor(36, 40);
          display.setTextSize(1);
          display.println(getDateOnly());
          display.display();
          i = 1995;
        }
        else if (menuPage == 2)
        { // Settings entry page
          isPagePressable = true;
          display.clearDisplay();
          display.invertDisplay(false);
          display.setTextSize(1);
          display.setCursor(16, 48);
          display.setTextColor(WHITE);
          display.println("Einstellungen");
          display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
          display.display();
          i = 1995;
        }
      }
      else
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Einstellungen\nverlassen");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
      }
    }
  }
  i++;
  server.handleClient();
  handleActivity();
  rotary_loop();
  delay(50);
  if (millis() > 20000)
  {
    rotaryEncoder.enable();
  }
}
