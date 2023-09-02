

void helloWorld(const char *HelloWorld);
void UDBDebug(String message);
void UDBDebug(const char * message);
void MQTT_callback(char* topic, byte* payload, unsigned int length);


enum alignment {LEFT, RIGHT, CENTER};
void DisplayWXicon(int x, int y, String IconName, bool IconSize);
void drawString(int x, int y, String text, alignment align);
void Nodata(int x, int y, bool IconSize, String IconName);
void addmoon(int x, int y, int scale, bool IconSize);
void Visibility(int x, int y, String Visi);
void CloudCover(int x, int y, int CCover) ;
void Haze(int x, int y, bool IconSize, String IconName) ;
void Fog(int x, int y, bool IconSize, String IconName) ;
void Snow(int x, int y, bool IconSize, String IconName);
void Tstorms(int x, int y, bool IconSize, String IconName) ;
void ChanceRain(int x, int y, bool IconSize, String IconName);
void ExpectRain(int x, int y, bool IconSize, String IconName);
void Rain(int x, int y, bool IconSize, String IconName);
void Cloudy(int x, int y, bool IconSize, String IconName);
void MostlyCloudy(int x, int y, bool IconSize, String IconName);
void MostlySunny(int x, int y, bool IconSize, String IconName);
void Sunny(int x, int y, bool IconSize, String IconName);

void alertMessage(int x, int y, String text, alignment align);