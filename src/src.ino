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
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include "RTClib.h"

#include "SimpleBLE.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

//#include <BLEDevice.h>
//#include <BLEUtils.h>
//#include <BLEServer.h>

#include <ESP32Encoder.h>

#include "SparkFun_SGP30_Arduino_Library.h"
#include "icons.c"
#include <CircularBuffer.h>
#include <math.h>
//#include "SD.h"

#define SDA 21
#define SCL 22

#define ROTARY_ENCODER_A_PIN 15
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN -1

#define VERSION "V1.3.4 "

#define SD_CS 5

#define FORMAT_SPIFFS_IF_FAILED false

#define LED_PIN 14
#define LED_COUNT 16

// NeoPixel brightness, 0 (min) to 255 (max)
int BRIGHTNESS = 40;

#define DISPLAY_TIMOUT 8000

bool updateRing = true;
int lastAlarmLvl = 0;

int amountToFill = 120;

String menuPage = "0.0";
int menuPageMax = 4;
int menuPageMin = 0;
int menuSubPageMin = 1;
int menuSubPageMax = 4;
int timeSinceShow = 0;

CircularBuffer<float, 120> temperature;
CircularBuffer<float, 120> humidity;
CircularBuffer<float, 120> preasure;

CircularBuffer<float, 120> CO2;
CircularBuffer<float, 120> TVOC;

bool isPagePressable = false;
int subMenu = 0;
bool testCriticalCO2lvl = false;
bool partyMode = false;

bool buttonPressUnhandeld = false;

// Logging vars
bool enableLogging = true;
int lastLog = millis();
int logIntervall = 30000;
int subIntervall = -1;

int graphToPlot = 0;

float allValsTemp = 0;
float allValsHum = 0;
float allValspres = 0;
float allValsCO2 = 0;
float allValsTVOC = 0;

int logCount = 0;

String infoText = String(VERSION) + " " + String(__DATE__) + " " + String(__TIME__);
int infoIcon = -1;

volatile int interruptCounter;
int totalInterruptCounter;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

int loopI2 = 0;

long timeSinceLastAction = 0;
bool doneInactivityHandler = false;
bool doneActivityHandler = false;
int test_limits = 2;
char daysOfTheWeek[7][12] = {"Sonntag", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

bool allowUpdate = false;

ESP32Encoder encoder;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_SH1106 display(SDA, SCL);
Adafruit_BME280 bmp;
RTC_DS3231 rtc;
SGP30 co2Sensor;
SimpleBLE ble;
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

void readFile(const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = SPIFFS.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available())
  {
    Serial.write(file.read());
  }
}

void getLatestData()
{
  Serial.println("LATEST DATA");
  File file = SPIFFS.open("/log.txt");
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  // Get line amount of log file
  int lineCount = 0;
  int linesToRead = 0;
  while (file.available())
  {
    file.readStringUntil('\n');
    lineCount++;
  }
  linesToRead = lineCount - 100;
  if (linesToRead < 0)
  {
    linesToRead = lineCount;
  }

  String outF[105];
  int tempI = 0;
  bool stillRun = true;
  Serial.println("LEN1: " + String(outF->length()));
  while (file.available() and stillRun == true)
  {
    Serial.println("Loooopy");
    outF[tempI] = String(file.readStringUntil('\n'));
    if (tempI == 100)
    {
      stillRun = false;
      Serial.println("Done");
    }
    tempI++;
  }

  //while(tempIa <= outF->length())
  int tempA = 0;
  while (tempA <= tempI)
  {
    Serial.println(outF[tempA]);
    tempA++;
  }

  Serial.println("LEN: " + String(outF->length()));
  Serial.println("TEST: " + String(tempI));
}

void deleteFile(const char *path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  if (SPIFFS.remove(path))
  {
    Serial.println("- file deleted");
  }
  else
  {
    Serial.println("- delete failed");
  }
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
  if (isPagePressable)
  {

    if (menuPage == "3.0") // Time to open settings
    {
      menuPage = "3.1";
      menuSubPageMax = 8;
    }
    else if (menuPage == "3.1")
    {
      menuPage = "3.0";
    }
    else if (menuPage == "3.2")
    {
      if (enableLogging)
      {
        makeInfoWindow("Stopping logging", 1);
        enableLogging = false;
      }
      else
      {
        makeInfoWindow("Start logging", 1);
        enableLogging = true;
      }
    }
    else if (menuPage == "3.4")
    {
      makeInfoWindow("Test Warning", 1);
    }
    else if (menuPage == "3.5")
    {
      Serial.println("!------------!");
      Serial.println(testCriticalCO2lvl);
      if (testCriticalCO2lvl == true)
      {
        testCriticalCO2lvl = false;
      }
      else
      {
        testCriticalCO2lvl = true;
      }
      //testCriticalCO2lvl = !testCriticalCO2lvl;
      Serial.println(testCriticalCO2lvl);
    }
    else if (menuPage == "3.6")
    {
      if (partyMode == true)
      {
        partyMode = false;
      }
      else
      {
        partyMode = true;
      }
      //testCriticalCO2lvl = !testCriticalCO2lvl;
    }
    else if (menuPage == "3.7")
    {
      Serial.println("-------[DATA LOG DUMP]-------");
      //readFile("/log.txt");
      getLatestData();
    }
    else if (menuPage == "3.8")
    {
      Serial.println("-------[DELETING LOG]-------");
      deleteFile("/log.txt");
    }
    else if (menuPage == "4.0")
    {
      graphToPlot++;
      if (graphToPlot > 4)
      {
        graphToPlot = 0;
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
    Serial.println("Device active again");
  }

  if (timeSinceLastAction + DISPLAY_TIMOUT <= millis() && doneActivityHandler == false)
  { // Inactive
    display.dim(true);
    doneInactivityHandler = false;
    doneActivityHandler = true;
    Serial.println("Device gone inactive");
  }
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void activity()
{
  timeSinceLastAction = millis();
}

void rotary_loop()
{
  //first lets handle rotary encoder button click
  if (buttonPressUnhandeld == true)
  {
    //we can process it here or call separate function like:
    buttonPressUnhandeld = false;
    rotary_onButtonClick();
  }

  //lets see if anything changed
  //int16_t encoderDelta = rotaryEncoder.encoderChanged();
  int16_t encoderDelta = encoder.getCount();
  encoder.clearCount();
  encoderDelta = encoderDelta / 1;
  //optionally we can ignore whenever there is no change
  if (encoderDelta == 0)
    return;
  activity();

  //for some cases we only want to know if value is increased or decreased (typically for menu items)
  if (encoderDelta > 0)
  {
    Serial.println("Enocder up");
    if (getValue(menuPage, '.', 1) == "0")
    {
      Serial.println("menu handler PAGE 0 is " + getValue(menuPage, '.', 0));
      if (getValue(menuPage, '.', 0).toInt() < menuPageMax)
      {
        Serial.println("Doing something");
        menuPage = String(getValue(menuPage, '.', 0).toInt() + 1) + "." + getValue(menuPage, '.', 1);
      }
    }
    else
    {
      Serial.println("submenu handler");
      if (getValue(menuPage, '.', 1).toInt() < menuSubPageMax)
      {
        Serial.println("Doing something");
        menuPage = getValue(menuPage, '.', 0) + "." + String(getValue(menuPage, '.', 1).toInt() + 1);
      }
    }
  }
  else
  {
    Serial.println("Enocder down");
    if (getValue(menuPage, '.', 1) == "0")
    {
      Serial.println("menu handler");
      if (getValue(menuPage, '.', 0).toInt() > menuPageMin)
      {
        Serial.println("Doing something");
        menuPage = String(getValue(menuPage, '.', 0).toInt() - 1) + "." + getValue(menuPage, '.', 1);
        Serial.println(menuPage);
      }
    }
    else
    {
      Serial.println("Submenu handler");
      if (getValue(menuPage, '.', 1).toInt() > menuSubPageMin)
      {
        Serial.println("Doing something");
        menuPage = getValue(menuPage, '.', 0) + "." + String(getValue(menuPage, '.', 1).toInt() - 1);
      }
    }
  }
}

void plotGraph(int whatToPlot)
{
  if (whatToPlot == 0)
  {
    float maximum = 2000;
    float screenResY = 60;
    float scaleY = screenResY / maximum;
    int dataPointToDrawIndex = 0;
    int drawValue = -1;
    display.clearDisplay();
    display.invertDisplay(false);
    while (dataPointToDrawIndex <= 120)
    {
      drawValue = screenResY - (int)roundf(CO2[dataPointToDrawIndex] * scaleY);
      display.drawPixel(dataPointToDrawIndex, drawValue, WHITE);
      //Serial.println("Draw X: " + String(drawValue) + " Input value: " + String(CO2[dataPointToDrawIndex]) + " non Rounded Value: " + String(CO2[dataPointToDrawIndex] * scaleY) + " Scale: " + String(scaleY));
      dataPointToDrawIndex++;
    }
    display.setCursor(0, 0);
    display.println("CO2");
  }
  if (whatToPlot == 1)
  {
    float maximum = 1000;
    float screenResY = 60;
    float scaleY = screenResY / maximum;
    int dataPointToDrawIndex = 0;
    int drawValue = -1;
    display.clearDisplay();
    display.invertDisplay(false);
    while (dataPointToDrawIndex <= 120)
    {
      drawValue = screenResY - (int)roundf(TVOC[dataPointToDrawIndex] * scaleY);
      display.drawPixel(dataPointToDrawIndex, drawValue, WHITE);
      //Serial.println("Draw X: " + String(drawValue) + " Input value: " + String(CO2[dataPointToDrawIndex]) + " non Rounded Value: " + String(CO2[dataPointToDrawIndex] * scaleY) + " Scale: " + String(scaleY));
      dataPointToDrawIndex++;
    }
    display.setCursor(0, 0);
    display.println("TVOC");
  }
  if (whatToPlot == 2)
  {
    float maximum = 40;
    float screenResY = 60;
    float scaleY = screenResY / maximum;
    int dataPointToDrawIndex = 0;
    int drawValue = -1;
    display.clearDisplay();
    display.invertDisplay(false);
    while (dataPointToDrawIndex <= 120)
    {
      drawValue = screenResY - (int)roundf(TVOC[dataPointToDrawIndex] * scaleY);
      display.drawPixel(dataPointToDrawIndex, drawValue, WHITE);
      //Serial.println("Draw X: " + String(drawValue) + " Input value: " + String(CO2[dataPointToDrawIndex]) + " non Rounded Value: " + String(CO2[dataPointToDrawIndex] * scaleY) + " Scale: " + String(scaleY));
      dataPointToDrawIndex++;
    }
    display.setCursor(0, 0);
    display.println("Temperature");
  }
  if (whatToPlot == 3)
  {
    float maximum = 100;
    float screenResY = 60;
    float scaleY = screenResY / maximum;
    int dataPointToDrawIndex = 0;
    int drawValue = -1;
    display.clearDisplay();
    display.invertDisplay(false);
    while (dataPointToDrawIndex <= 120)
    {
      drawValue = screenResY - (int)roundf(TVOC[dataPointToDrawIndex] * scaleY);
      display.drawPixel(dataPointToDrawIndex, drawValue, WHITE);
      //Serial.println("Draw X: " + String(drawValue) + " Input value: " + String(CO2[dataPointToDrawIndex]) + " non Rounded Value: " + String(CO2[dataPointToDrawIndex] * scaleY) + " Scale: " + String(scaleY));
      dataPointToDrawIndex++;
    }
    display.setCursor(0, 0);
    display.println("Humidity");
  }
  if (whatToPlot == 4)
  {
    float maximum = 100000;
    float screenResY = 60;
    float scaleY = screenResY / maximum;
    int dataPointToDrawIndex = 0;
    int drawValue = -1;
    display.clearDisplay();
    display.invertDisplay(false);
    while (dataPointToDrawIndex <= 120)
    {
      drawValue = screenResY - (int)roundf(TVOC[dataPointToDrawIndex] * scaleY);
      display.drawPixel(dataPointToDrawIndex, drawValue, WHITE);
      //Serial.println("Draw X: " + String(drawValue) + " Input value: " + String(CO2[dataPointToDrawIndex]) + " non Rounded Value: " + String(CO2[dataPointToDrawIndex] * scaleY) + " Scale: " + String(scaleY));
      dataPointToDrawIndex++;
    }
    display.setCursor(0, 0);
    display.println("Pressure");
  }

  display.display();
}

void theaterChaseRainbow(int wait)
{
  int firstPixelHue = 0; // First pixel starts at red (hue 0)
  for (int a = 0; a < 30; a++)
  { // Repeat 30 times...
    for (int b = 0; b < 3; b++)
    {                //  'b' counts from 0 to 2...
      strip.clear(); //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for (int c = b; c < strip.numPixels(); c += 3)
      {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int hue = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color);                       // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}

void updateLEDring()
{
  /* 
  * 250-400ppm       dark green
  * 400-1,000ppm     green
  * 1,000-2,000ppm   yellow
  * 2,000-5,000 ppm  orange
  * 5,000            red
  * >40,000 ppm      red blinking        aka u ded soon     updateRing
  */
  // co2Sensor.measureAirQuality();
  int mesValue = co2Sensor.CO2;
  int currentAlarmLvl = 1;
  if (testCriticalCO2lvl == false && partyMode == false)
  {
    if (mesValue < 400)
    {
      currentAlarmLvl = 2;
    }
    if (mesValue >= 400 and mesValue <= 1000)
    {
      currentAlarmLvl = 3;
    }
    if (mesValue >= 1001 and mesValue <= 2000)
    {
      currentAlarmLvl = 4;
    }
    if (mesValue >= 2001 and mesValue <= 5000)
    {
      currentAlarmLvl = 5;
    }
    if (mesValue >= 5000 and mesValue <= 8000)
    {
      currentAlarmLvl = 6;
    }
    if (mesValue >= 40000)
    {
      currentAlarmLvl = 7;
    }
  }
  else
  {
    colorWipe(strip.Color(255, 0, 0, 0), 50);
    delay(200);
    colorWipe(strip.Color(0, 0, 0, 0), 1);
    colorWipe(strip.Color(255, 0, 0, 0), 50);
  }

  if (currentAlarmLvl != lastAlarmLvl)
  {
    updateRing = true;
    lastAlarmLvl = currentAlarmLvl;
  }
  else
  {
    updateRing = false;
  }
  if (updateRing && testCriticalCO2lvl == false && partyMode == false)
  {
    Serial.println("Actually ring update");
    if (currentAlarmLvl == 2)
    {
      colorWipe(strip.Color(0, 255, 0, 0), 50);
    }
    if (currentAlarmLvl == 3)
    {
      colorWipe(strip.Color(0, 150, 0, 0), 50);
    }
    if (currentAlarmLvl == 4)
    {
      colorWipe(strip.Color(255, 255, 0, 0), 50);
    }
    if (currentAlarmLvl == 5)
    {
      colorWipe(strip.Color(255, 165, 0, 0), 50);
    }
    if (currentAlarmLvl == 6)
    {
      colorWipe(strip.Color(255, 0, 0, 0), 50);
    }
    if (currentAlarmLvl == 7)
    {
      colorWipe(strip.Color(255, 0, 0, 0), 50);
      delay(200);
      colorWipe(strip.Color(0, 0, 0, 0), 1);
      colorWipe(strip.Color(255, 0, 0, 0), 50);
    }
  }
  if (partyMode)
  {
    theaterChaseRainbow(40);
  }
  delay(10);
}

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
  updateLogMath();
  if (lastLog + subIntervall <= millis())
  {
    Serial.println("SUB LOG");
    lastLog = millis();
    allValsTemp += bmp.readTemperature();
    allValsHum += bmp.readHumidity();
    allValspres += bmp.readPressure();
    allValsCO2 += co2Sensor.CO2;
    allValsTVOC += co2Sensor.TVOC;
    logCount++;
    Serial.println("LOG COUNT: " + String(logCount));
  }
  if (logCount >= 5)
  {

    allValsTemp = allValsTemp / logCount;
    allValsHum = allValsHum / logCount;
    allValspres = allValspres / logCount;
    allValsCO2 = allValsCO2 / logCount;
    allValsTVOC = allValsTVOC / logCount;
    logCount = 0;
    Serial.println("Tried to start log");

    temperature.push(allValsTemp);
    humidity.push(allValsHum);
    preasure.push(allValspres);
    CO2.push(allValsCO2);
    TVOC.push(allValsTVOC);
    amountToFill--;

    /*for(int i = 0; i<=amountToFill; i++){
      CO2.push(400);
    }*/

    /* File fileToAppend = SPIFFS.open("/log.txt", FILE_APPEND);

    if (!fileToAppend)
    {
      Serial.println("There was an error opening the file for appending");
      makeInfoWindow("Unable to open log file", 1);
      return;
    }
    String logLine = getTimeInLogFormat();
    //co2Sensor.measureAirQuality();
    logLine += ";";
    logLine += String(allValsTemp) + ";" + String(allValsHum) + ";" + String(allValspres) + ";" + String(allValsCO2) + ";" + String(allValsTVOC) + "\n";
    if (fileToAppend.print(logLine))
    {
      Serial.println("File content was appended");
    }
    else
    {
      Serial.println("File append failed");
      makeInfoWindow("Log saveing failed", 1);
    }*/
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

void updateLogMath()
{
  subIntervall = logIntervall / 5;
  //Serial.println("Accutllay measure every " + String(subIntervall) + " seconds");
}

void handleTimeSet()
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
  ptr += "<h1>Fertig!</h1>\n";
  ptr += "<a href='/'>Zur&uuml;ck!</a>\n";
  ptr += "</body></html>";
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  String date = String(server.arg(server.argName(0)));
  String time = String(server.arg(server.argName(1)));
  Serial.println(date);
  Serial.println(time);
  int date_d = getValue(date, '-', 0).toInt();
  int date_m = getValue(date, '-', 1).toInt();
  int date_y = getValue(date, '-', 2).toInt();
  int time_m = getValue(time, ':', 0).toInt();
  int time_h = getValue(time, ':', 1).toInt();
  rtc.adjust(DateTime(date_d, date_m, date_y, time_m, time_h));
  server.send(200, "text/html", ptr);
}

String SendHTML()
{
  //co2Sensor.measureAirQuality();
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
  ptr += "Luftdruck: " + String(bmp.readPressure()) + "Pa<br>\n"; //co2Sensor.CO2
  ptr += "CO2 Gehalt: " + String(co2Sensor.CO2) + "ppm<br>\n";
  ptr += "TVOC: " + String(co2Sensor.TVOC) + "ppb<br>\n";
  ptr += "<h2>Uhr setzen</h2>\n";
  ptr += "<form action='/setClock'>";
  ptr += "  <label for='birthday'>Datum:</label>";
  ptr += "  <input type='date' id='datum' name='datum'>";
  ptr += "  <label for='appt'>Uhrzeit:</label>";
  ptr += "  <input type='time' id='uhrzeit' name='uhrzeit'>";
  ptr += "  <input type='submit' value='Speichern'>";
  ptr += "</form>";
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

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  //co2Sensor.measureAirQuality();
  allowUpdate = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR isr()
{
  buttonPressUnhandeld = true;
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
  server.on("/setClock", handleTimeSet);
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
    //rtc.adjust(DateTime(2020, 10, 25, 15, 54, 30));
  }

  ble.begin("ESP32 SimpleBLE");

  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN);
  encoder.setCount(20);
  encoder.clearCount();
  attachInterrupt(ROTARY_ENCODER_BUTTON_PIN, isr, FALLING);
  ///rotaryEncoder.begin();
  ///rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
  //optionally we can set boundaries and if values should cycle or not
  ///rotaryEncoder.setBoundaries(0, menuPageMax * 2, true); //minValue, maxValue, cycle values (when max go to min and vice versa)

  bmp.begin(0x76);
  Wire.begin();
  if (co2Sensor.begin() == false)
  {
    Serial.println("No SGP30 Detected. Check connections.");
  }
  co2Sensor.initAirQuality();
  int count = 0;
  //First fifteen readings will be
  //CO2: 400 ppm  TVOC: 0 ppb
  while (count <= 16)
  {
    co2Sensor.measureAirQuality();
    count++;
    delay(200);
    Serial.println(count);
  }

  strip.begin();
  strip.show();
  strip.setBrightness(BRIGHTNESS);
  colorWipe(strip.Color(0, 0, 0, 255), 50);
  delay(500);
  timeSinceShow = millis();

  activity();

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  // Prefill CO2 Logger
  for (int i = 0; i <= 120; i++)
  {
    //CO2.push((i*9)+400);
    CO2.push(400);
  }

  /*SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }*/
  Serial.println("Start");
}



int i = 2000;
int ringUpdate = 0;

void loop()
{
  if (allowUpdate)
  {
    allowUpdate = false;
    co2Sensor.measureAirQuality();
  }
  //Serial.println("LOOP");
  if (ringUpdate >= 5)
  {
    ringUpdate = 0;
    updateLEDring();
    //Serial.println("Update led ring");
  }
  ringUpdate++;
  server.handleClient();
  handleActivity();
  rotary_loop();
  delay(50);

  if (enableLogging)
  {
    executeLogAction();
  }

  if (i >= 2000)
  {
    //Serial.println(menuPage);
    if (handleWindows())
    {
      if (menuPage == "0.0") // Just temperature
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
      if (menuPage == "1.0") // Just co2 + TVOC
      {
        //Serial.println(" SENSOR: " + String(co2Sensor.measureAirQuality()));
        isPagePressable = false;
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setTextSize(2);
        display.println("CO2: ");
        display.print(" ");
        display.print(co2Sensor.CO2);
        display.println(" ppm");
        //String str = "CO2: " + String(co2Sensor.CO2);
        display.println("TVOC: ");
        display.print(" ");
        display.print(co2Sensor.TVOC);
        display.println(" ppb");

        /*str = "TVOC: " + String(co2Sensor.TVOC);
        str += " ppb";
        display.println(str);*/
        display.display();
        i = 1990;
      }
      else if (menuPage == "2.0")
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
      else if (menuPage == "3.0")
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
      else if (menuPage == "4.0")
      { // Plot
        isPagePressable = true;
        plotGraph(graphToPlot);
        i = 1995;
      }

      if (menuPage == "3.1")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Einstellungen\nverlassen");
        display.drawXBitmap(48, 10, doorIcon_bits, doorIcon_height, doorIcon_width, WHITE);
        display.display();
      }
      if (menuPage == "3.2")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Start / Stop logging");
        display.println("State: " + enableLogging);
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = true;
      }
      if (menuPage == "3.3")
      {
        display.clearDisplay();
        display.invertDisplay(true);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Test Seite");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = false;
      }
      if (menuPage == "3.4")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Make a test popup");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = true;
      }
      if (menuPage == "3.5")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Test critical light");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = true;
      }
      if (menuPage == "3.6")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Party mode");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = true;
      }
      if (menuPage == "3.7")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Output whole log to console");
        display.drawXBitmap(48, 10, cog_wheel_bits, cog_wheel_height, cog_wheel_width, WHITE);
        display.display();
        isPagePressable = true;
      }
      if (menuPage == "3.8")
      {
        display.clearDisplay();
        display.invertDisplay(false);
        display.setTextSize(1);
        display.setCursor(16, 48);
        display.setTextColor(WHITE);
        display.println("Delete log");
        display.drawXBitmap(48, 10, warningSign_bits, warningSign_height, warningSign_width, WHITE);
        display.display();
        isPagePressable = true;
      }
    }
  }
  i++;
}
