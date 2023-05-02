/*

Wemos ESP 8266 mit 4.2 inch Waveshare SPI e-paper
für Gang Stromanzeige

 * 4.2inch Display wemos mini
 * // BUSY -> D2, RST -> D4, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V   +++
BUSY  D2 gpIO4
RST   D4 GPIO2
senDC    D3 GPIO0
CS    D1  --  D8 gpio15 -  pulldown 4.7k auf Masse !!!  -- bei Gang2 auf D1
CLK   D5-GPIO14
DIN   D7-GPIO13
GND   GND
3.3V  3V3

Dallas Temp auf D1  gpIO5
Bewegungssensor auf A0

*/

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <time.h>  
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
//#include <ArduinoJson.h>

#include "main.h"

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ D8, /*DC=D3*/ D3, /*RST=D4*/ D4, /*BUSY=D2*/ D2));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

int8_t RedrawCounter=0;

short temp=0;
short bat=0;
short prod=0;
short wasser=0;
float Buddy=0, Mika=0, Matti=0, Timmi=0;


const char* wifihostname = "ESP_Epaper_Neu";

#define UDPDEBUG 1
#ifdef UDPDEBUG
WiFiUDP udp;
const char * udpAddress = "192.168.0.34";
const int udpPort = 19814;
#endif

#define NTP_SERVER "de.pool.ntp.org"
#define DefaultTimeZone "CET-1CEST,M3.5.0/02,M10.5.0/03"  
String MY_TZ = DefaultTimeZone ;
struct tm timeinfo;
char time_last_restart_day = -1;

WiFiClient wifiClient;
const char* mqtt_server = "192.168.0.46";
PubSubClient mqttclient(wifiClient);

//  missing in esp8266, from esp32
bool getLocalTime(struct tm * info, uint32_t ms)
{
    uint32_t start = millis();
    time_t now;
    while((millis()-start) <= ms) {
        time(&now);
        localtime_r(&now, info);
        if(info->tm_year > (2016 - 1900)){
            return true;
        }
        delay(10);
    }
    return false;
}

void setTimeZone(String TimeZone) {
  struct tm local;
  configTzTime(TimeZone.c_str(), NTP_SERVER); // ESP32 Systemzeit mit NTP Synchronisieren
  getLocalTime(&local, 10000);      // Versuche 10 s zu Synchronisieren
  #ifdef webdebug  
    Serial.println("TimeZone: "+TimeZone);
    Serial.println(&local, "%A, %B %d %Y %H:%M:%S");
  #endif  
}

void WifiConnect() {
    WiFi.setHostname(wifihostname);  
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    short counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (++counter > 50)
        ESP.restart();
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    IPAddress ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.println(ip);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);
  Serial.println("setup");

  display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
 u8g2Fonts.begin(display); 

  //display.setRotation(0);
  helloWorld("Starte Network");
    WifiConnect();

    Serial.println(WiFi.localIP());
    helloWorld("WLAN connected");
    ArduinoOTA.setHostname(wifihostname);  
    
    ArduinoOTA.onStart([]() {
    if (ArduinoOTA.getCommand() == U_FLASH) {
      helloWorld("Uploading Firmware");
    } 
    else { // U_SPIFFS
     }  
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.println("Progress: " + String( (progress / (total / 100))));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.println(error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });


    ArduinoOTA.begin();
    setTimeZone(MY_TZ);


    Serial.printf("vor MQTT");
    mqttclient.setServer(mqtt_server, 1883);
    mqttclient.setCallback(MQTT_callback);
    mqttclient.setBufferSize(1024);
   if (mqttclient.connect(wifihostname, MQTT_User, MQTT_Pass)) {
      UDBDebug("MQTT connect successful"); 
      const char *TOPIC = "hm/status/Temp_Aussen2/TEMPERATURE";
      mqttclient.subscribe(TOPIC);
      const char *TOPIC2 = "HomeServer/Batterie/USOC";
      mqttclient.subscribe(TOPIC2);
      const char *TOPIC3 = "HomeServer/Tiere/#";
      mqttclient.subscribe(TOPIC3);
      const char *TOPIC4 = "HomeServer/Heizung/WasserDay";
      mqttclient.subscribe(TOPIC4);      
      const char *TOPIC5 = "HomeServer/Strom/Produktion";
      mqttclient.subscribe(TOPIC5);  
     }  
    else
       UDBDebug("MQTT connect error");  

   Serial.printf("nach MQTT2");   
   

  while (!getLocalTime(&timeinfo, 2000)) {
    UDBDebug("error getLocalTime"); 
    delay(2000); }

    Serial.println("setup done"); 
    helloWorld("setup done");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
      WifiConnect();
    
    ArduinoOTA.handle();

    if (!mqttclient.loop()) {
      if (mqttclient.connect(wifihostname, MQTT_User, MQTT_Pass)) {
        UDBDebug("MQTT reconnect successful"); 
      }  
      else
        UDBDebug("MQTT reconnect error");  
    };

    if(!getLocalTime(&timeinfo, 2000)){
      Serial.println("Failed to obtain time");
      UDBDebug("Failed to obtain time");  
     }
   else
    {
      if ((time_last_restart_day == 6) && (timeinfo.tm_wday == 0))
      {
        // time for restart !! one restart every week
        UDBDebug("weekly restart");
        ESP.restart();
      }
      else time_last_restart_day = timeinfo.tm_wday;
    }

    delay(1);
}


void helloWorld(const char *HelloWorld)
{
  //Serial.println("helloWorld");
  display.setRotation(3);
  uint16_t bg = GxEPD_WHITE;
  uint16_t fg = GxEPD_BLACK;
  u8g2Fonts.setFontMode(1);                 // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);            // left to right (this is default)
  u8g2Fonts.setForegroundColor(fg);         // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(bg);         // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvR14_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  int16_t tw = u8g2Fonts.getUTF8Width(HelloWorld); // text box width
  int16_t ta = u8g2Fonts.getFontAscent(); // positive
  int16_t td = u8g2Fonts.getFontDescent(); // negative; in mathematicians view
  int16_t th = ta - td; // text box height
  //Serial.print("ascent, descent ("); Serial.print(u8g2Fonts.getFontAscent()); Serial.print(", "); Serial.print(u8g2Fonts.getFontDescent()); Serial.println(")");
  // center bounding box by transposition of origin:
  // y is base line for u8g2Fonts, like for Adafruit_GFX True Type fonts
  uint16_t x = (display.width() - tw) / 2;
  uint16_t y = (display.height() - th) / 2 + ta;
  //Serial.print("bounding box    ("); Serial.print(x); Serial.print(", "); Serial.print(y); Serial.print(", "); Serial.print(tw); Serial.print(", "); Serial.print(th); Serial.println(")");
  display.firstPage();
  do
  {
    display.fillScreen(bg);
    u8g2Fonts.setCursor(x, y); // start writing at this position
    u8g2Fonts.print(HelloWorld);
  }
  while (display.nextPage());
  //Serial.println("helloWorld done");
}



void helloWorld2(const char *HelloWorld)
{
  //Serial.println("helloWorld");
  display.setRotation(0);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = (display.width() - tbw) / 2;
  uint16_t y = (display.height() + tbh) / 2; // y is base line!
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(x, y);
  display.print(HelloWorld);
  
  while (display.nextPage());
  //Serial.println("helloWorld done");

  display.powerOff();
}

void UpdateDisplay()
{
  display.setRotation(3);
  uint16_t bg = GxEPD_WHITE;
  uint16_t fg = GxEPD_BLACK;
  u8g2Fonts.setFontMode(1);                 // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);            // left to right (this is default)
  u8g2Fonts.setForegroundColor(fg);         // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(bg);         // apply Adafruit GFX color

  if (RedrawCounter++ > 10) {  // war 15;
      display.setFullWindow();
      RedrawCounter = 0;
  }
  else
    display.setPartialWindow(0, 0, display.width(), display.height());

  display.firstPage();
  do
  {
    display.fillScreen(bg);
    u8g2Fonts.setFont(u8g2_font_inb38_mf);
    u8g2Fonts.setCursor(180, 60);
    u8g2Fonts.print(String(temp)+"°");
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(200, 295);
    u8g2Fonts.print(String(bat)+"%");

    u8g2Fonts.setFont(u8g2_font_helvR14_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(200, 350);
    u8g2Fonts.print(String(prod)+" Wh");
    u8g2Fonts.setCursor(200, 370);
    u8g2Fonts.print(String(wasser)+" Liter");

    u8g2Fonts.setCursor(20, 295);
    u8g2Fonts.print("Buddy: ");
    u8g2Fonts.setCursor(80, 295);
    u8g2Fonts.print(String(Buddy)+" kg");  
    u8g2Fonts.setCursor(20, 320);
    u8g2Fonts.print("Mika: ");
    u8g2Fonts.setCursor(80, 320);
    u8g2Fonts.print(String(Mika)+" kg");  
    u8g2Fonts.setCursor(20, 345);
    u8g2Fonts.print("Matti: ");
    u8g2Fonts.setCursor(80, 345);    
    u8g2Fonts.print(String(Matti)+" kg");  
    u8g2Fonts.setCursor(20, 370);
    u8g2Fonts.print("Timmi: ");    
    u8g2Fonts.setCursor(80, 370);
    u8g2Fonts.print(String(Timmi)+" kg");  

  }
  while (display.nextPage());
  display.powerOff();
}


void UDBDebug(String message) {
#ifdef UDPDEBUG
  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t* ) message.c_str(), (size_t) message.length());
  udp.endPacket();
#endif  
}

void UDBDebug(const char * message) {
#ifdef UDPDEBUG
  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t*) message, strlen(message));
  udp.endPacket();
#endif  
}

  // Callback function
void MQTT_callback(char* topic, byte* payload, unsigned int length) {

    String message = String(topic);
    //int8_t joblength = message.length()+1;// 0 char
    payload[length] = '\0';
    String value = String((char *) payload);
    Serial.println("### "+message +" - "+value);

    if (message == "hm/status/Temp_Aussen2/TEMPERATURE") {
      temp = value.toInt();
    }

    if (message == "HomeServer/Batterie/USOC") {
      bat = value.toInt();
      UpdateDisplay();
    }
    
    if (message == "HomeServer/Strom/Produktion") {
      prod = value.toInt();
    }

    if (message == "HomeServer/Heizung/WasserDay") {
      wasser = value.toInt();
    }

    if (message == "HomeServer/Tiere/Tag_Buddy") {
      Buddy = value.toFloat();
    }        
    if (message == "HomeServer/Tiere/Tag_Mika") {
      Mika = value.toFloat();
    }  
    if (message == "HomeServer/Tiere/Tag_Matti") {
      Matti = value.toFloat();
    }  
    if (message == "HomeServer/Tiere/Timmi") {
      Timmi = value.toFloat();
    }  

}