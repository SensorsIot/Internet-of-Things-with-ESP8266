/*
  ESP_WebConfig

  Copyright (c) 2015 John Lassen. All rights reserved.
  This is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Latest version: 1.1.3  - 2015-07-20
  Changed the loading of the Javascript and CCS Files, so that they will successively loaded and that only one request goes to the ESP.

  The rest of the coding was done by Andreas Spiess 17.11.15


  First initial version to the public

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <WiFiUdp.h>

/* credentials.h has to be in your libary folder. its content is:

#define mySSID "yourSSID"
#define myPASSWORD "yourPassword"

If you do not want this file, hard-code your credentials in this sketch

*/

#include <credentials.h>

#include "helpers.h"
#include "global.h"
#include "NTP.h"


// Include the HTML, STYLE and Script "Pages"

#include "Page_Root.h"
#include "Page_Admin.h"
#include "Page_Script.js.h"
#include "Page_Style.css.h"
#include "Page_NTPsettings.h"
#include "Page_Information.h"
#include "Page_General.h"
#include "Page_applSettings.h"
#include "PAGE_NetworkConfiguration.h"
#include "example.h"

extern "C" {
#include "user_interface.h"
}
WiFiClient client;
Ticker ticker;

os_timer_t myTimer;


//OTA
const char* host = "esp8266-ota";
const uint16_t aport = 8266;
bool otaFlag = false;
WiFiServer TelnetServer(aport);
WiFiClient Telnet;
WiFiUDP OTA;

void setup ( void ) {
  EEPROM.begin(512);
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting ESP8266");

  os_timer_setfn(&myTimer, ISRbeepTicker, NULL);
  os_timer_arm(&myTimer, BEEPTICKER, true);

  // Custom
  pinMode(BEEPPIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LEFTPIN, INPUT_PULLUP);
  pinMode(RIGHTPIN, INPUT_PULLUP);
  ledColor = off;
  beep(3);
  delay(2000);
  if (!ReadConfig())
  {
    // DEFAULT CONFIG
    Serial.println("Setting default parameters");
    // please define the credentials either in the file credentials.h or here
    config.ssid = mySSID; // SSID of access point
    config.password = mypassword;  // password of access point
    config.dhcp = true;
    config.IP[0] = 192; config.IP[1] = 168; config.IP[2] = 1; config.IP[3] = 100;
    config.Netmask[0] = 255; config.Netmask[1] = 255; config.Netmask[2] = 255; config.Netmask[3] = 0;
    config.Gateway[0] = 192; config.Gateway[1] = 168; config.Gateway[2] = 1; config.Gateway[3] = 1;
    config.ntpServerName = "0.ch.pool.ntp.org";
    config.Update_Time_Via_NTP_Every =  5;
    config.timeZone = 1;
    config.isDayLightSaving = true;
    config.DeviceName = "Not Named";
    config.wayToStation = 3;
    config.warningBegin = 5;
    config.base = "lausen stutz";
    config.right = "lausen";
    config.left = "lausen";
    WriteConfig();
  }
  if (!(digitalRead(LEFTPIN) || digitalRead(RIGHTPIN))) {   // OTA Mode?
    Serial.println("OTA READY");
    otaFlag = true;
    otaInit();
    for (int i = 0; i < 10; i++) {
      ledColor = both;
      delay(200);
      ledColor = off;
      delay(200);
    }
  } else {
    // normal operation
    status = admin;
    tkSecond.attach(1, ISRsecondTick);

    currentDirection = EEPROM.read(300);
    Serial.printf("Current Direction %d \n", currentDirection);
    if ((currentDirection == left || currentDirection == right) && digitalRead(LEFTPIN)) {
      // ---------------- RECOVERY -----------------------
      status = recovery;
    } else {

      // normal operation
      WiFi.mode(WIFI_STA);
      WiFi.softAP( "ESP");

      // Admin page
      server.on ( "/", []() {
        Serial.println("admin.html");
        server.send ( 200, "text/html", PAGE_AdminMainPage );  // const char top of page
      }  );

      server.on ( "/favicon.ico",   []() {
        Serial.println("favicon.ico");
        server.send ( 200, "text/html", "" );
      }  );

      // Network config
      server.on ( "/config.html", send_network_configuration_html );
      // Info Page
      server.on ( "/info.html", []() {
        Serial.println("info.html");
        server.send ( 200, "text/html", PAGE_Information );
      }  );
      server.on ( "/ntp.html", send_NTP_configuration_html  );

      server.on ( "/appl.html", send_application_configuration_html  );
      server.on ( "/general.html", send_general_html  );
      //  server.on ( "/example.html", []() { server.send ( 200, "text/html", PAGE_EXAMPLE );  } );
      server.on ( "/style.css", []() {
        Serial.println("style.css");
        server.send ( 200, "text/plain", PAGE_Style_css );
      } );
      server.on ( "/microajax.js", []() {
        Serial.println("microajax.js");
        server.send ( 200, "text/plain", PAGE_microajax_js );
      } );
      server.on ( "/admin/values", send_network_configuration_values_html );
      server.on ( "/admin/connectionstate", send_connection_state_values_html );
      server.on ( "/admin/infovalues", send_information_values_html );
      server.on ( "/admin/ntpvalues", send_NTP_configuration_values_html );
      server.on ( "/admin/applvalues", send_application_configuration_values_html );
      server.on ( "/admin/generalvalues", send_general_configuration_values_html);
      server.on ( "/admin/devicename",     send_devicename_value_html);


      server.onNotFound ( []() {
        Serial.println("Page Not Found");
        server.send ( 400, "text/html", "Page not Found" );
      }  );
      server.begin();
      Serial.println( "HTTP server started" );

      AdminTimeOutCounter = 0;
      waitLoopEntry = millis();
    }
  }
}

void loop(void ) {
  yield(); // For ESP8266 to not dump

  if (otaFlag) {
    otaReceive();
  }
  else {
    customLoop();
  }
}

//-------------------------------------- CUSTOM ----------------------------------------

void customLoop() {
  defDirection _dir;
  String _line;

  // Non blocking code !!!
  switch (status) {
    case admin:
      ledColor = both;
      server.handleClient();

      // exit
      if (AdminTimeOutCounter > AdminTimeOut) {
        Serial.println("Admin Mode disabled!");
        ledColor = red;
        for (int hi = 0; hi < 3; hi++) beep(2);
        WiFi.mode(WIFI_AP);
        ConfigureWifi();
        ledColor = green;


        // exit
        waitJSONLoopEntry = 0;
        cNTP_Update = 999;
        status = idle;
        lastStatus = idle;
      }
      break;

    case idle:
      if (lastStatus != idle) Serial.println("Status idle");
      ledColor = off;
      storeDirToEEPROM(none);
      freq = -1;  // no signal
      url = "";
      JSONline = "";

      // exit
      _dir = readButton();
      if (_dir == left) status = requestLeft;
      if (_dir == right) status = requestRight;
      lastStatus = idle;
      break;



    case requestLeft:
      if (lastStatus != requestLeft) Serial.println("Status requestLeft");
      storeDirToEEPROM(left);
      url = "/v1/connections?from=" + config.base + "&to=" + config.left  + "&fields[]=connections/from/departure&fields[]=connections/from/prognosis/departure&fields[]=connections/from/departureTimestamp&limit=" + MAX_CONNECTIONS;
      if (lastStatus != requestLeft) storeDepartureString(); // if valid url
      if (JSONline.length() > 1) {
        processRequest();

        // exit
        if (lastDepartureTimeStamp != departureTimeStamp && (lastStatus == requestRight || lastStatus == requestLeft)) status = idle; // next departure time choosen
        //  Serial.printf("lastDepartureTimeStamp %d departureTimeStamp %d lastStatus %d \n", lastDepartureTimeStamp , departureTimeStamp, lastStatus);
        lastDepartureTimeStamp = departureTimeStamp;
        lastStatus = requestLeft;
      }
      _dir = readButton();
      if (_dir == right) {  //change direction
        Serial.println("Change to right");
        _dir = right;
        status = requestRight;
        lastStatus = requestLeft;
      }
      break;



    case requestRight:
      if (lastStatus != requestRight) Serial.println("Status requestRight");
      storeDirToEEPROM(right);
      url = "/v1/connections?from=" + config.base + "&to=" + config.right  + "&fields[]=connections/from/departure&fields[]=connections/from/prognosis/departure&fields[]=connections/from/departureTimestamp&limit=" + MAX_CONNECTIONS;
      if (lastStatus != requestRight) storeDepartureString(); // if valid url
      if (JSONline.length() > 1) {
        processRequest();

        // exit
        if (lastDepartureTimeStamp != departureTimeStamp && (lastStatus == requestRight || lastStatus == requestRight)) status = idle; // next departure time choosen
        //  Serial.printf("lastDepartureTimeStamp %d departureTimeStamp %d lastStatus %d \n", lastDepartureTimeStamp , departureTimeStamp, lastStatus);
        lastDepartureTimeStamp = departureTimeStamp;
        lastStatus = requestRight;
      }
      _dir = readButton();
      if (_dir == left) {   //change direction
        _dir = left;
        Serial.println("Change to left");
        status = requestLeft;
        lastStatus = requestRight;
      }
      break;


    case recovery:
      Serial.println("------------ Recovery --------------");
      Serial.println("");
      WiFi.mode(WIFI_AP);
      ConfigureWifi();
      ledColor = off;
      Serial.println(currentDirection);

      // exit
      switch (currentDirection) {
        case left:
          status = requestLeft;
          lastStatus = recovery;
          Serial.println("Recovery left");
          break;

        case right:
          status = requestRight;
          lastStatus = recovery;
          Serial.println("Recovery right");
          break;

        default:
          status = idle;
          lastStatus = recovery;
          break;
      }
      cNTP_Update = 999; // trigger NTP immediately
      minTillDep = -999;
      break;

    default:
      break;
  }

  // store NTP time
  if ( cNTP_Update > (config.Update_Time_Via_NTP_Every * 60 )) {
    storeNTPtime();
    if (DateTime.year > 1970) cNTP_Update = 0;  // trigger loop till date is valid
  }

  // store departure time String from openTransport
  if (millis() - waitJSONLoopEntry > loopTime) {
    if (minTillDep > 1 || minTillDep < 0) { // no updates in the last minute
      if (url.length() > 1) storeDepartureString();  // if valid url
      if (JSONline != "") waitJSONLoopEntry = millis();
    }
  }

  // Display LED
  if (millis() - ledCounter > 1000 ) {
    ledCounter = millis();
    ledState = !ledState;
  }

  if (ledState)  led(ledColor);
  else led(off);

  // send Signal (Beep)
  if (freq < 0) setSignal(0, 0); // off
  else setSignal(1, freq);

  if (_lastStatus != status || millis() - waitLoopEntry > 10000) {
    displayStatus();
    waitLoopEntry = millis();
    _lastStatus = status;
  }
  customWatchdog = millis();
}


//------------------------- END LOOP -------------------------------------


void processRequest() {
  long _diffSec, _diffMin;

  int   _positionDeparture = 1;
  do {
    decodeDepartureTime(_positionDeparture);
    if (departureTime != -999) {  // valid time
      _diffSec = departureTime - actualTime;
      if (_diffSec < -10000) _diffSec += 24 * 3600;  // correct if time is before midnight and departure is after midnight
      _diffMin = (_diffSec / 60) - config.wayToStation;
    } else _diffMin = -999;
    _positionDeparture++;
  } while (_diffMin < 0 && _positionDeparture <= MAX_CONNECTIONS + 1);  // next departure if first not reachable

  minTillDep = (_positionDeparture <= MAX_CONNECTIONS) ? _diffMin : -999; // no connection found

  if (minTillDep != -999) {  // valid result
    freq = (minTillDep >= 0 && minTillDep < 10) ? intensity[minTillDep] : freq = -1; //set frequency if minTillDep between 10 and zero minutes
    loopTime = getLoopTime(minTillDep);
  }
}


boolean getStatus() {
  bool stat;
  String _line;

  _line = client.readStringUntil('\n');
  // Serial.print(" statusline ");
  // Serial.println(line);

  int separatorPosition = _line.indexOf("HTTP/1.1");

  // Serial.print(" separatorPosition ");
  // Serial.println(separatorPosition);
  //  Serial.print("Line ");
  // Serial.print(line);
  if (separatorPosition >= 0) {

    if (_line.substring(9, 12) == "200") stat = true;
    else stat = false;
    //   Serial.print("Status ");
    //   Serial.println(stat);
    return  stat;
  }
}


void storeDepartureString() {
  bool ok = false;
  String _line;
  unsigned long serviceTime = millis();

  ledColor = red;

  url.replace(" ", "%20");

  if (!client.connect("transport.opendata.ch", 80)) {
    Serial.println("connection to ... failed");

  } else {
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host:" + serverTransport + "\r\n" + "Connection: keep-alive\r\n\r\n");
    // Wait for answer of webservice
    Serial.println(url.substring(1, url.indexOf("&fields")));
    while (!client.available()) {
      // Serial.println("waiting");
    }
    Serial.printf("Client connect. Service time %d \n",  millis() - serviceTime);
    delay(200);
  }
  // Service answered
  ok = getStatus();
  Serial.printf("Got Status. Service time %d \n",  millis() - serviceTime);

  if (ok) {  // JSON packet is avablable
    while (client.available()) {
      yield();
      ledColor = green;
      _line = client.readStringUntil('\n');
      //  Serial.println(_line);

      if (_line.indexOf("connections") > 1) {
        JSONline = _line; // JSON string detected
        Serial.printf("JSONline stored. Service time %d \n", millis() - serviceTime);
      }
    }
  } else  Serial.println("-- No data from Service --");
}






int findJSONkeyword(String keyword0, String keyword1, String keyword2, int pos ) {
  int hi = pos, i;
  String keyword[3];

  keyword[0] = keyword0;
  keyword[1] = keyword1;
  keyword[2] = keyword2;
  i = 0;
  while (keyword[i] != "" && i < 3) {
    hi = JSONline.indexOf(keyword[i], hi + 1);

    i++;
  }
  if (hi > JSONline.length()) hi = 0;
  return hi;
}





void decodeDepartureTime(int pos) {
  int hour;
  int minute;
  int second;
  int i = 0;
  long h1, h2, hh;
  int separatorPosition = 1;
  String keyword[3];

  while (i < pos) {
    separatorPosition = JSONline.indexOf("from", separatorPosition + 1);
    i++;
  }
  // separatorPosition stands at the line requested by calling function
  for (int i = 0; i < 3; i++) keyword[i] = "";
  hh = findJSONkeyword("departure", "", "", separatorPosition);
  h1 = parseJSONDate(hh);

  // Serial.println(JSONline);

  hh = findJSONkeyword("prognosis", "departure", "" , separatorPosition);
  h2 = parseJSONDate(hh);

  hh = findJSONkeyword("departureTimestamp", "", "" , separatorPosition);  // find unique identifier of connection
  departureTimeStamp = parseJSONnumber(hh);
  departureTime = ( h2 > 0) ? h2 : h1;
}


int getTimeStamp(int pos) {

  int hh = findJSONkeyword("departureTimestamp", "", "", pos );
  return JSONline.substring(pos, pos + 4).toInt();
}





long parseJSONDate(int pos) {
  int hi;
  pos = pos + 11;  // adjust for beginning of text
  if (JSONline.substring(pos, pos + 4) != "null" ) {
    pos = pos + 12; // overread date;

    int hour = JSONline.substring(pos, pos + 2).toInt();
    int minute = JSONline.substring(pos + 3, pos + 5).toInt();
    int second = JSONline.substring(pos + 6, pos + 8).toInt();

    // ----------------------- Spieldaten ------------------------------
    // hour = 10;
    //  minute = 28;
    //  second = 0;

    // ----------------------- Spieldaten ------------------------------

    hi = second + 60 * minute + 3600 * hour;
  } else hi = -999;
  return hi;
}



int parseJSONnumber(int pos) {
  pos = pos + 20;
  return JSONline.substring(pos, pos + 10).toInt();
}



defDirection readButton() {
  defDirection dir = none;
  if (!digitalRead(LEFTPIN)) dir = left;
  if (!digitalRead(RIGHTPIN)) dir = right;

  if (dir != none) beep(3);
  return dir;
}


void beep(int _dura) {
  beepOnTime = _dura;
  beepOffTime = 2;
  delay(BEEPTICKER + 10); // wait for next beepTicker
  while (beeperStatus != beeperIdle) yield();
  beepOnTime = 0;

}

void setSignal (int _onTime, int _offTime) {
  if (beeperStatus == beeperIdle) {
    beepOnTime = _onTime;
    beepOffTime = _offTime;
  }
}

// define loop time based on time till departure
int getLoopTime(int _timeTillDeparture) {

  int _loopTime = LOOP_FAST;
  if (_timeTillDeparture > 5) _loopTime = LOOP_SLOW;
  if (_timeTillDeparture == -999) _loopTime = 0;  // no valid info, immediate update required
  return _loopTime;
}

void storeDirToEEPROM(defDirection dir) {

  if (EEPROM.read(300) != dir) {
    Serial.printf("EEPROM direction before %d and after %d \n", EEPROM.read(300), dir);
    Serial.println(dir);
    EEPROM.write(300, dir);
    EEPROM.commit();
  }
}


void printTime(String purpose, long _tim) {

  int hours = _tim / 3600;
  int res = _tim - hours * 3600;
  int minutes = res / 60;
  res = res - (minutes * 60);
  int seconds = res;
  Serial.print(" ");
  Serial.print(purpose);
  Serial.print(" ");
  Serial.print(hours);
  Serial.print(" H ");
  Serial.print(minutes);
  Serial.print(" M ");
  Serial.print(seconds);
}

void displayStatus() {
  printTime("Tim", actualTime);
  printTime("Dep", departureTime);
  Serial.print(" Status ");
  Serial.print(status);
  Serial.print(" lastStatus ");
  Serial.print(lastStatus);
  Serial.print(" minTillDep ");
  Serial.print(minTillDep);
  Serial.print(" loopTime ");
  Serial.print(loopTime);
  Serial.print(" freq ");
  Serial.println(freq);
}


void ISRbeepTicker(void *pArg) {

  switch (beeperStatus) {
    case beeperIdle:
      beepOnTimer  = beepOnTime;
      beepOffTimer = beepOffTime;

      // exit
      if (beepOnTime > 0) beeperStatus = beeperOn;
      break;

    case beeperOff:
      digitalWrite(BEEPPIN, LOW); // always off
      beepOffTimer--;
      // exit
      if (beepOffTimer <= 0) {
        beeperStatus = beeperIdle;
      }
      break;

    case beeperOn:
      if (beepOffTimer > 0) beepOnTimer--;
      digitalWrite(BEEPPIN, HIGH);

      // exit
      if (beepOnTimer <= 0) {
        beeperStatus = beeperOff;
      }
      break;

    default:
      break;
  }
}



//------------------- OTA ---------------------------------------
void otaInit() {

  led(red);

  for (int i = 0; i < 3; i++) beep(3);
  WiFi.mode(WIFI_AP);
  ConfigureWifi();
  MDNS.begin(host);
  MDNS.addService("arduino", "tcp", aport);
  OTA.begin(aport);
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);
  Serial.print("IP address: ");
  led(green);
  Serial.println(WiFi.localIP());
  Serial.println("OTA settings applied");
}

void otaReceive() {
  if (OTA.parsePacket()) {
    IPAddress remote = OTA.remoteIP();
    int cmd  = OTA.parseInt();
    int port = OTA.parseInt();
    int size   = OTA.parseInt();

    Serial.print("Update Start: ip:");
    Serial.print(remote);
    Serial.printf(", port:%d, size:%d\n", port, size);
    uint32_t startTime = millis();

    WiFiUDP::stopAll();

    if (!Update.begin(size)) {
      Serial.println("Update Begin Error");
      return;
    }

    WiFiClient client;
    if (client.connect(remote, port)) {

      uint32_t written;
      while (!Update.isFinished()) {
        written = Update.write(client);
        if (written > 0) client.print(written, DEC);
      }
      Serial.setDebugOutput(false);

      if (Update.end()) {
        client.println("OK");
        Serial.printf("Update Success: %u\nRebooting...\n", millis() - startTime);
        ESP.restart();
      } else {
        Update.printError(client);
        Update.printError(Serial);
      }
    } else {
      Serial.printf("Connect Failed: %u\n", millis() - startTime);
    }
  }
  //IDE Monitor (connected to Serial)
  if (TelnetServer.hasClient()) {
    if (!Telnet || !Telnet.connected()) {
      if (Telnet) Telnet.stop();
      Telnet = TelnetServer.available();
    } else {
      WiFiClient toKill = TelnetServer.available();
      toKill.stop();
    }
  }
  if (Telnet && Telnet.connected() && Telnet.available()) {
    while (Telnet.available())
      Serial.write(Telnet.read());
  }
  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t * sbuf = (uint8_t *)malloc(len);
    Serial.readBytes(sbuf, len);
    if (Telnet && Telnet.connected()) {
      Telnet.write((uint8_t *)sbuf, len);
      yield();
    }
    free(sbuf);
  }
}


