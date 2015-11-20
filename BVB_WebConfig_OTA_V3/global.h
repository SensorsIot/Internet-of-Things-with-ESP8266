#ifndef GLOBAL_H
#define GLOBAL_H

ESP8266WebServer server(80);									// The Webserver
boolean firstStart = true;										// On firststart = true, NTP will try to get a valid time
int AdminTimeOutCounter = 0;									// Counter for Disabling the AdminMode
strDateTime DateTime;											// Global DateTime structure, will be refreshed every Second
WiFiUDP UDPNTPClient;											// NTP Client
unsigned long UnixTimestamp = 0;								// GLOBALTIME  ( Will be set by NTP)
boolean Refresh = false; // For Main Loop, to refresh things like GPIO / WS2812
int cNTP_Update = 0;											// Counter for Updating the time via NTP
Ticker tkSecond;												// Second - Timer for Updating Datetime Structure
boolean AdminEnabled = true;		// Enable Admin Mode for a given Time
byte Minute_Old = 100;				// Helpvariable for checking, when a new Minute comes up (for Auto Turn On / Off)

#define ACCESS_POINT_NAME  "ESP"
//#define ACCESS_POINT_PASSWORD  "12345678"
#define AdminTimeOut 6  // Defines the Time in Seconds, when the Admin-Mode will be diabled


struct strConfig {
  String ssid;
  String password;
  byte  IP[4];
  byte  Netmask[4];
  byte  Gateway[4];
  boolean dhcp;
  String ntpServerName;
  long Update_Time_Via_NTP_Every;
  long timezone;
  boolean daylight;
  String DeviceName;
  byte wayToStation;
  byte warningBegin;
  String base;
  String right;
  String left;
} config;



//custom declarations
int freq = -1; // signal off

#ifdef ESP_12
//ESP-12E
#define BEEPPIN 4
#define LEFTPIN 16
#define RIGHTPIN 14
#else
//NodeMCU
#define BEEPPIN D8
#define LEFTPIN D5
#define RIGHTPIN D4
#endif

int counter = 0;

#define LOOP_FAST 20000
#define LOOP_SLOW 60000
#define BEEPTICKER 100
char serverTransport[] = "transport.opendata.ch";
String url;
String line;
const int httpPort = 80;
const int intensity[] = {1, 1, 1, 5, 5, 10, 10, 20, 20, 40, 40};
int loopTime = LOOP_SLOW;
unsigned long entryLoop;
bool okNTPvalue = false;  // NTP signal ok
bool requestOK = false;
int minTillDep, secTillDep, lastMinute;
ledColor ledColor;
boolean ledState = false;
unsigned long  ledCounter;
char str[80];

int beepOffTimer,beepOnTimer,beepOffTime,beepOnTime ;

enum defDirection {
  none,
  left,
  right
};

enum defBeeper {
  beeperOn,
  beeperOff,
  beeperIdle
};

volatile defBeeper beeperStatus = beeperIdle;

enum defStatus {
  admin,
  waitForNTP,
  doNothing,
  requestLeft,
  requestRight,
  waitLeft,
  waitRight,
  connectLeft,
  connectRight,
  webLeft,
  webRight
};
defStatus status = doNothing;
defStatus lastStatus;

byte currentDirection;



/*
**
** CONFIGURATION HANDLING
**
*/
void ConfigureWifi()
{
  Serial.println("Configuring Wifi");

  WiFi.begin ("WLAN", "password");

  WiFi.begin (config.ssid.c_str(), config.password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    led(red);
    delay(500);
  }
  if (!config.dhcp)
  {
    WiFi.config(IPAddress(config.IP[0], config.IP[1], config.IP[2], config.IP[3] ),  IPAddress(config.Gateway[0], config.Gateway[1], config.Gateway[2], config.Gateway[3] ) , IPAddress(config.Netmask[0], config.Netmask[1], config.Netmask[2], config.Netmask[3] ));
  }
}


void WriteConfig()
{

  Serial.println("Writing Config");
  EEPROM.write(0, 'C');
  EEPROM.write(1, 'F');
  EEPROM.write(2, 'G');

  EEPROM.write(16, config.dhcp);
  EEPROM.write(17, config.daylight);

  EEPROMWritelong(18, config.Update_Time_Via_NTP_Every); // 4 Byte
  EEPROMWritelong(22, config.timezone); // 4 Byte

  EEPROM.write(32, config.IP[0]);
  EEPROM.write(33, config.IP[1]);
  EEPROM.write(34, config.IP[2]);
  EEPROM.write(35, config.IP[3]);

  EEPROM.write(36, config.Netmask[0]);
  EEPROM.write(37, config.Netmask[1]);
  EEPROM.write(38, config.Netmask[2]);
  EEPROM.write(39, config.Netmask[3]);

  EEPROM.write(40, config.Gateway[0]);
  EEPROM.write(41, config.Gateway[1]);
  EEPROM.write(42, config.Gateway[2]);
  EEPROM.write(43, config.Gateway[3]);

  WriteStringToEEPROM(64, config.ssid);
  WriteStringToEEPROM(96, config.password);
  WriteStringToEEPROM(128, config.ntpServerName);

  // Application Settings
  WriteStringToEEPROM(160, config.base);
  WriteStringToEEPROM(192, config.left);
  WriteStringToEEPROM(224, config.right);
  EEPROM.write(256, config.warningBegin);
  EEPROM.write(257, config.wayToStation);

  WriteStringToEEPROM(258, config.DeviceName);

  EEPROM.commit();
}
boolean ReadConfig()
{
  Serial.println("Reading Configuration");
  if (EEPROM.read(0) == 'C' && EEPROM.read(1) == 'F'  && EEPROM.read(2) == 'G' )
  {
    Serial.println("Configurarion Found!");
    config.dhcp = 	EEPROM.read(16);

    config.daylight = EEPROM.read(17);

    config.Update_Time_Via_NTP_Every = EEPROMReadlong(18); // 4 Byte

    config.timezone = EEPROMReadlong(22); // 4 Byte

    config.IP[0] = EEPROM.read(32);
    config.IP[1] = EEPROM.read(33);
    config.IP[2] = EEPROM.read(34);
    config.IP[3] = EEPROM.read(35);
    config.Netmask[0] = EEPROM.read(36);
    config.Netmask[1] = EEPROM.read(37);
    config.Netmask[2] = EEPROM.read(38);
    config.Netmask[3] = EEPROM.read(39);
    config.Gateway[0] = EEPROM.read(40);
    config.Gateway[1] = EEPROM.read(41);
    config.Gateway[2] = EEPROM.read(42);
    config.Gateway[3] = EEPROM.read(43);
    config.ssid = ReadStringFromEEPROM(64);
    config.password = ReadStringFromEEPROM(96);
    config.ntpServerName = ReadStringFromEEPROM(128);


    // Application parameters
    config.base = ReadStringFromEEPROM(160);
    config.left = ReadStringFromEEPROM(192);
    config.right = ReadStringFromEEPROM(224);
    config.warningBegin = EEPROM.read(256);
    config.wayToStation = EEPROM.read(257);

    config.DeviceName = ReadStringFromEEPROM(306);
    return true;

  }
  else
  {
    Serial.println("Configurarion NOT FOUND!!!!");
    return false;
  }
}

/*
**
**  NTP
**
*/

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE];

bool NTPRefresh()
{
  bool okNTP = false;

  if (WiFi.status() == WL_CONNECTED)
  {
    UDPNTPClient.begin(2390);  // Port for NTP receive
    IPAddress timeServerIP;
    WiFi.hostByName(config.ntpServerName.c_str(), timeServerIP);

    //Serial.println("sending NTP packet...");
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    UDPNTPClient.beginPacket(timeServerIP, 123);
    UDPNTPClient.write(packetBuffer, NTP_PACKET_SIZE);
    UDPNTPClient.endPacket();

    delay(100);

    int cb = UDPNTPClient.parsePacket();
    if (!cb) {
      Serial.println("No NTP packet yet");
    }
    else
    {
      okNTP = true; Serial.print("NTP packet received, length=");
      Serial.println(cb);
      UDPNTPClient.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      UnixTimestamp = epoch;
    }
  } else {
    Serial.println("Internet yet not connected");
    delay(500);
  }
  yield();
  return okNTP;
}

void Second_Tick()
{
  strDateTime tempDateTime;
  AdminTimeOutCounter++;
  cNTP_Update++;
  UnixTimestamp++;
  ConvertUnixTimeStamp(UnixTimestamp +  (config.timezone *  360) , &tempDateTime);
  if (config.daylight) // Sommerzeit beachten
    if (summertime(tempDateTime.year, tempDateTime.month, tempDateTime.day, tempDateTime.hour, 0))
    {
      ConvertUnixTimeStamp(UnixTimestamp +  (config.timezone *  360) + 3600, &DateTime);
    }
    else
    {
      DateTime = tempDateTime;
    }
  else
  {
    DateTime = tempDateTime;
  }
  Refresh = true;

  //-------------------------------------- USER LOAD ---------------------------------------------

  // non blocking code, very fast
}

#endif
