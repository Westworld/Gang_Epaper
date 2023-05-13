/*

ESP32 mit 4.2 inch Waveshare SPI e-paper
für Gang Stromanzeige

// mapping suggestion for ESP32, e.g. TTGO T8 ESP32-WROVER
// BUSY -> 4, RST -> 0, DC -> 2, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
// for use with Board: "ESP32 Dev Module":

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

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ 5, /*DC=D3*/ 2, /*RST=D4*/ 0, /*BUSY=D2*/ 4));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

int8_t RedrawCounter=0;

short temp=0;
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
String Wetter_hour1="";
String Wetter_hour2="";
String Wetter_hour3="";
String Wetter_hour4="";


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

  display.init();//115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
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

    Serial.println("vor MQTT");
    mqttclient.setServer(mqtt_server, 1883);  
    mqttclient.setCallback(MQTT_callback); 
    mqttclient.setBufferSize(8192); 
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


void UpdateWetterHour(String data, short x, short y) {

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    else
    Serial.println("hour");
    
    
    int temp = doc["temp"]; // 8
    //float wind = doc["wind"]; // 17.1
    //const char* main = doc["main"]; // "Clouds"
    //const char* description = doc["description"]; // "Mäßig bewölkt"
    const char* icon = doc["icon"]; // "03d"
    //const char* day = doc["day"]; // "Thu"
    const char* hour = doc["hour"]; // "08:00"

    u8g2Fonts.setFont(u8g2_font_helvR10_tf);
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(hour); 
    u8g2Fonts.setFont(u8g2_font_logisoso22_tf);
    u8g2Fonts.setCursor(x+10, y+27);
    u8g2Fonts.print(String(temp)+"°"); 

 

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
    u8g2Fonts.setFont(u8g2_font_fub42_tf); //u8g2_font_inb38_mf);
    u8g2Fonts.setCursor(180, 60);
    u8g2Fonts.print(String(temp)+"°");
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    u8g2Fonts.setCursor(200, 295);
    u8g2Fonts.print(String(bat)+"%");

    u8g2Fonts.setFont(u8g2_font_helvR14_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(200, 320);
    u8g2Fonts.print(String(prod)+" Wh");
    u8g2Fonts.setCursor(200, 370);
    u8g2Fonts.print(String(wasser)+" Liter");

    u8g2Fonts.setCursor(20, 310);
    u8g2Fonts.print("Buddy: ");
    u8g2Fonts.setCursor(90, 310);
    u8g2Fonts.print(String(Buddy,1)+" kg");  
    u8g2Fonts.setCursor(20, 331);
    u8g2Fonts.print("Mika: ");
    u8g2Fonts.setCursor(90, 331);
    u8g2Fonts.print(String(Mika,1)+" kg");  
    u8g2Fonts.setCursor(20, 353);
    u8g2Fonts.print("Matti: ");
    u8g2Fonts.setCursor(90, 353);    
    u8g2Fonts.print(String(Matti,1)+" kg");  
    u8g2Fonts.setCursor(20, 375);
    u8g2Fonts.print("Timmi: ");    
    u8g2Fonts.setCursor(90, 375);
    u8g2Fonts.print(String(Timmi,1)+" kg");  

    u8g2Fonts.setFont(u8g2_font_luBS12_tf); // u8g2_font_helvR12_tf);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setCursor(20, 75);
    u8g2Fonts.print(Wetter_description);  

    UpdateWetterHour(Wetter_hour1, 10, 120);
    UpdateWetterHour(Wetter_hour2, 10, 180);
    UpdateWetterHour(Wetter_hour3, 10, 240);    
    UpdateWetterHour(Wetter_hour4, 160, 120);



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

    String message = String(topic);
    //int8_t joblength = message.length()+1;// 0 char
    payload[length] = '\0';
    String value = String((char *) payload);
    Serial.println("### "+message +" - "+value);

    if (message == F("hm/status/Temp_Aussen2/TEMPERATURE")) {
      temp = value.toInt();
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
        UDBDebug(message.substring(0, 17));
        if (message.substring(0, 17) == F("HomeServer/Wetter")) {
          if (message == F("HomeServer/Wetter/wind")) {
            Wetter_wind = value.toFloat();
            return;
          }
          if (message == F("HomeServer/Wetter/main")) {
            Wetter_main = value;
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
          if (message == F("HomeServer/Wetter/rain")) {
            Wetter_rain = value.toFloat();
            return;
          }       
          if (message == F("HomeServer/Wetter/rainForecast")) {
            Wetter_rainForecast = value;
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


String getMeteoconIcon(String icon) {
 	// clear sky
  // 01d
  if (icon == "01d") 	{
    return "B";
  }
  // 01n
  if (icon == "01n") 	{
    return "C";
  }
  // few clouds
  // 02d
  if (icon == "02d") 	{
    return "H";
  }
  // 02n
  if (icon == "02n") 	{
    return "4";
  }
  // scattered clouds
  // 03d
  if (icon == "03d") 	{
    return "N";
  }
  // 03n
  if (icon == "03n") 	{
    return "5";
  }
  // broken clouds
  // 04d
  if (icon == "04d") 	{
    return "Y";
  }
  // 04n
  if (icon == "04n") 	{
    return "%";
  }
  // shower rain
  // 09d
  if (icon == "09d") 	{
    return "R";
  }
  // 09n
  if (icon == "09n") 	{
    return "8";
  }
  // rain
  // 10d
  if (icon == "10d") 	{
    return "Q";
  }
  // 10n
  if (icon == "10n") 	{
    return "7";
  }
  // thunderstorm
  // 11d
  if (icon == "11d") 	{
    return "P";
  }
  // 11n
  if (icon == "11n") 	{
    return "6";
  }
  // snow
  // 13d
  if (icon == "13d") 	{
    return "W";
  }
  // 13n
  if (icon == "13n") 	{
    return "#";
  }
  // mist
  // 50d
  if (icon == "50d") 	{
    return "M";
  }
  // 50n
  if (icon == "50n") 	{
    return "M";
  }
  // Nothing matched: N/A
  return ")";

}