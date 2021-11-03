/*
remix from HarryFun's great Morphing Digital Clock idea https://github.com/hwiguna/HariFun_166_Morphing_Clock
follow the great tutorial there and eventually use this condide as alternative

provided 'AS IS', use at your own risk
 * mirel.t.lazar@gmail.com

other remix by niq_ro -  nicu.florica@gmail.com
 * ver.1.0 - presure in mmHg instead mPa
 * ver.1.a - added "C" at every temperature, shortet date format (year as 21 instead 2021), presure or umidity (at 3 sec) in upper right corner
 * ver.1.b - change presure format (762Hg instead 762P), change few letters
 * ver.1.c - remove date and move temperature (actual, minimum, maximum) under clock, change other few letters
 * ver.1.d - move all info numerical value under clock (alternatively show), remain upper just icons, removed fireworks part
 * ver.1.e - added permannent type of wheater as text (icons or animation as options)
 * ver.1.e.2 - added colours at digits and description of weather
 * ver.1.f - extract more info from openweathermap data (wind, clouds percent)
 * ver.1.g - changed NtpClientLib.h with NtpClient.h for extract easy day of week
 * ver.1.h - added romanian language for description, clear zone display when openweathermap not send any data or APIkey is wrong, remain just data or name of day week
 * ver.1.h2 - small correcionfor clear zone :)
 * ver.1.i - changed animated icon from https://github.com/cybr1d-cybr1d/MorphingClockRemix
 * ver.1.j - clear seconds zone when weather data is uploaded (fill will "search info" text)
 * ver.1.k - niq_ro changed moon simbol
 * ver.1k1 - small corrections (thunderstorm)
 * ver.1.l - added OTA
 * ver.1.l.1 - added variable for sunshine ans sunrise
 * ver.1.l.2 - changed moon with clouds animated icons
 * ver.1.l.3 - brighness with automatic change (night/day) + automatic brithness on day (clouds %)
*/

#include <TimeLib.h>
#include <NTPClient.h>  // https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/ 
#include <WiFiUdp.h>    // https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#define double_buffer
#include <PxMatrix.h>  // version=1.3.0

#define USE_ICONS
#define USE_WEATHER_ANI

unsigned long tpafisare = 0;
unsigned long tppauza = 3000;
byte ics = 0;
byte lang0 = 1; // 0 - english or romana, 1 - mixt (english/romana)
byte lang1 = 0;  // 0 - english, 1 - romana (alternative)

int zi = 0; // https://github.com/gmag11/NtpClient/issues/84

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const long utcOffsetInSeconds = 7200;  // +2
//const long utcOffsetInSeconds = 10800;  // +3

//=== WIFI MANAGER ===
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager  // version=0.16.0
char wifiManagerAPName[] = "MorphClk";
char wifiManagerAPPassword[] = "MorphClk";

//== DOUBLE-RESET DETECTOR ==
#include <DoubleResetDetector.h>
#define DRD_TIMEOUT 10 // Second-reset must happen within 10 seconds of first reset to be considered a double-reset
#define DRD_ADDRESS 0 // RTC Memory Address for the DoubleResetDetector to use
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

//== SAVING CONFIG ==
#include "FS.h"
#include <ArduinoJson.h>  // version=5.13.5
bool shouldSaveConfig = false; // flag for saving data

//callback notifying us of the need to save config
void saveConfigCallback () 
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

#ifdef ESP8266
#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2
#endif

// Pins for LED MATRIX
PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

//=== SEGMENTS ===
byte cin = 25; //color intensity
byte cin0 = 26;
byte cinmin = 25;  // night
byte cinmax = 70; // day
byte rasarit = 7;
byte apus = 17;
byte noapte = 0;


#include "Digit.h"
Digit digit0(&display, 0, 63 - 1 - 9*1, 8, display.color565(0, 0, cin));
Digit digit1(&display, 0, 63 - 1 - 9*2, 8, display.color565(0, 0, cin));
Digit digit2(&display, 0, 63 - 4 - 9*3, 8, display.color565(0, 0, cin));
Digit digit3(&display, 0, 63 - 4 - 9*4, 8, display.color565(0, 0, cin));
Digit digit4(&display, 0, 63 - 7 - 9*5, 8, display.color565(0, 0, cin));
Digit digit5(&display, 0, 63 - 7 - 9*6, 8, display.color565(0, 0, cin));

#ifdef ESP8266
// ISR for display refresh
void display_updater ()
{
  //display.displayTestPattern(70);
  display.display (70);
}
#endif

void getWeather ();

void configModeCallback (WiFiManager *myWiFiManager) 
{
  Serial.println ("Entered config mode");
  Serial.println (WiFi.softAPIP());

  // You could indicate on your screen or by an LED you are in config mode here

  // We don't want the next time the boar resets to be considered a double reset
  // so we remove the flag
  drd.stop ();
}

char timezone[5] = "3";
char military[3] = "Y";     // 24 hour mode? Y/N
char u_metric[3] = "Y";     // use metric for units? Y/N
char date_fmt[7] = "D.M.Y"; // date format: D.M.Y or M.D.Y or M.D or D.M or D/M/Y.. looking for trouble
bool loadConfig () 
{
  File configFile = SPIFFS.open ("/config.json", "r");
  if (!configFile) 
  {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size ();
  if (size > 1024) 
  {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes (buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success ()) 
  {
    Serial.println("Failed to parse config file");
    return false;
  }

  strcpy (timezone, json["timezone"]);
  strcpy (military, json["military"]);
  //avoid reboot loop on systems where this is not set
  if (json.get<const char*>("metric"))
    strcpy (u_metric, json["metric"]);
  else
  {
    Serial.println ("metric units not set, using default: Y");
  }
  if (json.get<const char*>("date-format"))
    strcpy (date_fmt, json["date-format"]);
  else
  {
    Serial.println ("date format not set, using default: D.M.Y");
  }
  
  return true;
}

bool saveConfig () 
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["timezone"] = timezone;
  json["military"] = military;
  json["metric"] = u_metric;
  json["date-format"] = date_fmt;

  File configFile = SPIFFS.open ("/config.json", "w");
  if (!configFile)
  {
    Serial.println ("Failed to open config file for writing");
    return false;
  }

  Serial.println ("Saving configuration to file:");
  Serial.print ("timezone=");
  Serial.println (timezone);
  Serial.print ("military=");
  Serial.println (military);
  Serial.print ("metric=");
  Serial.println (u_metric);
  Serial.print ("date-format=");
  Serial.println (date_fmt);

  json.printTo (configFile);
  return true;
}

#include "TinyFont.h"
const byte row0 = 2+0*10;
const byte row1 = 2+1*10;
const byte row2 = 2+2*10;
void wifi_setup ()
{
  //-- Config --
  if (!SPIFFS.begin ()) 
  {
    Serial.println ("Failed to mount FS");
    return;
  }
  loadConfig ();

  //-- Display --
  display.fillScreen (display.color565 (0, 0, 0));
  display.setTextColor (display.color565 (0, 0, cin));
 
    TFDrawText (&display, String(" MORPHING CLOCK "), 0, 6, display.color565(0, 0, cin));
    TFDrawText (&display, String("niq_ro's rem~ix "), 0, 13, display.color565(cin, cin, 0));
    TFDrawText (&display, String("   ver. 1.l.3   "), 0, 20, display.color565(cin, 0, cin));
    delay(3000);
    display.fillScreen (display.color565 (0, 0, 0));
    
  //-- WiFiManager --
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback (saveConfigCallback);
  WiFiManagerParameter timeZoneParameter ("timeZone", "Time Zone", timezone, 5); 
  wifiManager.addParameter (&timeZoneParameter);
  WiFiManagerParameter militaryParameter ("military", "24Hr (Y/N)", military, 3); 
  wifiManager.addParameter (&militaryParameter);
  WiFiManagerParameter metricParameter ("metric", "Metric Units (Y/N)", u_metric, 3); 
  wifiManager.addParameter (&metricParameter);
  WiFiManagerParameter dmydateParameter ("date_fmt", "Date Format (D.M.Y)", date_fmt, 6); 
  wifiManager.addParameter (&dmydateParameter);

  //-- Double-Reset --
  if (drd.detectDoubleReset ()) 
  {
    Serial.println ("Double Reset Detected");
/*
    display.setCursor (0, row0);
    display.print ("AP:");
    display.print (wifiManagerAPName);

    display.setCursor (0, row1);
    display.print ("Pw:");
    display.print (wifiManagerAPPassword);
   
    display.setCursor (0, row2);
    display.print ("192.168.4.1");
*/
    display.fillScreen (display.color565 (0, 0, 0));
    TFDrawText (&display, String("AP:"), 0, 0, display.color565(0, 0, cin));
    TFDrawText (&display, String(wifiManagerAPName), 15, 0, display.color565(cin, 0, cin));
    TFDrawText (&display, String("Pw:"), 0, 10, display.color565(0, 0, cin));
    TFDrawText (&display, String(wifiManagerAPPassword), 15, 10, display.color565(cin, 0, cin));
    TFDrawText (&display, String("IP:"), 0, 20, display.color565(0, 0, cin));
    TFDrawText (&display, String("192.168.4.1"), 15, 20, display.color565(cin, 0, cin));

    wifiManager.startConfigPortal (wifiManagerAPName, wifiManagerAPPassword);

    display.fillScreen (display.color565(0, 0, 0));
  } 
  else 
  {
    Serial.println ("No Double Reset Detected");

    //display.setCursor (2, row1);
    //display.print ("connecting");
    TFDrawText (&display, String("   CONNECTING   "), 0, 13, display.color565(0, 0, cin));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name wifiManagerAPName
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect (wifiManagerAPName);
  }
  
  Serial.print ("timezone=");
  Serial.println (timezone);
  Serial.print ("military=");
  Serial.println (military);
  Serial.print ("metric=");
  Serial.println (u_metric);
  Serial.print ("date-format=");
  Serial.println (date_fmt);
  //timezone
  strcpy (timezone, timeZoneParameter.getValue ());
  //military time
  strcpy (military, militaryParameter.getValue ());
  //metric units
  strcpy (u_metric, metricParameter.getValue ());
  //date format
  strcpy (date_fmt, dmydateParameter.getValue ());
  //display.fillScreen (0);
  //display.setCursor (2, row1);
  TFDrawText (&display, String("     ONLINE     "), 0, 13, display.color565(0, 0, cin));
  Serial.print ("WiFi connected, IP address: ");
  Serial.println (WiFi.localIP ());
  //

  if (shouldSaveConfig) 
  {
    saveConfig ();
  }
  drd.stop ();
  
  //delay (1500);
  getWeather ();
}

byte hh;
byte mm;
byte ss;
byte zz;
byte ll;
int yy;
byte ntpsync = 1;
//

byte prevhh = 255;
byte prevmm = 255;
byte prevss = 255;
long tnow;
unsigned long tpceas = 0;

void setup()
{	
	Serial.begin (115200);
  //display setup
  display.begin (16);
#ifdef ESP8266
  display_ticker.attach (0.002, display_updater);
#endif
  //
  wifi_setup ();
  //
   
// Port defaults to 8266
  // ArduinoOTA.setPort(8266);

 // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("OtherMorphingClock");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  

// Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(utcOffsetInSeconds);

cin = cinmin;
  
  //prep screen for clock display
  display.fillScreen (0);
  int cc_gry = display.color565 (128, 128, 128);
  //reset digits color
  digit0.SetColor (cc_gry);
  digit1.SetColor (cc_gry);
  digit2.SetColor (cc_gry);
  digit3.SetColor (cc_gry);
  digit4.SetColor (cc_gry);
  digit5.SetColor (cc_gry);
  digit1.DrawColon (cc_gry);
  digit3.DrawColon (cc_gry);
timeClient.update();
hh = timeClient.getHours();
mm = timeClient.getMinutes();
ss = timeClient.getSeconds();
zi = timeClient.getDay();
tpceas = millis();
digit0.Draw (ss % 10);
    digit1.Draw (ss / 10);
    digit2.Draw (mm % 10);
    digit3.Draw (mm / 10);
    digit4.Draw (hh % 10);
    digit5.Draw (hh / 10);
}

//open weather map api key 
String apiKey   = "manybeersforniq"; //e.g a hex string like "abcdef0123456789abcdef0123456789"
//the city you want the weather for 
String location = "Craiova,RO";  //"680332"; //"Craiova, RO"e.g. "Paris,FR"
char server[]   = "api.openweathermap.org";
WiFiClient client;
int tempMin = -10000;
int tempMax = -10000;
int tempM = -10000;
int presM = -10000;
int humiM = -10000;
int condM = -1;  //-1 - undefined, 0 - unk, 1 - sunny, 2 - cloudy, 3 - overcast, 4 - rainy, 5 - thunders, 6 - snow
int condid = -1; // 
float wind = -1; // speed wind in mps
int wind1 = -1; // speed wind in km/h
int unghi = -1;  // deg wind
int nori = -1; // percent for clouds

String condS = "";

void getWeather ()
{
  if (!apiKey.length ())
  {
    Serial.println ("w:missing API KEY for weather data, skipping"); 
    return;
  }
  Serial.print ("i:connecting to weather server.. "); 
  // if you get a connection, report back via serial: 
  if (client.connect (server, 80))
  { 
    Serial.println ("connected."); 
    // Make a HTTP request: 
    client.print ("GET /data/2.5/weather?"); 
    client.print ("q="+location); 
    client.print ("&appid="+apiKey); 
    client.print ("&cnt=1"); 
    (*u_metric=='Y')?client.println ("&units=metric"):client.println ("&units=imperial");
    client.println ("Host: api.openweathermap.org"); 
    client.println ("Connection: close");
    client.println (); 
  } 
  else 
  { 
    Serial.println ("w:unable to connect");
    return;
  } 
  delay (1000);
  String sval = "";
  int bT, bT2;
  //do your best
  String line = client.readStringUntil ('\n');
  if (!line.length ())
    Serial.println ("w:unable to retrieve weather data");
  else
  {
    Serial.print ("weather:"); 
    Serial.println (line); 
    //id
    bT = line.indexOf ("\"id\":");  
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 5);
      sval = line.substring (bT + 5, bT2);
      Serial.print ("id: ");
      Serial.println (sval);
      condid = sval.toInt ();
    }
    else
      Serial.println ("id NOT found!");

// part from https://github.com/cybr1d-cybr1d/MorphingClockRemix/
    bT = line.indexOf ("\"icon\":\"");
    if (bT > 0)
    {
      bT2 = line.indexOf ("\"", bT + 8);
      sval = line.substring (bT + 8, bT2);
      Serial.print ("cond ");
      Serial.println (sval);
      //0 - unk, 1 - sunny, 2 - cloudy, 3 - overcast, 4 - rainy, 5 - thunders, 6 - snow
      if (sval.equals("01d"))
      {
        condM = 1; //sunny
        noapte = 0;
      }
      else if (sval.equals("01n"))
      {
        condM = 8; //clear night
        noapte = 1;
      }  
      else if (sval.equals("02d"))
      {
        condM = 2; //partly cloudy day
        noapte = 0;
      }
      else if (sval.equals("02n"))
      {
        condM = 10; //partly cloudy night
        noapte = 1;
      }
      else if (sval.equals("03d"))
      {
        condM = 3; //overcast day
        noapte = 0;
      }
      else if (sval.equals("03n"))
      {
        condM = 11; //overcast night
        noapte = 1;
      }
      else if (sval.equals("04d"))
      {
        condM = 3;//overcast day
        noapte = 0;
      } 
      else if (sval.equals("04n"))
      {
        condM = 11;//overcast night
        noapte = 1;
      }
      else if (sval.equals("09d"))
      {
        condM = 4; //rain
        noapte = 0;
      }
      else if (sval.equals("09n"))
      {
        condM = 4;
        noapte = 1;
      }
      else if (sval.equals("10d"))
      {
        condM = 4;
        noapte = 0;
      }
      else if (sval.equals("10n"))
      {
        condM = 4;
        noapte = 1;
      }
      else if (sval.equals("11d"))
      {
        condM = 5; //thunder
        noapte = 0;
      }
      else if (sval.equals("11n"))
      {
        condM = 5;
        noapte = 1;
      }
      else if (sval.equals("13d"))
      {
        condM = 6; //snow
        noapte = 0;
      }
      else if (sval.equals("13n"))
      {
        condM = 6;
        noapte = 1;
      }
      else if (sval.equals("50d"))
      {
        condM = 7; //haze (day)
        noapte = 0;
      }
      else if (sval.equals("50n"))
      {
        condM = 9; //fog (night)
        noapte = 1;
      }
      // condM = 10; // just for test
      //tempM = sval.toInt();
      condS = sval;
    }

//condM = 10;  // ico test

    //tempM
    bT = line.indexOf ("\"temp\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 7);
      sval = line.substring (bT + 7, bT2);
      Serial.print ("temp: ");
      Serial.print (sval);
      tempM = sval.toInt ();
      Serial.print (" ->");
      Serial.println (tempM);
    }
    else
      Serial.println ("temp NOT found!");

    //tempMin
    bT = line.indexOf ("\"temp_min\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print ("temp min: ");
      Serial.print (sval);
      tempMin = sval.toInt ();
      Serial.print (" ->");
      Serial.println (tempMin);
    }
    else
      Serial.println ("temp_min NOT found!");

    //tempMax
    bT = line.indexOf ("\"temp_max\":");
    if (bT > 0)
    {
     // bT2 = line.indexOf ("},", bT + 11);
      bT2 = line.indexOf (",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print ("temp max: ");
      Serial.print (sval);
      tempMax = sval.toInt ();
      Serial.print (" ->");
      Serial.println (tempMax);
    }
    else
      Serial.println ("temp_max NOT found!");

    //pressM
    bT = line.indexOf ("\"pressure\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print ("press: ");
      Serial.print (sval);
      presM = sval.toInt()*0.750062;
      Serial.print ("mPa ->");
      Serial.print (presM);
      Serial.println ("mmHg");
    }
    else
      Serial.println ("pressure NOT found!");

    //humiM
    bT = line.indexOf ("\"humidity\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print ("humi: ");
      Serial.print (sval);
      humiM = sval.toInt();
      Serial.print (" ->");
      Serial.println (humiM);
    }
    else
      Serial.println ("humidity NOT found!");

    //wind speed
    bT = line.indexOf ("\"speed\":");
    if (bT > 0)
    {    
      bT2 = line.indexOf (",\"", bT + 8);    
      sval = line.substring (bT + 8, bT2);
      Serial.print ("wind: ");
      Serial.print (sval);
      wind = sval.toFloat();
      Serial.print (" ->");
      Serial.print (wind);
      wind1 = (float)(3.6*wind+0.5);
      Serial.print ("m/s ->");
      Serial.print (wind1);
      Serial.println ("km/h");
    }
    else
      Serial.println ("wind speed NOT found!");

    //wind deg
    bT = line.indexOf ("\"deg\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 6);
      sval = line.substring (bT + 6, bT2);
      Serial.print ("deg: ");
      Serial.print (sval);
      unghi = sval.toInt();
      if (unghi >379.9) unghi = 0;
      Serial.print (" ->");
      Serial.println (unghi);
    }
    else
      Serial.println ("wind deg NOT found!");

 //clouds %
    bT = line.indexOf ("\"all\":");
    if (bT > 0)
    {
      bT2 = line.indexOf (",\"", bT + 6);
      sval = line.substring (bT + 6, bT2);
      Serial.print ("clouds %: ");
      Serial.print (sval);
      nori = sval.toInt();
      Serial.print (" ->");
      Serial.println (nori);
    }
    else
      Serial.println ("clouds % NOT found!");
     
  }//connected
}  // end get weather

#ifdef USE_ICONS

#include "TinyIcons.h"
 //icons 10x5: 10 cols, 5 rows - part from https://github.com/cybr1d-cybr1d/MorphingClockRemix/
 // moony icons changed by niq_ro
int moony_ico [50] = {
  //3 nuances: 0x18c3 < 0x3186 < 0x4a49
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int moony1_ico [50] = {
  //3 nuances: 0x18c3 < 0x3186 < 0x4a49
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int moony2_ico [50] = {
  //3 nuances: 0x18c3 < 0x3186 < 0x4a49
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int moony3_ico [50] = {
  //3 nuances: 0x18c3 < 0x3186 < 0x4a49
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0xffff,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int moony4_ico [50] = {
  //3 nuances: 0x18c3 < 0x3186 < 0x4a49
  0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int sunny_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffe0, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffe0,
  0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000,
};

int sunny1_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffe0, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffff,
  0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000,
};

int sunny2_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffe0,
  0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000,
};

int cloudy_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffe0, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0xffe0, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
};

int cloudy1_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff,
};

int cloudy2_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff,
  0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff,
};

int cloudy3_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffff,
  0xffff, 0xffff, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffff,
};

int cloudy4_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000,
};

int cloudy5_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0xffff,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000,
};

int cloudy6_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0xffe0,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0x0000, 0x0000,
};

int cloudy7_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0xffe0, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffe0, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0x0000, 0xffff,
  0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int cloudy8_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000,
  0xffff, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0xffe0,
  0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
};

int cloudy9_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0xffe0, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
  0xffe0, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
};

int cloudyn_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
};

int cloudy1n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0xffff, 0xffff, 0x0000,
  0xffff, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0xffff,
};

int cloudy2n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff,
  0xffff, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x0000, 0xffe0, 0xffe0, 0x0000, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0xffff, 0xffff,
};

int cloudy3n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0xffff,
  0xffff, 0xffff, 0xffff, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0xffff,
};

int cloudy4n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0x0000, 0x0000, 0x0000, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int cloudy5n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
};

int cloudy6n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000,
};

int cloudy7n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000,
};

int cloudy8n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int cloudy9n_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffff, 0xffff, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
};

int ovrcst_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0xffe0, 0xffff, 0xffff, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int ovrcst1_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffe0, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int ovrcst2_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0xffe0, 0xffff, 0xffff, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffe0,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int ovrcstn_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffff, 0xffff, 0x0000, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffe0, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000, 0x0000,
};

int thndr_ico [50] = {
  0x041f, 0xc618, 0x041f, 0xc618, 0xc618, 0xc618, 0x041f, 0xc618, 0xc618, 0x041f,
  0xc618, 0xc618, 0xc618, 0xc618, 0x041f, 0xc618, 0xc618, 0x041f, 0xc618, 0xc618,
  0xc618, 0x041f, 0xc618, 0xc618, 0xc618, 0x041f, 0xc618, 0xc618, 0xc618, 0xc618,
  0xc618, 0xc618, 0xc618, 0x041f, 0xc618, 0xc618, 0xc618, 0xc618, 0xc618, 0x041f,
  0xc618, 0x041f, 0xc618, 0xc618, 0xc618, 0xc618, 0x041f, 0xc618, 0x041f, 0xc618,
};

int rain_ico [50] = {
  0x041f, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f,
  0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x041f, 0x0000,
};

int rain1_ico [50] = {
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x041f, 0x0000,
  0x041f, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f,
  0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f,
};

int rain2_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x041f, 0x0000,
  0x041f, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f,
  0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000,
};

int rain3_ico [50] = {
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x041f, 0x0000,
  0x041f, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f,
  0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000,
};

int rain4_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f,
  0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x041f, 0x0000,
  0x041f, 0x0000, 0x041f, 0x0000, 0x0000, 0x0000, 0x041f, 0x0000, 0x0000, 0x041f,
};

int snow_ico [50] = {
  0xc618, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618,
  0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0xc618, 0x0000,
};

int snow1_ico [50] = {
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0xc618, 0x0000,
  0xc618, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618,
  0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618,
};

int snow2_ico [50] = {
  0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0xc618, 0x0000,
  0xc618, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618,
  0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000,
};

int snow3_ico [50] = {
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0xc618, 0x0000,
  0xc618, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618,
  0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000,
};

int snow4_ico [50] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618,
  0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0xc618, 0x0000,
  0xc618, 0x0000, 0xc618, 0x0000, 0x0000, 0x0000, 0xc618, 0x0000, 0x0000, 0xc618,
};

int mist_ico [50] = {
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
};

int mist1_ico [50] = {
  0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f,
  0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000,
};

int mist2_ico [50] = {
  0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f,
};

int mist3_ico [50] = {
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
  0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000,
  0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f,
};

int mistn_ico [50] = {
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
};

int mist1n_ico [50] = {
  0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000,
};

int mist2n_ico [50] = {
  0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000,
  0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f, 0xf81f,
};

int mist3n_ico [50] = {
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0xffe0, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0x0000, 0xffe0, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
  0x0000, 0x0000, 0x0000, 0x0000, 0xffe0, 0xffe0, 0x0000, 0x0000, 0x0000, 0x0000,
  0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0xffe0, 0x0000, 0xf81f, 0xf81f, 0x0000,
  0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f, 0xf81f, 0x0000, 0x0000, 0xf81f,
};

int *suny_ani[] = {sunny_ico, sunny1_ico, sunny2_ico, sunny1_ico, sunny2_ico};
int *clod_ani[] = {cloudy_ico, cloudy1_ico, cloudy2_ico, cloudy3_ico, cloudy4_ico, cloudy5_ico, cloudy6_ico, cloudy7_ico, cloudy8_ico, cloudy9_ico};
int *ovct_ani[] = {ovrcst_ico, ovrcst1_ico, ovrcst2_ico, ovrcst1_ico, ovrcst2_ico};
int *rain_ani[] = {rain_ico, rain1_ico, rain2_ico, rain3_ico, rain4_ico};
int *thun_ani[] = {thndr_ico, rain1_ico, rain2_ico, rain3_ico, rain4_ico};
int *snow_ani[] = {snow_ico, snow1_ico, snow2_ico, snow3_ico, snow4_ico};
int *mony_ani[] = {moony_ico, moony1_ico, moony_ico, moony_ico, moony_ico, moony2_ico, moony_ico, moony_ico, moony3_ico, moony_ico, moony_ico, moony_ico, moony_ico, moony4_ico, moony_ico, moony_ico, moony_ico};
int *mist_ani[] = {mist_ico, mist1_ico, mist2_ico, mist3_ico};
int *mistn_ani[] = {mistn_ico, mist1n_ico, mist2n_ico, mist3n_ico};
int *clodn_ani[] = {cloudyn_ico, cloudy1n_ico, cloudy2n_ico, cloudy3n_ico, cloudy4n_ico, cloudy5n_ico, cloudy6n_ico, cloudy7n_ico, cloudy8n_ico, cloudy9n_ico};
int *ovctn_ani[] = {ovrcstn_ico};

#endif

int xo = 1, yo = 26;
char use_ani = 0;
char daytime = 1;

void draw_date ()
{
  int cc_grn = display.color565 (0, cin, 0);
  Serial.println ("showing the date");

  timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();
  Serial.print("Epoch Time: ");
  Serial.println(epochTime);

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

  zz = ptm->tm_mday;
  ll = ptm->tm_mon+1;
  yy = ptm->tm_year+1900;

  //date below the clock
  String lstr = "   ";
  for (int i = 0; i < 5; i += 2)
  {
    switch (date_fmt[i])
    {
      case 'D':
        lstr += (zz < 10 ? "0" + String(zz) : String(zz));
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
      case 'M':
        lstr += (ll < 10 ? "0" + String(ll) : String(ll));
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
      case 'Y':
        lstr += String(yy);
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
    }
  }

    xo = 0; //0*TF_COLS; 
    yo = 26;
    int cc_niq = display.color565 (0, cin, cin);
    TFDrawText (&display, lstr, xo, yo, cc_niq);
    xo = 13*TF_COLS;
    int cc_blk = display.color565 (0, 0, 0);
    TFDrawText (&display, "   ", xo, yo, cc_blk);
}

void draw_animations (int stp)
{
  //weather icon animation
  int xo = 0*TF_COLS; 
  int yo = 1;
  //0 - unk, 1 - sunny, 2 - cloudy, 3 - overcast, 4 - rainy, 5 - thunders, 6 - snow
  if (use_ani)
  {
    int *af = NULL;
    //weather/night icon
    //if (!daytime)
      //af = mony_ani[stp%5];
    //else
    //{
      switch (condM)
      {
        case 1:
            af = suny_ani[stp%5];
          break;
        case 2:
            af = clod_ani[stp%10];
          break;
        case 3:
            af = ovct_ani[stp%5];
          break;
        case 4:
            af = rain_ani[stp%5];
          break;
        case 5:
            af = thun_ani[stp%5];
          break;
        case 6:
            af = snow_ani[stp%5];
          break;
        case 7:
            af = mist_ani[stp%4];
          break;
        case 8:
            af = mony_ani[stp%17];
          break;
        case 9:
            af = mistn_ani[stp%4];
          break;
        case 10:
            af = clodn_ani[stp%10];
          break;
        case 11:
            af = ovctn_ani[stp%1];
          break;
      }
    //}
    //draw animation
    if (af)
      DrawIcon (&display, af, xo, yo, 10, 5);
  }
}

void draw_weather_conditions ()
{
  //0 - unk, 1 - sunny, 2 - cloudy, 3 - overcast, 4 - rainy, 5 - thunders, 6 - snow
  Serial.print ("weather conditions ");
  Serial.println (condM);
  xo = 0*TF_COLS;
  yo = 1;
  if (condM == 0 && daytime)
  {
    Serial.print ("!weather condition icon unknown, show: ");
    Serial.println (condS);
    int cc_dgr = display.color565 (255, 0, 0);
    //draw the first 5 letters from the unknown weather condition
    String lstr = "?";
    lstr.toUpperCase ();
    TFDrawText (&display, lstr, xo, yo, cc_dgr);
  }
  else
  //{
    //TFDrawText (&display, String("     "), xo, yo, 0);
  //}
  switch (condM)
  {
    case 0://unk
      break;
    case 1://sunny
   // DrawIcon (&display, af, xo, yo, 10, 5);
      DrawIcon (&display, sunny_ico, xo, yo, 10, 5);
      //DrawIcon (&display, cloudy_ico, xo, yo, 10, 5);
      //DrawIcon (&display, ovrcst_ico, xo, yo, 10, 5);
      //DrawIcon (&display, rain_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 2://cloudy
      DrawIcon (&display, cloudy_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 3://overcast
      DrawIcon (&display, ovrcst_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 4://rainy
      DrawIcon (&display, rain_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 5://thunders
      DrawIcon (&display, thndr_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 6://snow
      DrawIcon (&display, snow_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 7://mist
      DrawIcon (&display, mist_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 8://clear night
      DrawIcon (&display, moony_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 9://fog night
      DrawIcon (&display, mistn_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 10://partly cloudy night
      DrawIcon (&display, cloudyn_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
    case 11://cloudy night
      DrawIcon (&display, ovrcstn_ico, xo, yo, 10, 5);
      use_ani = 1;
      break;
  }
  xo = 3*TF_COLS; 
  yo = 1;
  Serial.print ("!weather condition icon unknown, show: ");
  Serial.println (condS);
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);

//condid = 200;  // just test

String lstr = "";
if (lang1 == 0)
{
if ((condid >= 200) and (condid <= 232)) lstr = "Thunderstorm"; //"furtuna, ";
if ((condid >= 300) and (condid <= 321)) lstr = "Drizzle     "; //"burnita, ";
if ((condid >= 500) and (condid <= 531)) lstr = "Rain        "; //"ploaie, ";
if ((condid >= 600) and (condid <= 622)) lstr = "Snow        "; //"ninsoare, ";
if ((condid >= 701) and (condid <= 781)) lstr = "Dust / Fog  "; //"praf/ceata, ";
if (condid == 800)                       lstr = "Clear sky   "; //"cer senin ";
if ((condid >= 801) and (condid <= 804)) lstr = "Clouds:     "; //"innorat, ";
}
else
{
if ((condid >= 200) and (condid <= 232)) lstr = "Furtuna     "; //"furtuna, ";
if ((condid >= 300) and (condid <= 321)) lstr = "Burnita     "; //"burnita, ";
if ((condid >= 500) and (condid <= 531)) lstr = "Ploaie      "; //"ploaie, ";
if ((condid >= 600) and (condid <= 622)) lstr = "Ninsoare    "; //"ninsoare, ";
if ((condid >= 701) and (condid <= 781)) lstr = "Praf / ceata"; //"praf/ceata, ";
if (condid == 800)                       lstr = "Cer senin   "; //"cer senin ";
if ((condid >= 801) and (condid <= 804)) lstr = "Innorat:    "; //"innorat, ";
}

lstr.toUpperCase ();
TFDrawText (&display, lstr, xo, yo, cc_grn);

lstr = "";
if ((condid >= 801) and (condid <= 804))
{
  if (nori < 100) lstr += " ";
  if (nori < 10) lstr += " ";
  lstr = lstr + String(nori); // 

xo = 11*TF_COLS;
TFDrawText (&display, lstr, xo, yo, cc_ylw);

xo = 14*TF_COLS;
TFDrawText (&display, String("% "), xo, yo, cc_cer);
xo = 15*TF_COLS;
TFDrawText (&display, " ", xo, yo+2, cc_cer);
}
else
    nori = 0;
}


void loop()
{
   ArduinoOTA.handle();
   
	static int i = 0;
	static int last = 0;
  static int cm;
  //time changes every miliseconds, we only want to draw when digits actually change

if ((millis() - tpceas > 1000) or (yy == 1970))
{
timeClient.update();
hh = timeClient.getHours();
mm = timeClient.getMinutes();
ss = timeClient.getSeconds();
zi = timeClient.getDay();
tpceas = millis();
}

  //animations?
  cm = millis ();
  //

  //weather animations
  if ((cm - last) > 150)
  {
    //Serial.println(millis() - last);
    last = cm;
    i++;
    //
    draw_animations (i);
    //
  }
  //
  if (ntpsync)
  {
    ntpsync = 0;
    //
    prevss = ss;
    prevmm = mm;
    prevhh = hh;
    //brightness control: dimmed during the night(cinmin), bright during the day(cinmax)
    if (noapte == 1 && cin == cinmax)
 //   if (hh >= apus && cin == cinmax)
    {
      cin = cinmin;
      Serial.println ("night mode brightness");
     // daytime = 0;
    }
    /*
    if (hh < rasarit && cin == cinmax)
    {
      cin = cinmin;
      Serial.println ("night mode brightness");
     // daytime = 0;
    }
    */
    //during the day, bright
    if (noapte == 0 && cin == cinmin)
   // if (hh >= rasarit && hh < apus && cin == cinmin)
    {
      cin = cinmax;
      Serial.println ("day mode brightness");
     // daytime = 1;
    }
   if (noapte == 0 && cin != cin0)
   {
    cin = cinmin+(cinmax-cinmin)*(1-nori/100); 
    Serial.print ("day mode brightness: ");
    Serial.print (cin);
    Serial.println ("/255");
   }
    
    //we had a sync so draw without morphing
    int cc_gry = display.color565 (128, 128, 128);
    int cc_dgr = display.color565 (30, 30, 30);

  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
//  int cc_gry = display.color565 (128, 128, 128);
//  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);
  
    //dark blue is little visible on a dimmed screen
    //int cc_blu = display.color565 (0, 0, cin);
    int cc_col = cc_gry;
    //
    if (cin == cinmin)
      cc_col = cc_dgr;
    //reset digits color
/*    
    digit0.SetColor (cc_col);
    digit1.SetColor (cc_col);
    digit2.SetColor (cc_col);
    digit3.SetColor (cc_col);
    digit4.SetColor (cc_col);
    digit5.SetColor (cc_col);
*/
    digit0.SetColor (cc_blu);  // units of seconds
    digit1.SetColor (cc_blu);  // tens of seconds
    digit2.SetColor (cc_ylw);  // units of minutes
    digit3.SetColor (cc_ylw);  // tens of minutes
    digit4.SetColor (cc_red);  // units of hours
    digit5.SetColor (cc_red);  // tens of hours
    //clear screen
    display.fillScreen (0);

    digit1.DrawColon (cc_col);
    digit3.DrawColon (cc_col);
    //military time?
    if (hh > 12 && military[0] == 'N')
      hh -= 12;
    //
    digit0.Draw (ss % 10);
    digit1.Draw (ss / 10);
    digit2.Draw (mm % 10);
    digit3.Draw (mm / 10);
    digit4.Draw (hh % 10);
    digit5.Draw (hh / 10);
  }
  else
  {
    //seconds
    if (ss != prevss) 
    {
      int s0 = ss % 10;
      int s1 = ss / 10;
      if (s0 != digit0.Value ()) digit0.Morph (s0);
      if (s1 != digit1.Value ()) digit1.Morph (s1);
      //ntpClient.PrintTime();
      prevss = ss;
      //refresh weather every 20mins at 30sec in the minute
      if (ss == 30 && ((mm % 20) == 0))
      {
     int cc_dgr = display.color565 (30, 30, 30);
     int cc_cer = display.color565 (cin, cin, cin);
     int cc_blk = display.color565 (0, 0, 0);
    xo = 10*TF_COLS;
    yo = 9;
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 14;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 19;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 10;
if (lang1 == 0)
    TFDrawText (&display, "SEARCH", xo, yo, cc_cer);
else
    TFDrawText (&display, " CAUT ", xo, yo, cc_cer);
    yo = 18;  
if (lang1 == 0) 
    TFDrawText (&display, " INFO ", xo, yo, cc_cer); 
else
    TFDrawText (&display, " DATE ", xo, yo, cc_cer);
      
      getWeather ();
      
    yo = 9;  
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 14;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 19;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
      int s0 = ss % 10;
      int s1 = ss / 10;

     
     timeClient.update();
     ss = timeClient.getSeconds();
     digit0.Draw (ss%10);
     digit1.Draw (ss/10);
     // if (s0 != digit0.Value ()) digit0.Morph (s0);
     // if (s1 != digit1.Value ()) digit1.Morph (s1);
       digit1.DrawColon (cc_cer);
       digit3.DrawColon (cc_cer);
      }
  /*      
      if (ss == 30)
      {
    //   10,6
     int cc_dgr = display.color565 (30, 30, 30);
     int cc_cer = display.color565 (cin, cin, cin);
     int cc_blk = display.color565 (0, 0, 0);
    xo = 10*TF_COLS;
    yo = 9;
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 14;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 19;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 11;
if (lang1 == 0)
    TFDrawText (&display, "search", xo, yo, cc_cer);
else
    TFDrawText (&display, " caut ", xo, yo, cc_cer);
    yo = 19;  
if (lang1 == 0) 
    TFDrawText (&display, " info ", xo, yo, cc_cer); 
else
    TFDrawText (&display, " date ", xo, yo, cc_cer); 
       delay(7000);
    yo = 9;  
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 14;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
    yo = 19;   
    TFDrawText (&display, "      ", xo, yo, cc_blk);
       digit0.Draw (ss % 10);
       digit1.Draw (ss / 10);
       digit1.DrawColon (cc_cer);
      }
  */      
    }
    //minutes
    if (mm != prevmm)
    {
      int m0 = mm % 10;
      int m1 = mm / 10;
      if (m0 != digit2.Value ()) digit2.Morph (m0);
      if (m1 != digit3.Value ()) digit3.Morph (m1);
      prevmm = mm;
      //
     //   draw_weather (); // ?
    }
    //hours
    if (hh != prevhh) 
    {
      prevhh = hh;
      //
      draw_date ();
      //brightness control: dimmed during the night(25), bright during the day(150)
      if (hh == 20 || hh == 8)
      {
        ntpsync = 1;
        //bri change is taken care of due to the sync
      }
      //military time?
      if (hh > 12 && military[0] == 'N')
        hh -= 12;
      //
      int h0 = hh % 10;
      int h1 = hh / 10;
      if (h0 != digit4.Value ()) digit4.Morph (h0);
      if (h1 != digit5.Value ()) digit5.Morph (h1);
    }//hh changed
  }

  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);
  int cc_blk = display.color565 (0, 0, 0);

 int  lcc = cc_red;
 
if (millis() - tpafisare > tppauza)
{
 if ((ics == 0) and (condM != 0)) draw_weather_conditions(); //weather conditions
 if (ics == 0) draw_date();
 if (ics == 1) show_zi();
 if ((ics == 2) and (tempM > -100)) show_temp(); 
 if ((ics == 3) and (humiM >0)) show_humi();
 if ((ics == 4) and (presM >0)) show_pres();
 if ((ics == 5) and (wind > -1)) show_wind();
 
tpafisare = millis();

ics = ics + 1;
if (ics >= 6) 
{
  ics = 0;
  if (lang0 == 1)
 {
  lang1 = lang1 + 1;
  lang1 = lang1%2;
//  Serial.print("lang1 = ");
//  Serial.println(lang1); 
 }
}
//  Serial.print("ics = ");
//  Serial.println(ics); 
}

xo = 15*TF_COLS;
yo = 0;
TFDrawText (&display, " ", xo, yo, cc_blk);
	//delay (0);
cin0 = cin;
}  // main loop

void show_humi()
{
      //-humidity
     yo = 26;
     xo = 0;
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);

 int  lcc = cc_red;
  if (lang1 == 0)
{
     TFDrawText (&display, "Hum~idity: ", xo, yo, cc_cer);
}
else
{
     TFDrawText (&display, "Um~iditate:", xo, yo, cc_cer);
}
    lcc = cc_red;
    if (humiM < 65)
      lcc = cc_grn;
    if (humiM < 35)
      lcc = cc_blu;
    if (humiM < 15)
      lcc = cc_wht;
    String lstr = "";
    if (humiM < 100) lstr = lstr + " ";
    lstr = lstr + String (humiM);
      xo = 11*TF_COLS;
      TFDrawText (&display, lstr, xo, yo, lcc);
      xo = 14*TF_COLS;
      TFDrawText (&display, "% ", xo, yo, cc_cer);
      xo = 15*TF_COLS;
      TFDrawText (&display, " ", xo, yo+2, cc_cer);
}

void show_pres()
{
   //-pressure
    yo = 26;
    xo = 0;
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);

 int  lcc = cc_red;
 if (lang1 == 0)
{ 
      TFDrawText (&display, "Press: ", xo, yo, cc_cer);
}
  else
{
      TFDrawText (&display, "Pres.: ", xo, yo, cc_cer);
}
    //lstr = String (presM); 
      xo = 7*TF_COLS;
      TFDrawText (&display, String (presM), xo, yo, cc_ylw);
      xo = 10*TF_COLS;
      TFDrawText (&display, "m~m~Hg", xo, yo, cc_cer);
}  

void show_temp()
{
  // actual temperature

//tempM = random(-30,30);  // just for test
//tempMin = random(-30,30);  // just for test
//tempMax = random(-30,30);  // just for test
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);

 int  lcc = cc_red;
xo = 0; //0*TF_COLS; 
yo = 26;
//TFDrawText (&display, "                ", xo, yo, lcc);
  String  lstr = "";
 if (tempM > -10000)
    {
    if (abs(tempM) < 10) lstr = lstr + " "; 
    if (tempMin == 0) lstr = lstr + " ";
    //-temperature
    lcc = cc_red;
    if (*u_metric == 'Y')
    {
      //C
      if (tempM >= 26)
        lcc = cc_cer;
      if (tempM < 26)
        lcc = cc_grn;
      if (tempM < 18)
        lcc = cc_ylw;
      if (tempM < 6)
        lcc = cc_wht;
      if (tempM < 0)
        lcc = cc_blu;
    }
    else
    {
      //F
      if (tempM >= 79)
        lcc = cc_cer;
      if (tempM < 79)
        lcc = cc_grn;
      if (tempM < 64)
        lcc = cc_blu;
      if (tempM < 43)
        lcc = cc_wht;
      if (tempM < 32)
        lcc = cc_blu;
    }
    if (tempM > 0) 
    lstr = lstr + "+" +String (tempM) + "*" + String((*u_metric=='Y')?"C":"F");
    else
    lstr = lstr + String (tempM) + "*" + String((*u_metric=='Y')?"C":"F");
    lstr = lstr + "  ";
  //  Serial.print ("temperature: ");
  //  Serial.println (lstr);
    }
    TFDrawText (&display, lstr, xo, yo, lcc);
lstr = "(";
 // minimum / maximum temperature
if (tempMin > -10000)
    {
     if (tempMin > 0) 
     lstr = lstr + "+" +String (tempMin);
     else
     lstr = lstr + String (tempMin);
   //   Serial.print ("temp min: ");
   //   Serial.println (lstr);
    }
    lstr = lstr + "/";
    if (tempMax > -10000)
    {
      if (tempMax > 0) 
      lstr = lstr + "+" +String (tempMax);
      else
      lstr = lstr + String (tempMax);
  //    Serial.print ("temp max: ");
  //    Serial.println (lstr);
    }
    lstr = lstr + ")";
    if (abs(tempMin) == 0) lstr = lstr + " "; 
    if (tempMin < 10) lstr = lstr + " "; 
    if (abs(tempMax) == 0) lstr = lstr + " "; 
    if (tempMax < 10) lstr = lstr + " "; 
    xo = 7*TF_COLS; 
    yo = 26;
  TFDrawText (&display, lstr, xo, yo, cc_cer);
}


void show_wind()
{
   //wind speed
    yo = 26;
    xo = 0;
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);
  int cc_blk = display.color565 (0, 0, 0);
  
 int  lcc = cc_red;
 if (lang1 == 0)
{
      TFDrawText (&display, "Wind: ", xo, yo, cc_cer);
}
else
{
   TFDrawText (&display, "Vant: ", xo, yo, cc_cer);
}
      String lstr = ""; 
if (lang1 == 0)
{
// http://snowfence.umn.edu/Components/winddirectionanddegrees.htm
if (unghi>=337.9 || unghi<=22.5)  lstr = lstr + " N"; // North
if (unghi>=22.5 && unghi<=67.5)   lstr = lstr + "NE"; // North - Est
if (unghi>=67.5 && unghi<=112.5)  lstr = lstr + " E"; //"ESTE : viento de Levante... ";
if (unghi>=112.5 && unghi<=157.5) lstr = lstr + "SE"; //"Sureste : viento de Siroco... ";
if (unghi>=157.5 && unghi<=202.5) lstr = lstr + " S"; //"SUR : viento de Ostro... ";
if (unghi>=202.5 && unghi<=247.5) lstr = lstr + "SW"; //"Suroeste : viento de Lebeche... ";
if (unghi>=247.5 && unghi<=295.5) lstr = lstr + " W"; //"OESTE : viento de Poniente... ";
if (unghi>=295.5 && unghi<=337.9) lstr = lstr + "NW"; //"Noroeste : viento de Maestro... ";
}
else
{
if (unghi>=337.9 || unghi<=22.5)  lstr = lstr + " N"; // North
if (unghi>=22.5 && unghi<=67.5)   lstr = lstr + "NE"; // North - Est
if (unghi>=67.5 && unghi<=112.5)  lstr = lstr + " E"; //"ESTE : viento de Levante... ";
if (unghi>=112.5 && unghi<=157.5) lstr = lstr + "SE"; //"Sureste : viento de Siroco... ";
if (unghi>=157.5 && unghi<=202.5) lstr = lstr + " S"; //"SUR : viento de Ostro... ";
if (unghi>=202.5 && unghi<=247.5) lstr = lstr + "SV"; //"Suroeste : viento de Lebeche... ";
if (unghi>=247.5 && unghi<=295.5) lstr = lstr + " V"; //"OESTE : viento de Poniente... ";
if (unghi>=295.5 && unghi<=337.9) lstr = lstr + "NV"; //"Noroeste : viento de Maestro... ";
  
}
     xo = 6*TF_COLS;
      TFDrawText (&display, lstr, xo, yo, cc_red);

      xo = 8*TF_COLS;
      lstr = ""; 
      if (wind1 < 10) lstr = lstr + " "; 
      if (wind1 < 100) lstr = lstr + " "; 
      lstr = lstr + String (wind1);
   //   Serial.println(lstr);
      TFDrawText (&display, lstr, xo, yo, cc_ylw);
      xo = 11*TF_COLS;
      TFDrawText (&display, "km~/h", xo, yo, cc_cer);
}  

void show_zi()
{
    yo = 26;
  int cc_wht = display.color565 (cin, cin, cin);
  int cc_red = display.color565 (cin, 0, 0);
  int cc_grn = display.color565 (0, cin, 0);
  int cc_blu = display.color565 (0, 0, cin);
  int cc_ylw = display.color565 (cin, cin, 0);
  int cc_gry = display.color565 (128, 128, 128);
  int cc_dgr = display.color565 (30, 30, 30);
  int cc_cer = display.color565 (cin, 0, cin);
  int cc_blk = display.color565 (0, 0, 0);
xo = 15*TF_COLS;
TFDrawText (&display, " ", xo, yo, cc_cer);
xo = 0;
//TFDrawText (&display, String("                "), xo, yo, cc_blk);
String lstr = "";
if (lang1 == 0)
{
  if (zi == 0)
     lstr = lstr + "    Sunday     ";
  if (zi == 1)
     lstr = lstr + "    Monday     ";
  if (zi == 2)
     lstr = lstr + "    Tuesday    ";
  if (zi == 3)
     lstr = lstr + "   Wednesday   ";
  if (zi == 4)
     lstr = lstr + "   Thursday    ";
  if (zi == 5)
     lstr = lstr + "    Friday     ";
  if (zi == 6)
     lstr = lstr + "   Saturday    ";
}
else
{
  if (zi == 0)
     lstr = lstr + "   Duminica    ";
  if (zi == 1)
     lstr = lstr + "     Luni      ";
  if (zi == 2)
     lstr = lstr + "     Marti     ";
  if (zi == 3)
     lstr = lstr + "    Miercuri   ";
  if (zi == 4)
     lstr = lstr + "      Joi      ";
  if (zi == 5)
     lstr = lstr + "    Vineri     ";
  if (zi == 6)
     lstr = lstr + "    Sambata    ";
}
     lstr.toUpperCase ();
TFDrawText (&display, lstr, xo, yo, cc_ylw);    
}
