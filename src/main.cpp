//#define CORE_DEBUG_LEVEL ARDUHAL_LOG_LEVEL_DEBUG
//#define DEBUG_ESP_HTTP_SERVER
#include <fs.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
//#include <DNSServer.h>
//#include <WiFiManager.h>         
//#include "SSD1306.h"
//#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <U8g2lib.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RtcDS3231.h>
//#include <ESP8266WebServer.h>
#include <BME280I2C.h>
#include <driver/adc.h>
//#include <AsyncTCP.h>
//#include <Hash.h>
//#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>     // Basis-Bilbiothek für die Adafruit Sensor Biblitheken (Bibliothek von Adafruit)
#include <Adafruit_TSL2561_U.h>  // Bibliothek für den Lichtsensor TSL2661 (Bibliothek von Adafruit)

#include <ESP8266TrueRandom.h> 

#include <tinyxml2.h>

#include "orderparametermessage.h"
#include "AnalogPHMeter.h"
#include "roomlabel.h"
#include "ph.h"
#include "photometer.h"
#include "exceptionmessage.h"
#include "QueueArray.h"

#include "myAutoConnect.h"

/*
#define XML  "application/xml\r\n"
#define JSON "application/json\r\n"
#define PAGE "text/html\r\n"
*/

#define MYIP        "{{MyIp}}"
#define TEXT        "{{Text}}"
#define SUCCESS     "{{Success}}"
#define CODE        "{{Code}}"
#define MESSAGENAME "{{MessageName}}"
#define MESSAGEURI  "{{MessageUri}}"
#define LINKS       "{{Links}}"
#define DEVICEINFO  "{{DeviceInfo}}"

#define USE_LittleFS
 


int ledPin[]              = {13, 12, 14};   // grüne LED an GPIO Pin 13/D7, rote LED an GPIO Pin 12/D6, blaue LED an GPIO Pin 14/D5 (hier für Lucky Light: LL-509RGBC2E-006)
#define sdaPin          = 21;              // SDA an GPIO/Pin  4 / D2   Anschluss-Pin für das SDA-Signal zur Datenkommunikation mit dem Lichtsensor
#define sclPin          = 22;              // SCL an GPIO/Pin  5 / D1   Anschluss-Pin für das SCL-Signal zur Datenkommunikation mit dem Lichtsensor

#define ADDRESS_LCD1   0x3F
#define ADDRESS_LCD2   0x27

#define ADDRESS_OLED   0x3C
#define ADDRESS_BME280 0x76
#define ADDRESS_RTC    0x68


/*

//esp8266
#define DHTPIN 7
#define TRIGGER_PIN  2
#define SDA_PIN 6
#define SCL_PIN 5
*/

//ESP32
#define DHT_PIN       23
#define TRIGGER_PIN1  21
#define TRIGGER_PIN2  22

#define SDA_PIN       19
#define SCL_PIN       18

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

#define UTC_OFFSET -2 //hours

#define LCDMAXCHARS 20
#define SENSORREADINTERVAL     5000
#define WATCHDOGTIME         120000 
#define DISPLAYUPDATEINTERVAL   200
#define MIN_FREE_SPIFFS      100000
#define LOGBUFFERSIZE         10000
#define MAXLOGFILESIZE       300000

using namespace tinyxml2;

float temperature = 0;
float humidity    = 0;
float pressure    = 0;


QueueArray<String> sendQueue;


RoomLabel roomLabel(LCDMAXCHARS);
PHMeter   pHMeter;
PhotoMeter photoMeter;

//works for my SSD1306 as well!
U8G2_SH1106_128X64_NONAME_F_HW_I2C * displayOled;
LiquidCrystal_I2C * displayLcd  =0;
DHT                 dht;
RtcDS3231<TwoWire>  rtc(Wire);
BME280I2C           bme;    // Default : forced mode, standby time = 1000 ms
                            // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345); 

bool rtcExists = false;
bool bmeExists = false;
bool dhtExists = false;
bool pHExists  = false;
bool Lcd1Exists = false;
bool Lcd2Exists = false;
bool OledExists  = false;
bool TSL2561Exists  = false;

enum eTypeOfDevice {eUnknown, 
                    eRoomLabel,
                    epH,
                    ePhotoMeter};

eTypeOfDevice myType = eUnknown;

RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

bool setupComplete = false;

unsigned long lastPasXInteraction = 0;
unsigned long pasXWatchDogTime    = WATCHDOGTIME;
String lastExceptionBatchID;
unsigned long lastExceptionSent=0;
const unsigned long exceptionSendInterval = 60000; //milliseconds

String logString;
bool logActive = false;

//WiFiServer server(80);     // std
//AsyncWebServer server(80); // async
WebServer server(80); 

//byte uuidNumber[16]; // UUIDs in binary form are 16 bytes long

const char PROGMEM configFileName[]  = "/config.json";
const char PROGMEM logFileName[]     = "/log.txt";

const char PROGMEM receivedMessageFileName[] = "/receivedmessage.xml";
const char PROGMEM sentMessageFileName[] = "/sentmessage.xml";

//const char PROGMEM pHCalibFileName[] = "/pHcalib.json";

bool ledStatus = false;

const char PROGMEM cStaticIp[16] = "192.168.178.254";
const char PROGMEM cStaticGw[16] = "10.0.1.1";
const char PROGMEM cStaticSn[16] = "255.255.255.0";

const char PROGMEM ApIp[]       = "192.168.4.1";
const char PROGMEM ApSsid[]     = "WerumMsiAP";
const char PROGMEM ApInfoText[] = "Config mode";


String staticIp(cStaticIp);
String staticGateway(cStaticGw);
String staticSubnetMask(cStaticSn);


const char PROGMEM token[] = "{\"access_token\": \"tfrzhjuiopiuztrdfpokiuftgzhju70987654erfgh\", \"token_type\": \"bearer\", \"expires_in\": 86399}";

const char PROGMEM transferResult[] = "<TransferResult xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
                                          "<HasError>false</HasError>"
                                          "<Success>"
                                          SUCCESS
                                          "</Success>"
                                          "<Error>"
                                              "<Code>"
                                              CODE
                                              "</Code>"
                                              "<Text>"
                                              TEXT
                                              "</Text>"
                                          "</Error>"
                                      "</TransferResult>";



const char PROGMEM descriptionLink[] =  "<a href=\""
                                        MESSAGEURI
                                        "\">"
                                        MESSAGENAME
                                        "</a></p>";


const char PROGMEM mainPage[]  = 
        "<html><head>"
          "<style>"
          "table, th, td {"
              "border: 1px solid black;"
              "border-collapse: collapse;"
          "}"
          "</style>"
          "</head><body>"
         "<title>MSI embedded tech demo</title></head><body><h1>MSI embedded tech demo</h1>"
         "<p><h2>Message description</h2>"
         LINKS
         "<p><h2>Adapter config</h2></p>"
         "<p><h3>Get secure token</h3><a href=\"/getsecuretoken\">http://"
         MYIP
         "/getsecuretoken</a></p>"
         "<p><h3>Get next message</h3><a href=\"/GetNextMessage\">http://"
         MYIP
         "/GetNextMessage</a></p>"
         "<p><h3>Post message</h3><a href=\"/PostMessage\">http://"
         MYIP
         "/PostMessage</a></p>"
         "</body></html>"
         "<p><h2>Debug</h2></p>"
         "<p><a href=\"/lastsentmessage\">Last sent message</a></p>"
         "<p><a href=\"/lastreceivedmessage\">Last received message</a></p>"
         "<p><a href=\"/log\">logfile</a></p>"
         "<p><a href=\"/resetlog\">reset log file</a></p>"
         "<p><a href=\"/activatelog\">activate log</a></p>"
         "<p><a href=\"/deactivatelog\">deactivate log</a></p>"
         "<p><a href=\"/watchdog\">activate watchdog</a></p>"
         "<p><a href=\"/nowatchdog\">deactivate watchdog</a></p>"
         "<p><a href=\"/configfile\">config file</a></p>"
         "<p><a href=\"/phcalibfile\">pH Calibration file</a></p>"
         "<p><a href=\"/lcdfile\">lcd file</a></p>"
         "<p><h2>Device Info</h2>"
         DEVICEINFO
         "</body></html>"
         ;



#define countof(a) (sizeof(a) / sizeof(a[0]))
String getDateTimeString(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%04u-%02u-%02u %02u:%02u:%02u"),
            dt.Year(),
            dt.Month(),
            dt.Day(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    //Serial.print(datestring);
    return (String(datestring));
}

String getUTCString(const RtcDateTime& dt)
{
    char datestring[24];

    RtcDateTime UTC = dt;
    UTC = UTC+ (UTC_OFFSET *60 *60);
    

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%04u-%02u-%02u %02u:%02u:%02u,000"),
            UTC.Year(),
            UTC.Month(),
            UTC.Day(),
            UTC.Hour(),
            UTC.Minute(),
            UTC.Second() );
    //Serial.print(datestring);
    return (String(datestring));
}

String getNow()
{
  RtcDateTime now;
  if (rtcExists)
  {
     now = rtc.GetDateTime();
  }
  
  return getDateTimeString(now);  
}

String getUTC()
{
  RtcDateTime now;
  if (rtcExists)
  {
     now = rtc.GetDateTime();
  }
  
  return getUTCString(now);  
}

void mylog(const char * text, bool forceWrite = false)
{
  logString += text;

  if (logActive)
  {
    if (forceWrite || (logString.length() > LOGBUFFERSIZE))
    {
      if (SPIFFS.totalBytes()-SPIFFS.usedBytes() > MIN_FREE_SPIFFS)
      {
        File f = SPIFFS.open(logFileName, "a");
        if (!f) 
        {
          Serial.println("Failed to open log file for writing");
        }
        else
        {
          if (f.size()<MAXLOGFILESIZE)
          {
            f.print(logString.c_str());
            f.close();
          }
        }
      }
      else
      {
        Serial.println("SPIFFS full!");
      }
      logString="";
    }  
  }
  else
  {
    // check for string overflow
    if (logString.length() > LOGBUFFERSIZE)
    {
      logString = logString.substring(logString.length()-LOGBUFFERSIZE);
    }
  }
  Serial.print(text);
}

void mylog(const String& text, bool forceWrite = false)
{
  mylog(text.c_str(), forceWrite);
}

void mylog(double d, bool forceWrite = false )
{
  mylog(String(d), forceWrite);
}

void mylog(unsigned long l, bool forceWrite = false )
{
  mylog(String(l), forceWrite);
}

void mylog(bool b, bool forceWrite = false )
{
  mylog(String(b), forceWrite);
}

void mylog(int i, bool forceWrite = false )
{
  mylog(String(i), forceWrite);
}

void addTableRow(String& table, const char* col1, const char* col2)
{
  if (col1 && col2)
  {
    table += "<tr><td>";
    table += col1;
    table += "</td><td>";
    table += col2;
    table += "</td><tr>";
  }
}

void getDeviceInfo(String& deviceInfo)
{
  deviceInfo = "<table>";
  addTableRow(deviceInfo, "Software date",    getDateTimeString(compiled).c_str());
  addTableRow(deviceInfo, "RTC",              rtcExists ? "yes":"no");
  addTableRow(deviceInfo, "BME280",           bmeExists ? "yes":"no");
  addTableRow(deviceInfo, "DHT22",            dhtExists ? "yes":"no");
  addTableRow(deviceInfo, "ph Sensor",        pHExists  ? "yes":"no");
  addTableRow(deviceInfo, "TSL2561 Sensor",   TSL2561Exists  ? "yes":"no");//ALe add Photometer
  addTableRow(deviceInfo, "LCD 1 (0x3F)",     Lcd1Exists ? "yes":"no");
  addTableRow(deviceInfo, "LCD 2 (0x27)",     Lcd2Exists ? "yes":"no");
  addTableRow(deviceInfo, "OLED",             OledExists  ? "yes":"no");
  addTableRow(deviceInfo, "Free heap",        String(ESP.getFreeHeap()).c_str());
  addTableRow(deviceInfo, "CPU Speed (MHz)",  String(ESP.getCpuFreqMHz()).c_str());
  addTableRow(deviceInfo, "Host name",        WiFi.getHostname());
  addTableRow(deviceInfo, "MAC adress",       WiFi.macAddress().c_str());
  addTableRow(deviceInfo, "IP",               WiFi.localIP().toString().c_str());
  addTableRow(deviceInfo, "SSID",             WiFi.SSID().c_str());
  addTableRow(deviceInfo, "RSSI",             String(WiFi.RSSI()).c_str());
  addTableRow(deviceInfo, "SPIFFS total",     String(SPIFFS.totalBytes()).c_str());
  addTableRow(deviceInfo, "SPIFFS used",      String(SPIFFS.usedBytes()).c_str());
  addTableRow(deviceInfo, "SPIFFS available", String(SPIFFS.totalBytes()-SPIFFS.usedBytes()).c_str());
  addTableRow(deviceInfo, "log file size",    String(SPIFFS.open(logFileName,"r").size()).c_str());
  addTableRow(deviceInfo, "log buffer size",  String(LOGBUFFERSIZE).c_str());
  addTableRow(deviceInfo, "log buffer used",  String(logString.length()).c_str());
  addTableRow(deviceInfo, "log active",       String(logActive).c_str());
  addTableRow(deviceInfo, "watchdog time (s)",  String(pasXWatchDogTime/1000).c_str());

  deviceInfo += "</table>";
}



bool readSensor()
{
  bool success = false;
  if (myType == eRoomLabel)
  {
    float temp(NAN), hum(NAN), pres(NAN);
    
    if (dhtExists)
    {
      //mylog("dht:");
      temp = dht.getTemperature();
      hum  = dht.getHumidity();
      
      //mylog(dht.getStatusString());
      
    }
    else if (bmeExists)
    {
        BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
        BME280::PresUnit presUnit(BME280::PresUnit_hPa);
      
        bme.read(pres, temp, hum, tempUnit, presUnit);
    }
    else
    {
      return success;
    }

    // only take over plausible values
    if (!isnan(temp) 
    && !isnan(hum) 
    && temp >-50.0 
    && hum  >0.0)
    {
      temperature = temp;
      humidity    = hum;
      success = true;
      if (!isnan(pres))
      {
        pressure = pres;
      }
    }
  }
  return success;
}

String getMainPage()
{
  String page = mainPage;
  String deviceInfo;
  getDeviceInfo(deviceInfo);

  page.replace(MYIP, WiFi.localIP().toString());
  page.replace(DEVICEINFO,deviceInfo);

  String links;

  if (myType == eRoomLabel)
  {
    for (int i=0; i<roomLabel.getMessageDescriptionCount(); i++)
    {
      String link = descriptionLink;
      link.replace(MESSAGEURI, String("/")+roomLabel.getMessageDescriptionId(i));
      link.replace(MESSAGENAME, roomLabel.getMessageDescriptionText(i));
      links += link;
    }
  }
  else if (myType==epH)
  {
    for (int i=0; i<pHMeter.getMessageDescriptionCount(); i++)
    {
      String link = descriptionLink;
      link.replace(MESSAGEURI, String("/")+pHMeter.getMessageDescriptionId(i));
      link.replace(MESSAGENAME, pHMeter.getMessageDescriptionText(i));
      links += link;
    }
  }
  else if (myType==ePhotoMeter)
  {
    for (int i=0; i<photoMeter.getMessageDescriptionCount(); i++)
    {
      String link = descriptionLink;
      link.replace(MESSAGEURI, String("/")+photoMeter.getMessageDescriptionId(i));
      link.replace(MESSAGENAME, photoMeter.getMessageDescriptionText(i));
      links += link;
    }
  }
  page.replace(LINKS, links);

  return page;
}

String getTransferResultText(bool Ok, const char * errorText)
{
  String result = transferResult;
  result.replace(CODE, Ok?"0":"-1");
  result.replace(TEXT, errorText);
  result.replace(SUCCESS, Ok?"true":"false");
  return result;
}

void toggleLED()
{
  ledStatus = ! ledStatus;
  digitalWrite(LED_BUILTIN, ledStatus);
}

void getServerRequest(String& message)
{
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (int i=0; i<server.args(); i++)
    {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    message += "\nHeader: ";
    message += server.headers();
    message += "\n";
    for (int i=0; i<server.headers(); i++)
    {
      message += " " + server.headerName(i) + ": " + server.header(i) + "\n";
    }
  
}

void notFound()
{
  bool foundValidURI = false;

  String messageDescription;
  String systemId;

  if (myType == eRoomLabel)
  {
    systemId="Display_" OP_SYSTEMID "_System"; 
    systemId.replace(OP_SYSTEMID,String(WiFi.localIP()[3]));
    for (int i=0; i<roomLabel.getMessageDescriptionCount(); i++)
    {
      if (String(String("/")+roomLabel.getMessageDescriptionId(i)).equals(server.uri()))
      {
        foundValidURI = true;
        messageDescription = roomLabel.getMessageDescription(i);
        messageDescription.replace(OP_SYSTEMID,systemId);

        server.send(200, "text/xml", messageDescription.c_str());
      }
    }
  }
  else if (myType == epH)
  {
    if (!foundValidURI)
    {
      systemId="pH_" OP_SYSTEMID "_System"; 
      systemId.replace(OP_SYSTEMID,String(WiFi.localIP()[3]));
      for (int i=0; i<pHMeter.getMessageDescriptionCount(); i++)
      {
        if (String(String("/")+pHMeter.getMessageDescriptionId(i)).equals(server.uri()))
        {
          foundValidURI = true;
          messageDescription = pHMeter.getMessageDescription(i);
          messageDescription.replace(OP_SYSTEMID,systemId);

          server.send(200, "text/xml", messageDescription.c_str());
        }
      }
    }
  }
  else if (myType == ePhotoMeter)
  {
    if (!foundValidURI)
    {
      systemId="PM_" OP_SYSTEMID "_System"; 
      systemId.replace(OP_SYSTEMID,String(WiFi.localIP()[3]));
      for (int i=0; i<photoMeter.getMessageDescriptionCount(); i++)
      {
        if (String(String("/")+photoMeter.getMessageDescriptionId(i)).equals(server.uri()))
        {
          foundValidURI = true;
          messageDescription = photoMeter.getMessageDescription(i);
          messageDescription.replace(OP_SYSTEMID,systemId);

          server.send(200, "text/xml", messageDescription.c_str());
        }
      }
    }
  }
  if (!foundValidURI)
  {
    String message = "File Not Found\n\n";
    getServerRequest(message);

    server.send(404, "text/plain", message);
  }

}




bool readConfigFile() 
{
  File f = SPIFFS.open(configFileName, "r");
  
  if (!f) 
  {
    mylog("Configuration file not found");
    return false;
  } 
  else 
  {
    size_t size = f.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    f.readBytes(buf.get(), size);
    f.close();
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    if (!json.success()) 
    {
      mylog(configFileName);
      mylog("JSON parseObject() failed\n");
      return false;
    }
    json.prettyPrintTo(Serial);

    if (json.containsKey("ip")) 
    {
      staticIp = (const char*)json["ip"];      
    }
    if (json.containsKey("gw")) 
    {
      staticGateway = (const char*)json["gw"];      
    }
    if (json.containsKey("sn")) 
    {
      staticSubnetMask = (const char*)json["sn"];      
    }
    
  }
  mylog("\nConfig file was successfully parsed\n");
  
  return true;
}


bool writeConfigFile() 
{
   
  mylog("Saving config file");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  // JSONify local configuration parameters
  json["ip"] = staticIp.c_str();
  json["gw"] = staticGateway.c_str();
  json["sn"] = staticSubnetMask.c_str();
    
  return OrderParameterMessage::writeJsonFile(configFileName, json);
}
  
bool existsI2c(byte address)
{
  byte error;
  // The i2c_scanner uses the return value of
  // the Write.endTransmisstion to see if
  // a device did acknowledge to the address.

  Wire.beginTransmission(address);
  error = Wire.endTransmission();
  if (error == 0)
  {
    return true;
  }
  else if (error==4)
  { 
    mylog("Unknow error at address 0x");
    if (address<16)
      Serial.print("0");
    Serial.println(address,HEX);
    return false;
  }

  return false;
}

int scanI2c()
{
  byte  address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    if (existsI2c(address))
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println(" !");
      nDevices++;
    }

  }
  return nDevices;
}
//const uint8_t  * font9 = u8g2_font_crox1h_tf;
//const uint8_t  * font9 = u8g2_font_t0_11_tf; 
const uint8_t  * font9 = u8g2_font_profont11_tf ;
//const uint8_t  * font9 = u8g2_font_profont10_tf;
const uint8_t  * font20 = u8g2_font_logisoso20_tf;
const uint8_t  * font24 = u8g2_font_logisoso24_tf;
const uint8_t  * font30 = u8g2_font_logisoso30_tf;
const uint8_t  * font38 = u8g2_font_logisoso38_tf;


static void displayOledScreen(const char * SSID = NULL, const char * IP=NULL, const char* infoText=NULL)
{
  if (displayOled)
  {
    displayOled->clearBuffer();
   
    if (displayLcd || !setupComplete) //if we have an extra LCD...
    {
      // if there is a LCD, the oled is for information only,.
      // so we show all sorts of gereral information
      displayOled->setFont(font9);
      displayOled->drawStr( 00, 00, infoText ? infoText : WiFi.isConnected() ? "connected" : "connecting...");  

      displayOled->drawStr( 00, 10, "ID:");  
      displayOled->drawStr( 20, 10, WiFi.macAddress().c_str());  

      displayOled->drawStr( 00, 20, "MyIP:");
      displayOled->drawStr( 00, 30, "SSID:");  

      displayOled->drawStr( 30, 20, IP   ? IP   : WiFi.localIP().toString().c_str());
      displayOled->drawStr( 30, 30, SSID ? SSID : WiFi.SSID().c_str());  

      if (myType == eRoomLabel)
      {
        displayOled->drawStr( 0, 40, "Temp");
        displayOled->drawStr( 30, 40, String(temperature).c_str());
        displayOled->drawStr( 64, 40, "Hum");  
        displayOled->drawStr( 90, 40, String(humidity).c_str());  
      }
      else if (myType== epH)
      {
        int val = adc1_get_raw(ADC1_CHANNEL_0);
        displayOled->drawStr( 0, 40, String(val).c_str());
      }
      else if (myType== ePhotoMeter)
      {
        // #todo
      }
      displayOled->drawStr(  0, 50, getNow().c_str());  

    }
    else if (myType == eRoomLabel)
    {
    
      displayOled->setFont(font24);
      displayOled->drawStr(  0, 00, "T:");
      displayOled->drawStr( 30, 00, String(temperature).c_str());
      displayOled->drawStr(  0, 40, "H:");  
      displayOled->drawStr( 30, 40, String(humidity).c_str());  

    }
    else if (myType == epH)
    {

      displayOled->setFont(font9);
      /*
      displayOled->drawStr( 00, 00, "IP:");  
      displayOled->drawStr( 20, 00, WiFi.localIP().toString().c_str());  
      */
      //unsigned int i = adc1_get_raw(ADC1_CHANNEL_0);
      /*
      unsigned long i = pHMeter.readADC();

      displayOled->drawStr( 00, 00, "ADC:");  
      displayOled->drawStr( 30, 00, String(i).c_str());  

      displayOled->drawStr( 80, 00, String(i*3770/4096).c_str());  
      */
      //displayOled->setFont(font38);
      displayOled->setFont(font20);
      displayOled->drawStr(  0, 20, "pH:");
      //displayOled->drawStr( 64, 9, pHMeter.getpHString().c_str());
      displayOled->setFont(font30);
      displayOled->drawStr( 35, 9, pHMeter.getpHString().c_str());
    }
    else if (myType== ePhotoMeter)
    {
      // #todo
    }
  
    displayOled->sendBuffer();

  }

}

void blinkLcdScreen(unsigned long ms)
{
  displayLcd->noBacklight();
  delay(ms);
  displayLcd->backlight();
}

void displayLcdScreen(const String& line1, 
                      const String& line2, 
                      const String& line3, 
                      const String& line4, 
                      bool enabled)
{
    displayLcd->setCursor(0,0);
    displayLcd->print(line1);
    displayLcd->setCursor(0,1);
    displayLcd->print(line2);
    displayLcd->setCursor(0,2);
    displayLcd->print(line3);
    displayLcd->setCursor(0,3);
    displayLcd->print(line4);
    
    if (enabled)
    {
      displayLcd->backlight();
    }
    else
    {
      displayLcd->noBacklight();
    }

}


void displayLcdScreen(const char * SSID = NULL, const char * IP=NULL, const char* infoText=NULL)
{
  if (displayLcd)
  {
    String line1;
    String line2;
    String line3;
    String line4;
    bool enabled = true;
    if (setupComplete)
    {
      roomLabel.getLcdData(line1, line2, line3, line4, enabled);
    }
    else
    {
      line1 = roomLabel.makeLCDLine(WiFi.macAddress().c_str(),"ID:");
      line2 = roomLabel.makeLCDLine(IP       ? IP       : WiFi.localIP().toString().c_str(), "MyIP:");
      line3 = roomLabel.makeLCDLine(SSID     ? SSID     : WiFi.SSID().c_str(), "SSID:");
      line4 = roomLabel.makeLCDLine(infoText ? infoText : WiFi.isConnected() ? "connected" : "connecting...", "");
      enabled = true;
    }

    displayLcdScreen (line1, 
                      line2, 
                      line3, 
                      line4, 
                      enabled);

 /*    
    DisplayLcd.print(getDateTimeString(getNow()));
  */  
  }
}


int todo2; // show "connecting"

/*
void wifiManagerConnectCallback (WiFiManager *myWiFiManager) 
{
  mylog("\nwifiManagerConnectCallback: ");
  String SSID = myWiFiManager->getSSID();
  if (SSID.length()==0)
  {
    SSID = WiFi.SSID();
  }

  displayLcdScreen(SSID.c_str(), myWiFiManager->getIp().toString().c_str(), "Connecting...");
  displayOledScreen(SSID.c_str(), myWiFiManager->getIp().toString().c_str(), "Connecting...");
  mylog("connecting to ");
  mylog(SSID);
  mylog("IP ");
  mylog(myWiFiManager->getIp().toString());
  mylog("\n");
}
*/


bool onConfigMode(IPAddress softapip) 
{
  mylog("onConfigMode: ");
  displayLcdScreen(ApSsid, ApIp, ApInfoText);
  displayOledScreen(ApSsid, ApIp, ApInfoText);
  return true;
}

/*

void wifiManagerSaveConfigCallback () 
{
  mylog("wifiManagerSaveConfigCallback ");
  staticIp = WiFi.localIP().toString();
  staticGateway = WiFi.gatewayIP().toString();
  staticSubnetMask = WiFi.subnetMask().toString();
  writeConfigFile();
}

*/

void wifiStart()
  {
    bool forcePortal = false;
    /*
    WiFiManager WifiManager;
    
    WifiManager.setConnectCallback(wifiManagerConnectCallback);
    WifiManager.setAPCallback(wifiManagerConfigModeCallback);
    WifiManager.setSaveConfigCallback(wifiManagerSaveConfigCallback);
    WifiManager.setConfigPortalTimeout(120);

    
    //set static ip
  
    IPAddress _ip,_gw,_sn;
    _ip.fromString(staticIp);
    _gw.fromString(staticGateway);
    _sn.fromString(staticSubnetMask); 
    WifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
*/
  

    if (digitalRead(TRIGGER_PIN1) == LOW)
    {
      mylog("Forcing ConfigPortal\n");
      forcePortal = true;
    }  

/*
    do
    {
      WifiManager.autoConnect(ApSsid); 
      
      if(!WiFi.isConnected())
      {
        mylog("\n\n\nWifiManager autoconnect failed, trying again\n");
        delay(100);
        ESP.restart();
      }
    }
    while (!WiFi.isConnected());
 */

    if (!autoConnectWifi(onConfigMode, forcePortal, ApSsid))
    {
      ESP.restart();
    }



    Serial.println("back from  autoconnect");

    String hostName = String(F("MSI")) + String((unsigned long)ESP.getEfuseMac()); 

    mylog("MySSID:");
    mylog(WiFi.SSID());
    mylog("\nMyIP:");
    mylog(WiFi.localIP().toString());
    mylog("\nMyMac:");
    mylog(WiFi.macAddress());
    WiFi.setHostname(hostName.c_str());
    mylog("\nMyHostName:");
    mylog(WiFi.getHostname());
    

  }

bool processMessage(const String& content, String & errorMessage)
{   
  bool success = false;
  
  String localMessageId;
  String localDeviceTypeId;
  OrderParameterMessage::getMessageIdFromXml(content, localMessageId, localDeviceTypeId);
  mylog(localMessageId);
  if (localMessageId.length())
  {
    if (roomLabel.hasThisMessageId(localMessageId.c_str(), localDeviceTypeId.c_str()))
    {
      //Serial.print(" is a RoomLabel message, success:");
      success = roomLabel.parseXml(content, errorMessage);
      mylog(success);
      mylog(" MessageID:");
      mylog(roomLabel.messageId);
      
      if (roomLabel.hasMessageToSend())
      {
        mylog("hasMessage");

        if (roomLabel.messageId.equals("BurstRequest"))
        {
          int num = roomLabel.numberOfMessages;
          mylog("\nSending burstmessages:\n");

          for (int i =0; i<num; i++)
          {
            if (!sendQueue.isFull())
            {
              mylog(i);
              mylog("\n");
              String response = roomLabel.getBurstMessageString(i);
              sendQueue.push(response);
            }
          }
        }
        else
        {


          // we ignore the fact that pressure might be NAN, handled by room label 
          String response = roomLabel.getNextMessageString(temperature, 
                                                          humidity,
                                                          pressure); 

          //Serial.print("QueueSize:");
          //Serial.print(sendQueue.count());

          //Serial.print("--push--");
          sendQueue.push(response);
          
          //Serial.print("->QueueSize:");
          //Serial.println(sendQueue.count());

        }
        roomLabel.setHasMessageToSend(false);
        //Serial.print(getNow());
      }

    }
    else if (pHMeter.hasThisMessageId(localMessageId.c_str(), localDeviceTypeId.c_str()))
    {
      success = pHMeter.parseXml(content, errorMessage);
      mylog("phMeter.parseXml:");
      mylog(success);
      
      mylog( " hasMessageToSend:");
      if (pHMeter.hasMessageToSend())
      {
        mylog( "true");
        String response = pHMeter.getNextMessageString(); 
        sendQueue.push(response);
        pHMeter.setHasMessageToSend(false);
      }
      else
      {
        mylog( "false");
      }

    }    /*
    else if (newclass::::hasThisMessageId(messageId))
    {
      newclass.parseXml(content,errorMessage);
    }
    */
    else
    {
      errorMessage = String("The message ID \"") + localMessageId +"\" is not supported by this device.";
    }

    if (success)
    {
      //Save XML for next start
      File f = SPIFFS.open(receivedMessageFileName, "w");
      if (!f) 
      {
        mylog("Failed to open message file for writing\n");
      }
      else
      {
        f.print(content.c_str());
        f.close();
      }
    }
  }
  mylog(errorMessage);
  mylog(".\n");
  return success;
}
/*
void readXmlFromFS()
{
  File f = SPIFFS.open(receivedMessageFileName, "r");
  if (!f) 
  {
    Serial.println("receivedMessage not found");
    return;
  } 
  else 
  {
    // we could open the file
    size_t size = f.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size+1]);

    // Read and store file contents in buf
    f.readBytes(buf.get(), size);
    buf[size]=0; //terminating zero
    // Closing file
    f.close();
    //Serial.println(buf.get());
    Serial.println("Messagefile successfully read.");
    String errorText;
    processMessage(String(buf.get()), errorText);
    roomLabel.setHasMessageToSend(false); // do not fire a repsonse
  }
}
*/

unsigned long lastGetNextMessage = 0;
unsigned long lastPostMessage = 0;

void serverStart()
{
  
  server.on("/", HTTP_GET, [](){
    unsigned long start = millis();
    mylog("MainPage:");
    server.send(200, "text/html", getMainPage());  
    mylog(millis()-start);
    mylog("ms\n");
    
  });

  server.on("/getsecuretoken", HTTP_POST, [](){
      unsigned long start = millis();
      
    /*
      String message = "GetSecureToken:";
      getServerRequest(message);
      mylog(message);
    */
      server.send(200, "application/JSON", token);
      lastPasXInteraction = millis();
      mylog(millis()-start);
      mylog("ms\n");
  });

  server.on("/nowatchdog", HTTP_GET, [](){
      Serial.println("nowatchdog");
      server.send(200, "text/PLAIN", "watchdog disabled");
      pasXWatchDogTime = 1000000000L;
  });

  server.on("/watchdog", HTTP_GET, [](){
      Serial.println("watchdog");
      server.send(200, "text/PLAIN", "watchdog enabled");
      pasXWatchDogTime = WATCHDOGTIME;
  });
  server.on("/resetlog", HTTP_GET, [](){
      SPIFFS.remove(logFileName);
      mylog(getNow());
      mylog("\nresetlog\n");
      server.send(200, "text/PLAIN", "log reset");
  });
  server.on("/activatelog", HTTP_GET, [](){
      logActive=true;
      mylog(getNow());
      mylog("\nactivate log\n");
      server.send(200, "text/PLAIN", "log activated");
  });
  server.on("/deactivatelog", HTTP_GET, [](){
      mylog(getNow());
      mylog("\ndeactivate log\n");
      logActive=false;
      server.send(200, "text/PLAIN", "log deactivated");
  });

  server.on("/configfile", HTTP_GET, [](){
      unsigned long start = millis();
      mylog("configfile", true);

      File f = SPIFFS.open(configFileName, "r");
      if (!f || f.size()==0) 
      {
        Serial.println("configfile not found");
        server.send(404, "text/PLAIN", "configfile not found");
      } 
      else 
      {
        server.streamFile(f, "text/PLAIN");
      }
      
      mylog(millis()-start);
      mylog("ms\n");
  });

  server.on("/phcalibfile", HTTP_GET, [](){
      unsigned long start = millis();
      mylog("configfile", true);

      File f = SPIFFS.open(pHCalibFileName, "r");
      if (!f || f.size()==0) 
      {
        Serial.println("pHCalibFile not found");
        server.send(404, "text/PLAIN", "pHCalibFile not found");
      } 
      else 
      {
        server.streamFile(f, "text/PLAIN");
      }
      
      mylog(millis()-start);
      mylog("ms\n");
  });

  server.on("/lcdfile", HTTP_GET, [](){
      unsigned long start = millis();
      mylog("lcdfile", true);

      File f = SPIFFS.open(lcdFileName, "r");
      if (!f || f.size()==0) 
      {
        Serial.println("lcdFile not found");
        server.send(404, "text/PLAIN", "lcdFile not found");
      } 
      else 
      {
        server.streamFile(f, "text/PLAIN");
      }
      
      mylog(millis()-start);
      mylog("ms\n");
  });

  server.on("/log", HTTP_GET, [](){
      unsigned long start = millis();
      mylog("log", true);
      if (logActive)
      {
        File f = SPIFFS.open(logFileName, "r");
        if (!f || f.size()==0) 
        {
          Serial.println("logfile not found");
          server.send(404, "text/PLAIN", "logfile not found");
        } 
        else 
        {
          server.streamFile(f, "text/PLAIN");
        }
      }
      else
      {
        server.send(200, "text/PLAIN", logString);
      }
      
      mylog(millis()-start);
      mylog("ms\n");
  });

  server.on("/lastsentmessage", HTTP_GET, [](){
      unsigned long start = millis();
      Serial.println("lastsentmessage");
      File f = SPIFFS.open(sentMessageFileName, "r");
      if (!f || f.size()==0) 
      {
        Serial.println("Messagefile not found");
        server.send(404, "application/xml", "Messagefile not found");
      } 
      else 
      {
        // we could open the file
        size_t size = f.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size+1]);

        // Read and store file contents in buf
        f.readBytes(buf.get(), size);
        buf[size]=0; //terminating zero
        // Closing file
        f.close();
        //Serial.println(buf.get());
        server.send(200, "application/XML", buf.get());
      }
    Serial.print(millis()-start);
    Serial.println("ms");
  });

  server.on("/lastreceivedmessage", HTTP_GET, [](){
     unsigned long start = millis();
     Serial.println("lastreceivedmessage");
      File f = SPIFFS.open(receivedMessageFileName, "r");
      if (!f) 
      {
        Serial.println("Messagefile not found");
        server.send(404, "text/plain", "Messagefile not found");
      } 
      else 
      {
        // we could open the file
        size_t size = f.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size+1]);

        // Read and store file contents in buf
        f.readBytes(buf.get(), size);
        buf[size]=0; //terminating zero
        // Closing file
        f.close();
        //Serial.println(buf.get());
        server.send(200, "application/XML", buf.get());
      }
    Serial.print(millis()-start);
    Serial.println("ms");
  });


  server.on("/GetNextMessage", HTTP_GET, [](){
    unsigned long start = millis();
    mylog(String("\n")+getNow() + ": ");
    mylog(start-lastGetNextMessage);
    mylog("ms<->");
    
    lastGetNextMessage = start;
    String message = "GetNextMessage";
    //getServerRequest(message);
    mylog(message);

    String response;
    toggleLED();
    
    if (!sendQueue.isEmpty())
    {
      mylog("Messages in queue:");
      mylog(sendQueue.count());
      mylog("\n");
/*
      Serial.print(getNow());
      Serial.print("QueueSize:");
      Serial.print(sendQueue.count());
      Serial.print("--pop--");
*/      
      response = sendQueue.pop();

      //Serial.print("->QueueSize:");
      //Serial.print(sendQueue.count());

       //Save XML
      File f = SPIFFS.open(sentMessageFileName, "w");
      if (!f) 
      {
        mylog("Failed to open message file for writing");
      }
      else
      {
        f.print(response.c_str());
        f.close();
      }

    }
    else 
    {
      response = "<NoMessage/>";
    }
    toggleLED();
    server.send(200, "application/XML", response);
    mylog(".");
    lastPasXInteraction = millis();
    mylog(millis()-start);
    mylog("ms\n");
  });

  server.on("/PostMessage", HTTP_POST, [] () {
    unsigned long start = millis();
    String message = "PostMessage:";
    mylog(String("\n")+getNow() + ": ");
    mylog(start-lastPostMessage);
    mylog("ms<->");
    
    lastPostMessage = start;
    //getServerRequest(message);
    Serial.println(message);

    toggleLED();
    Serial.println(getNow());
    String errorString;
    bool Ok= false;
    if (server.args())
    {
      String xml = server.arg(0);
      int xmlStart = xml.indexOf('<');
      int xmlEnd   = xml.lastIndexOf('>');
    
      if (xmlEnd>=0 && xmlStart>=0)
      {
        xmlEnd++; //including the >
        xml = xml.substring(xmlStart, xmlEnd);
        if (xml.length()>0)
        {
          Ok = processMessage(xml, errorString);
          
          //Save XML
          File f = SPIFFS.open(receivedMessageFileName, "w");
          if (!f) 
          {
            Serial.println("Failed to open message file for writing");
          }
          else
          {
            f.print(xml.c_str());
            f.close();
          }

        }
      }
    }

    if (Ok)
    {
      displayLcdScreen();
      server.send(200, "application/XML", getTransferResultText(Ok, errorString.c_str()));
    }
    else
    {
      Serial.println(".fail");
      server.send(200, "application/XML", getTransferResultText(Ok, errorString.c_str()));
      Serial.println(errorString.c_str());
    }
    Serial.print(".");
    lastPasXInteraction = millis();
    Serial.print(millis()-start);
    Serial.println("ms");
  });


  server.on("/burst", HTTP_GET, [](){
    unsigned long start = millis();
    mylog("burst:");
    server.send(200, "text/html", getMainPage());  

    if (myType == eRoomLabel)
    {
      //roomLabel.burst();
    }

    mylog(millis()-start);
    mylog("ms\n");
    
  });


  server.onNotFound(notFound);
  server.begin();
}




void oledStart()
{

  if (existsI2c(ADDRESS_OLED) && !displayOled)
  {
    OledExists=true;

    //displayOled = new SSD1306Wire(ADDRESS_OLED, SDA_PIN, SCL_PIN);    
    displayOled = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
    // set default font
    displayOled->setFont(font9);
    displayOled->setFontPosTop();
    mylog("Found oled\n");
  }

  if (displayOled)
  {
    displayOled->begin();
  }
}

void lcdStart()
{
  if (!displayLcd)
  {
    if (existsI2c(ADDRESS_LCD1))
    {
      Lcd1Exists=true;
      mylog("Using LCD 1\n");
      displayLcd = new LiquidCrystal_I2C(ADDRESS_LCD1,LCDMAXCHARS,4);
    }
    else if (existsI2c(ADDRESS_LCD2))
    {
      Lcd2Exists=true;
      mylog("Using LCD 2\n");
      displayLcd = new LiquidCrystal_I2C(ADDRESS_LCD2,LCDMAXCHARS,4);
    }
  }

  if (displayLcd)
  {
    displayLcd->init();
    displayLcd->backlight();
    displayLcdScreen();
  }
}

void RTCStart() 
{
  if (existsI2c(ADDRESS_RTC))
  {
    rtcExists = true;
    mylog("found RTC\n");
    rtc.Begin();
    if (!rtc.IsDateTimeValid()) 
    {
        // Common Cuases:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        mylog("RTC lost confidence in the DateTime!\n");

        // following line sets the RTC to the date & time this sketch was compiled
        // it will also reset the valid flag internally unless the Rtc device is
        // having an issue

        rtc.SetDateTime(compiled);
    }

    if (!rtc.GetIsRunning())
    {
        mylog("RTC was not actively running, starting now\n");
        rtc.SetIsRunning(true);
    }

    RtcDateTime now = rtc.GetDateTime();
    if (now < compiled) 
    {
        mylog("RTC is older than compile time!  (Updating DateTime)\n");
        rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        mylog("RTC is newer than compile time. (this is expected)\n");
        mylog(getDateTimeString(now));
    }
    else if (now == compiled) 
    {
        mylog("RTC is the same as compile time! (not expected but all is fine)\n");
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    rtc.Enable32kHzPin(false);
    rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
  }
  else
  {
    mylog("NO RTC FOUND!\n");
  }
}

void dhtStart()
{
  mylog("Looking for DHT:");
  dht.setup(DHT_PIN);
  unsigned long waitUntil = millis()+1000;
  
  do
  {
    if (dht.getStatus() == dht.ERROR_NONE)
    {
      dhtExists = true;
    }
    else
    {
      mylog("#");
      dht.setup(DHT_PIN);
      toggleLED();
      delay(20);
    }
  } 
  while (!dhtExists && (millis() < waitUntil));
      
  
  mylog("dht:");
  mylog(dht.getStatusString());
  mylog("\n");
  
}

void bmeStart()
{
  unsigned long waitUntil = millis()+1000;
  bool success=false;
  do 
  {
    success = bme.begin();
    if (success)
    {
      break;
    }
    delay(200);
  } while (millis()<waitUntil);

  if (success)
  {
    switch(bme.chipModel())
    {
        case BME280::ChipModel_BME280:
          mylog("\nFound BME280 sensor!\n");
          bmeExists= true;
          break;
        case BME280::ChipModel_BMP280:
          mylog("\nFound BM_P_280 sensor!\n"); // we do not support this type
          break;
        default:
          mylog("\nFound UNKNOWN sensor! Error!\n");
    }
  }
  else
  {
    mylog("\nno BME found\n");
  }
}

void pHStart()
{
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_11db); 
  
  if (adc1_get_raw(ADC1_CHANNEL_0)!=0)
  {
    pHExists = true;
    Serial.println("Found pH");
  }
  pHMeter.initialize();

}

void ePhotostart() // work in progress, no idea how the driver works
{

  if (tsl.begin() )
{
Serial.println("Found Photometer");
}
else {
Serial.println("No Photometer");

}
/*
  if (adc1_get_raw(ADC1_CHANNEL_0)!=0)
  {
    TSL2561Exists = true;
    Serial.println("Found Photometer");
  }
  photoMeter.initialize();
*/
}

unsigned long startMillis;
void setup() 
{
 
  startMillis = millis();

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(TRIGGER_PIN1, INPUT_PULLUP);           // set pin to input
  pinMode(TRIGGER_PIN2, INPUT_PULLUP);           // set pin to input
      

  bool result = SPIFFS.begin();
  mylog(String(" SPIFFS opened: ") + String(result) + "\n");

  mylog(WiFi.macAddress().c_str());
  mylog("\n");


  Wire.begin(SCL_PIN, SDA_PIN);
  mylog("\n-- scanI2c --\n");
  int nDevices = scanI2c();

  if (!nDevices)
  {
    mylog("no devices found, trying again with swapped pins...\n");
    Wire.begin(SDA_PIN,SCL_PIN);
    mylog("\n-- scanI2c --\n");
    scanI2c();
  }
  

  RTCStart();
  bmeStart();
  if (!bmeExists)
  {
    dhtStart();
  }
  pHStart();
  ePhotostart();

  if (bmeExists || dhtExists)
  {
    myType = eRoomLabel;
    if (bmeExists)
    {
      roomLabel.setVersion(2);
    }
    else
    {
      roomLabel.setVersion(1);
    }
  }
  else if (pHExists)
  {
    myType = epH;
  }
    else if (TSL2561Exists)
  {
    myType = ePhotoMeter;
  }

  readSensor();

  oledStart();
  lcdStart();

    // Mount the filesystem

  if (!readConfigFile()) 
  {
    mylog("Failed to read configuration file, using default values\n");
  }

  //readXmlFromFS();
  
  displayLcdScreen();
  displayOledScreen();
  wifiStart();

  mylog("\nConnected!\n");
  digitalWrite(LED_BUILTIN, LOW);
  serverStart();
  mylog("Server Started.\n");

  displayOledScreen(NULL, NULL, "waiting for PAS-X...");
  displayLcdScreen(NULL, NULL, "waiting for PAS-X...");

  lastPasXInteraction = millis(); // Watch dog timer starts now
}


unsigned long nextSensorRead    = SENSORREADINTERVAL;
unsigned long nextDisplayUpdate = DISPLAYUPDATEINTERVAL;
void loop() 
{
  heap_caps_check_integrity_all(true);

  // waiting for PAS-X to start polling
  if (!setupComplete)
  {
    if (lastGetNextMessage || lastPostMessage) // did PAS-X send a message?
    {
      mylog("Setup complete!\n");
      setupComplete = true;
      if (myType == eRoomLabel)
      {
        roomLabel.readLcdFile();
        displayLcdScreen();
      }
      else if (myType == epH)
      {
        pHMeter.readConfig();
      }
      else // don't know who I am... :(
      {
        displayLcdScreen((String)"", (String)"", (String)"", "No sensor found!    ", true);
      }
    }
  }
  else // setup IS complete, PAS-X responded, check sensors in intervalls.
  {
    if (millis() > nextSensorRead) 
    {

      nextSensorRead = millis() + SENSORREADINTERVAL;

      if (myType == eRoomLabel)
      { 
        if (readSensor()) // also reads pH
        {
          displayLcdScreen(); // #todo: necessary?
        }
        else if (dhtExists) // DHT sensor but no reading? Try again faster.
        {
          nextSensorRead = millis() + SENSORREADINTERVAL/10;
        }


        mylog(humidity);
        mylog("RH, ");
        mylog(temperature);
        mylog("C\n");
        if ((!(roomLabel.rhUpperLimit==0 && roomLabel.rhLowerLimit==0) //ignore if both limits are 0.0
        &&  (humidity > roomLabel.rhUpperLimit || humidity<roomLabel.rhLowerLimit))
          ||
          (!(roomLabel.tempUpperLimit==0 && roomLabel.tempLowerLimit==0) //ignore if both limits are 0.0
        &&  (temperature > roomLabel.tempUpperLimit || temperature<roomLabel.tempLowerLimit)))
        {

          String batchID = roomLabel.lastBatchId;
          String PU      = roomLabel.lastPU;
          mylog("Exception level reached\n");
         
          if (roomLabel.systemId.length() && (batchID != lastExceptionBatchID || (millis() - lastExceptionSent>exceptionSendInterval)))
          {
            lastExceptionSent = millis();
            lastExceptionBatchID = batchID;
            String timeStampString = getUTC();

            String userDescription;
            if (!(roomLabel.rhUpperLimit==0 && roomLabel.rhLowerLimit==0))
            {
              if (humidity > roomLabel.rhUpperLimit || humidity<roomLabel.rhLowerLimit)
              { 
                userDescription += "Humidity is outside the limit (" + String(roomLabel.rhLowerLimit) + "-" 
                                  + String(roomLabel.rhUpperLimit) + "RH%). Current humidity is " + String(humidity) + "RH%\n";
              }
            }
            if (!(roomLabel.tempUpperLimit==0 && roomLabel.tempLowerLimit==0))
            {
              if (temperature > roomLabel.tempUpperLimit || temperature<roomLabel.tempLowerLimit)
              {
                userDescription += "Temperature is outside the limit (" + String(roomLabel.tempLowerLimit) + "-" 
                                + String(roomLabel.tempUpperLimit) + "C). Current temperature is " + String(temperature) + "C";
              }
            }
            String systemDesciption;
            String exceptionMessage = ExceptionMessage::getExceptionMessageText(roomLabel.systemId.c_str(), 
                                                                                "",//batchID.c_str(), 
                                                                                PU.c_str(),
                                                                                "Sensor data outside of limits", 
                                                                                userDescription.c_str(), 
                                                                                timeStampString.c_str());
            mylog(exceptionMessage);
            if (displayLcd)
            {
              displayLcd->setBacklight(!roomLabel.getEnabled());
              delay(100);
              displayLcd->setBacklight(roomLabel.getEnabled());
            }

            sendQueue.push(exceptionMessage);
          }
        }
      }
      else if (myType == epH)
      {
        float pH = pHMeter.getpH();
        mylog(pH);
        mylog("pH\n");
        if ( pHMeter.upperLimit > pHMeter.lowerLimit 
        &&  (pH > pHMeter.upperLimit || pH<pHMeter.lowerLimit)
        &&  (millis() - lastExceptionSent>exceptionSendInterval))
        {
            String batchID = pHMeter.lastBatchId;
            lastExceptionSent = millis();
            String timeStampString = getUTC();

            String userDescription;
            userDescription += "pH value is outside the limit (" + String(pHMeter.lowerLimit) + "-" 
                              + String(pHMeter.upperLimit) + "pH). Current pH is " + String(pH) + "pH\n";
            String systemDesciption;
            String exceptionMessage = ExceptionMessage::getExceptionMessageText(pHMeter.systemId.c_str(), 
                                                                                batchID.c_str(), 
                                                                                "",//PU.c_str(),
                                                                                "System Description", 
                                                                                userDescription.c_str(), 
                                                                                timeStampString.c_str());
            mylog(exceptionMessage);

            sendQueue.push(exceptionMessage);
        }

      }
    }

    if (millis() > nextDisplayUpdate)
    {
      nextDisplayUpdate = millis() + DISPLAYUPDATEINTERVAL;
      displayOledScreen();
    }

  }
  
  if (!digitalRead(TRIGGER_PIN1))
  {
    Serial.print("1");
  }
  if (!digitalRead(TRIGGER_PIN2))
  {
    Serial.print("2");
  }
  
  if (millis() - lastPasXInteraction > pasXWatchDogTime)
  {
    mylog("\n############################################################################\n");
    mylog(getNow());
    mylog("################################ watchdog reset ############################\n");
    mylog("############################################################################\n", true);
    delay(100);
    ESP.restart();
  }


  // Check if WLAN is connected
  if (WiFi.status() != WL_CONNECTED)
  {   
    mylog("reconnecting...\n");
    WiFi.reconnect();
    if (WiFi.status() == WL_CONNECTED)
    {
      mylog("reconnected!\n");
    }
    else
    {
      delay(250);
    }
  }
  server.handleClient();
}

