/*

ESP32 mit 4.2 inch Waveshare SPI e-paper
für Gang Stromanzeige

// mapping suggestion for ESP32, e.g. TTGO T8 ESP32-WROVER
// BUSY -> 4, RST -> 0, DC -> 2, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
// for use with Board: "ESP32 Dev Module":
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

part of the code (icon handling)
https://github.com/G6EJD/ESP32-e-Paper-Weather-Display/blob/master/examples/Waveshare_4_2/Waveshare_4_2.ino

*/

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include "WiFi.h"
#include <time.h>  
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>



#include "main.h"

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS*/ 5, /*DC*/ 17, /*RST*/ 16, /*BUSY*/ 4));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;



boolean LargeIcon = true, SmallIcon = false;
#define Large  11           // For icon drawing, needs to be odd number for best effect
#define Small  5            // For icon drawing, needs to be odd number for best effect

int8_t RedrawCounter=0;

short temp=0;
short garten=0;
short bat=0;
short prod=0;
short wasser=0;
float Buddy=0, Mika=0, Matti=0, Timmi=0;

float Wetter_wind=0;
String Wetter_main="";
String Wetter_description="";
String Wetter_icon="";
float Wetter_rain=0;
String Wetter_rainForecast="";
String Wetter_tempForecast="";
String Wetter_hour1="";
String Wetter_hour2="";
String Wetter_hour3="";
String Wetter_hour4="";


const char* wifihostname = "ESP_Epaper_Neu";

#define UDPDEBUG 1
#ifdef UDPDEBUG
WiFiUDP udp;
const char * udpAddress = "192.168.0.95";
const int udpPort = 19814;
#endif

#define NTP_SERVER "de.pool.ntp.org"
#define DefaultTimeZone "CET-1CEST,M3.5.0/02,M10.5.0/03"  
String MY_TZ = DefaultTimeZone ;
struct tm timeinfo;
char time_last_restart_day = -1;
char time_last_minute = -1;

uint32_t lastUpdateMs = millis();

WiFiClient wifiClient;
const char* mqtt_server = "192.168.0.46";
PubSubClient mqttclient(wifiClient);


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
    alertMessage(300, 380, "Connect Wifi", RIGHT);

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
    UDBDebug("WiFi connected");
    Serial.println(ip);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);
  Serial.println("setup");

  display.init();//115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
 u8g2Fonts.begin(display); 

  //display.setRotation(0);
  helloWorld("Starte Network");
    WifiConnect();

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

    Serial.println("vor MQTT");
    mqttclient.setServer(mqtt_server, 1883);  
    mqttclient.setCallback(MQTT_callback); 
    mqttclient.setBufferSize(8192); 

    short counter = 0;
    while (!getLocalTime(&timeinfo, 10000)) {
      UDBDebug("error getLocalTime"); 
      if (counter++ > 20) 
        ESP.restart();
      delay(2000); }

    Serial.println("setup done"); 
    helloWorld("setup done");
    delay(2000);

   if (mqttclient.connect(wifihostname, MQTT_User, MQTT_Pass)) {
      UDBDebug(F("MQTT connect successful"));  
      const char *TOPIC = ("hm/status/Temp_Aussen2/TEMPERATURE");
      mqttclient.subscribe(TOPIC);  
      const char *TOPIC2 = ("HomeServer/Batterie/USOC");
      mqttclient.subscribe(TOPIC2);   
      const char *TOPIC3 = ("HomeServer/Tiere/#");
      mqttclient.subscribe(TOPIC3);
      const char *TOPIC4 = ("HomeServer/Heizung/WasserDay");
      mqttclient.subscribe(TOPIC4);      
      const char *TOPIC5 = ("HomeServer/Strom/Produktion");
      mqttclient.subscribe(TOPIC5);   
      const char *TOPIC6 = ("HomeServer/Wetter/#");
      mqttclient.subscribe(TOPIC6);  
      const char *TOPIC7 = ("hm/status/Gartensensor:2/HUMIDITY");
      mqttclient.subscribe(TOPIC7);  
      }  
    else
       UDBDebug("MQTT connect error");  

   Serial.printf("nach MQTT2");   
   


}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
      WifiConnect();
    
    ArduinoOTA.handle();

    if (!mqttclient.loop()) {
      UDBDebug("MQTT was disconnected"); 
      mqttclient.disconnect();
      delay(50);
      alertMessage(0, 380, "Connect MQTT", CENTER);
      if (mqttclient.connect(wifihostname, MQTT_User, MQTT_Pass)) { //, "HomeServer/Server/displayWill", 2, true, "dead", true)) {
        UDBDebug("MQTT reconnect successful"); 
      }  
      else
        UDBDebug("MQTT reconnect error");  
    };

    if(!getLocalTime(&timeinfo, 2000)){
      Serial.println("Failed to obtain time");
      UDBDebug("Failed to obtain time");  
     }
   
    if ((time_last_restart_day == 6) && (timeinfo.tm_wday == 0))
      {
        // time for restart !! one restart every week
        UDBDebug("weekly restart");
        ESP.restart();
      }
    else time_last_restart_day = timeinfo.tm_wday;

    uint32_t curmillis = millis();
    if (lastUpdateMs + 300000 < curmillis) {
        ESP.restart();
    }
    else 
      lastUpdateMs = curmillis;
 
    if (time_last_minute != timeinfo.tm_min) {
      time_last_minute = timeinfo.tm_min;
      char zeit[20];
      snprintf(zeit, 15, "%d:%d", timeinfo.tm_hour,timeinfo.tm_min);
      alertMessage(240, 383, zeit, LEFT);
      mqttclient.publish ("HomeServer/Server/Display", (const uint8_t*) zeit, 5, false);
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


void UpdateWetterHour(String data, short x, short y) {

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    
    int temp = doc["temp"]; // 8
    float rain = doc["rain"]; // 17.1
     DisplayWXicon( x+75,  y+10, doc["icon"], SmallIcon);
     const char* hour = doc["hour"]; // "08:00"

    u8g2Fonts.setFont(u8g2_font_helvR10_tf);
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(hour); 
    u8g2Fonts.setFont(u8g2_font_logisoso22_tf);
    u8g2Fonts.setCursor(x+10, y+30);
    u8g2Fonts.print(String(temp)+"°"); 
    if (rain > 0) {
      u8g2Fonts.setCursor(x+45, y+40);
      u8g2Fonts.setFont(u8g2_font_helvR10_tf);
      u8g2Fonts.print(rain);      
    }
}

void UpdateWetterRain(short x, short y) {
  StaticJsonDocument<1524> doc;
//Serial.println("wetterrain");
  DeserializationError error = deserializeJson(doc, Wetter_rainForecast);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
//Serial.println("wetterrain2: "+String(doc.size()));
  for (short i=0; i< doc.size();i++) {
      float root = doc[i];   // 0-9
      if (root > 0) {
        root *= 5;
        short y1 = (int) y - root;
        display.drawLine(x+i, y, x+i, y1, GxEPD_BLACK);
      }
  }
//Serial.println("wetterrain3");  
}

void UpdateWetterTemp(short x, short y) {
  StaticJsonDocument<1524> doc;
Serial.println("wettertemp");
  DeserializationError error = deserializeJson(doc, Wetter_tempForecast);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
Serial.println("wettertemp2: "+String(doc.size()));
  for (short i=0; i< doc.size();i++) {
      float root = doc[i];   // 0-9
      if (root > 0) {
        short y1 = (int) root + y;
        display.drawPixel(x+i, y1, GxEPD_BLACK);
      }
  }
Serial.println("wettertemp3");  
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
    display.setPartialWindow(0, 0, display.width(), 378); //display.height());

  display.firstPage();
  do
  {
    display.fillScreen(bg);
    DisplayWXicon( 70,  50, Wetter_icon, LargeIcon);

    u8g2Fonts.setFont(u8g2_font_fub42_tf); //u8g2_font_inb38_mf);
    u8g2Fonts.setCursor(180, 60);
    u8g2Fonts.print(String(temp)+"°");
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(200, 305);
    u8g2Fonts.print(String(bat)+"%");
    u8g2Fonts.setFont(u8g2_font_helvR14_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(180, 90);
    u8g2Fonts.print("Wind: "+String(Wetter_wind));

    //u8g2Fonts.setFont(u8g2_font_helvR14_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(200, 325);
    u8g2Fonts.print(String(prod)+" Wh");
    u8g2Fonts.setCursor(200, 370);
    u8g2Fonts.print(String(wasser)+" Liter");

    if (garten > 0) {
      u8g2Fonts.setCursor(200, 350);
      u8g2Fonts.print("Boden: "+String(garten));
    }

    u8g2Fonts.setCursor(20, 300);
    u8g2Fonts.print("Buddy: ");
    u8g2Fonts.setCursor(90, 300);
    u8g2Fonts.print(String(Buddy,1)+" kg");  
    u8g2Fonts.setCursor(20, 321);
    u8g2Fonts.print("Mika: ");
    u8g2Fonts.setCursor(90, 321);
    u8g2Fonts.print(String(Mika,1)+" kg");  
    u8g2Fonts.setCursor(20, 343);
    u8g2Fonts.print("Matti: ");
    u8g2Fonts.setCursor(90, 343);    
    u8g2Fonts.print(String(Matti,1)+" kg");  
    u8g2Fonts.setCursor(20, 365);
    u8g2Fonts.print("Timmi: ");    
    u8g2Fonts.setCursor(90, 365);
    u8g2Fonts.print(String(Timmi,1)+" kg");  

    u8g2Fonts.setFont(u8g2_font_helvR12_te); //(u8g2_font_luBS12_tf); // u8g2_font_helvR12_tf);  
    // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(5, 95);
    u8g2Fonts.print(Wetter_description);  

    UpdateWetterHour(Wetter_hour1, 10, 120);
    UpdateWetterHour(Wetter_hour2, 10, 180);
    UpdateWetterHour(Wetter_hour3, 10, 240);    
    UpdateWetterHour(Wetter_hour4, 160, 120);

    UpdateWetterRain(200, 180);
    UpdateWetterTemp(200, 230);

    time_last_minute = timeinfo.tm_min;
      char zeit[20];
      snprintf(zeit, 15, "%d:%d", timeinfo.tm_hour,timeinfo.tm_min);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(0, 380, zeit, LEFT);
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

float round1(float value) {
   return (int)(value * 10 + 0.5) / 10.0;
}

  // Callback function
void MQTT_callback(char* topic, byte* payload, unsigned int length) {

    lastUpdateMs = millis();

    String message = String(topic);
    //int8_t joblength = message.length()+1;// 0 char
    payload[length] = '\0';
    String value = String((char *) payload);
    Serial.println("### "+message +" - "+value);

    if (message == F("hm/status/Temp_Aussen2/TEMPERATURE")) {
      temp = value.toInt();
      return;
    }

    if (message == F("hm/status/Gartensensor:2/HUMIDITY")) {
      garten = value.toInt();
      return;
    }

    if (message == F("HomeServer/Batterie/USOC")) {
      bat = value.toInt();
      UpdateDisplay();
      return;
    }
    
    if (message == F("HomeServer/Strom/Produktion")) {
      prod = value.toInt();
      return;
    }

    if (message == F("HomeServer/Heizung/WasserDay")) {
      wasser = value.toInt();
      return;
    }

    if (message == F("HomeServer/Tiere/Tag_Buddy")) {
      Buddy = round1(value.toFloat());
      return;
    }        
    if (message == F("HomeServer/Tiere/Tag_Mika")) {
      Mika = round1(value.toFloat());
      return;
    }  
    if (message == F("HomeServer/Tiere/Tag_Matti")) {
      Matti = round1(value.toFloat());
      return;
    }  
    if (message == F("HomeServer/Tiere/Timmi")) {
      Timmi = round1(value.toFloat());
      return;
    }  

    if (message.length() > 17) {
        //UDBDebug(message.substring(0, 17));
        if (message.substring(0, 17) == F("HomeServer/Wetter")) {
          if (message == F("HomeServer/Wetter/wind")) {
            Wetter_wind = value.toFloat();
            return;
          }
          if (message == F("HomeServer/Wetter/main")) {
            Wetter_main = value;
            UDBDebug("ePaper: HomeServer/Wetter/main");
            UpdateDisplay();
            return;
          }
          if (message == F("HomeServer/Wetter/description")) {
            Wetter_description = value;
            return;
          }            
          if (message == F("HomeServer/Wetter/icon")) {
            Wetter_icon = value;
            return;
          }   
          if (message == F("HomeServer/Wetter/wind")) {
            Wetter_wind = value.toFloat();
            return;
          }       
          if (message == F("HomeServer/Wetter/rain")) {
            Wetter_rain = value.toFloat();
            return;
          } 
          if (message == F("HomeServer/Wetter/rainForecast")) {
            Wetter_rainForecast = value;
            return;
          }  
          if (message == F("HomeServer/Wetter/tempForecast")) {
            Wetter_tempForecast = value;
            return;
          } 
          if (message == F("HomeServer/Wetter/hour1")) {
            Wetter_hour1 = value;
            return;
          }  
          if (message == F("HomeServer/Wetter/hour2")) {
            Wetter_hour2 = value;
            return;
          }  
          if (message == F("HomeServer/Wetter/hour3")) {
            Wetter_hour3 = value;
            return;
          }  
          if (message == F("HomeServer/Wetter/hour4")) {
            Wetter_hour4 = value;
            return;
          }  
        }   
    }

    
}

void DisplayWXicon(int x, int y, String IconName, bool IconSize) {
  //Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
}

//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                      // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                      // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);            // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE); // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 8;
    offset = 18;
  } else y = y - 3; // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  } else linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 35, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y + 10, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y + 15, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 5, scale * 1.4, IconSize);
  addfog(x, y - 5, scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.5, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.5, 2); // Cloud top right
  addcloud(x, y,         Small * 0.5, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
  }
  display.fillCircle(x, y, r / 4, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 12, y - 3, Visi, LEFT);
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x + 12; y = y + 12;
    display.fillCircle(x - 50, y - 55, scale, GxEPD_BLACK);
    display.fillCircle(x - 35, y - 55, scale * 1.6, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 20, y - 12, scale, GxEPD_BLACK);
    display.fillCircle(x - 15, y - 12, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}


void alertMessage(int x, int y, String text, alignment align) {
  
  display.setPartialWindow(0, 378, display.width(), display.height());

  display.firstPage();
  do
  {
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y, text, align);
  }  
  while (display.nextPage());
  display.powerOff();
}