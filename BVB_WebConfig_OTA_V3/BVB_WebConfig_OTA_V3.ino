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

#include "helpers.h"
#include "global.h"

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
  delay(1000);
  Serial.println("");
  Serial.println("Starting ES8266");

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
    config.ssid = "WLAN";
    config.password = "juLi136f";
    config.dhcp = true;
    config.IP[0] = 192; config.IP[1] = 168; config.IP[2] = 1; config.IP[3] = 100;
    config.Netmask[0] = 255; config.Netmask[1] = 255; config.Netmask[2] = 255; config.Netmask[3] = 0;
    config.Gateway[0] = 192; config.Gateway[1] = 168; config.Gateway[2] = 1; config.Gateway[3] = 1;
    config.ntpServerName = "0.ch.pool.ntp.org";
    config.Update_Time_Via_NTP_Every =  5;
    config.timezone = 1;
    config.daylight = true;
    config.DeviceName = "Not Named";
    config.wayToStation = 3;
    config.warningBegin = 5;
    config.base = "lausen stutz";
    config.right = "lausen";
    config.left = "lausen";
    WriteConfig();
  }
  if (!(digitalRead(LEFTPIN) && digitalRead(RIGHTPIN))) {   // OTA Mode?
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

    currentDirection = EEPROM.read(301);
    if (currentDirection != 0) {

      // Recovery
      Serial.println("Recovery");
      WiFi.mode(WIFI_AP);
      ledColor = off;

      switch (currentDirection) {
        case 1:
          status = requestLeft;
          Serial.println("Recovery left");
          break;

        case 2:
          status = requestRight;
          Serial.println("Recovery right");
          break;

        default:
          status = doNothing;
          break;
      }
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
      tkSecond.attach(1, Second_Tick);

      AdminTimeOutCounter = 0;
      entryLoop = millis();
      lastStatus = doNothing;
    }
  }
}

void loop(void ) {
  yield(); // For ESP8266 to not dump

  if (otaFlag) {
    otaReceive();
  }
  else {
    if ( cNTP_Update > (config.Update_Time_Via_NTP_Every * 60) )
    {
      okNTPvalue = NTPRefresh();
      cNTP_Update = 0;
    }
    customLoop();
  }
}

//-------------------------------------- CUSTOM ----------------------------------------

void customLoop() {
  int _minTillDep;
  defDirection _dir;

  // Non blocking code !!!
  switch (status) {
    case admin:
      ledColor = both;
      server.handleClient();
      // exit
      if (AdminTimeOutCounter > AdminTimeOut) status = waitForNTP;
      break;

    case waitForNTP:
      Serial.println("Admin Mode disabled!");
      WiFi.mode(WIFI_AP);
      ledColor = red;
      for (int hi = 0; hi < 3; hi++) beep(2);
      ConfigureWifi();
      if (NTPRefresh()) status = doNothing; // NTP available, goto doNothing
      break;

    case doNothing:
      ledColor = off;
      storeDirToEEPROM(none);
      freq = -1;  // no signal

      // exit
      _dir = readButton();
      if (_dir == left) status = requestLeft;
      if (_dir == right) status = requestRight;
      break;

    case requestLeft:
      url =  "/v1/connections?from=" + config.base + "&to=" + config.left + "&fields[]=connections/from/departure&limit=1";
      // Serial.println(url);
      storeDirToEEPROM(left);
      _minTillDep = getData(url);
      if (_minTillDep > -999) {  // valid answer
        ledColor = green;
        minTillDep = _minTillDep  - config.wayToStation;
        if (_minTillDep < 0 ) status = doNothing;
        else {
          entryLoop = millis();
          status = waitLeft;
        }
      } else ledColor = red;
      break;

    case requestRight:
      url =  "/v1/connections?from=" + config.base + "&to=" + config.right + "&fields[]=connections/from/departure&limit=1";
      //Serial.println(url);
      storeDirToEEPROM(right);
      _minTillDep = getData(url);
      if (_minTillDep > -999) {
        ledColor = green;
        minTillDep = _minTillDep - config.wayToStation;
        if (_minTillDep < 0 ) status = doNothing;
        else {
          entryLoop = millis();
          status = waitRight;
        }
      } else ledColor = red;
      break;

    case waitLeft:
      freq = getWarningFrequency(minTillDep);
      loopTime = getLoopTime(minTillDep);
      if (millis() - ledCounter > 1000 ) {
        ledCounter = 0;
        ledState = !ledState;
      }
      // Exit
      if (millis() - entryLoop >= loopTime) status = requestLeft;
      if (readButton() == right) status = requestRight;
      break;

    case waitRight:
      //  Serial.println("Wait");
      freq = getWarningFrequency(minTillDep);
      loopTime = getLoopTime(minTillDep);

      if (millis() - ledCounter > 1000 ) {
        ledCounter = 0;
        ledState = !ledState;
      }

      // Exit
      if (readButton() == left) status = requestLeft;
      if (millis() - entryLoop >= loopTime) status = requestRight;
      break;

    default:
      break;
  }

  // Display LED
  if (ledState)  led(ledColor);
  else led(off);

  // send Signal (Beep)
  freq = getWarningFrequency(minTillDep);
  if (freq < 0) setSignal(0, 0); // off
  else setSignal(1, freq);

  if (lastStatus != status) displayStatus();
  lastStatus = status;
}


//------------------------- END LOOP -------------------------------------

int getData(String url) {
  int diffSec = 0;
  int diffMin = -999;
  int tim = 0;
  int dep = 0;
  bool ok = false;
  url.replace(" ", "%20");

  yield();


  tim = getTime();

  if (!client.connect("transport.opendata.ch", 80)) {
    Serial.println("connection to ... failed");
  } else {
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host:" + serverTransport + "\r\n" + "Connection: keep-alive\r\n\r\n");
    // Wait for answer of webservice
    // Serial.println(url);
    while (!client.available()) {
      // Serial.println("waiting");
      delay(200);
    }

    // Service answered
    ok = getStatus();
    // Serial.print("HTTP Status ");
    //Serial.println(ok);

    if (ok) {
      while (client.available()) {
        yield();
        line = client.readStringUntil('\n');
        // Serial.println(line);
        if (dep == 0) dep = getDeparture("departure", 23);
      }
      //  Serial.println(tim);
      //  Serial.println(dep);
      diffSec = dep - tim;

      if (diffSec < -10000) diffSec += 24 * 3600;  // correct if time is before midnight and departure is after midnight
      diffMin = diffSec / 60;
    } else {
      //   Serial.println("-- No data from Service --");
      diffMin = -999;
    }
  }
  Serial.print("diffMin ");
  Serial.println(diffMin);
  return diffMin ;
}

boolean getStatus() {
  bool stat;

  line = client.readStringUntil('\n');
  // Serial.print(" statusline ");
  // Serial.println(line);

  int separatorPosition = line.indexOf("HTTP/1.1");

  // Serial.print(" separatorPosition ");
  // Serial.println(separatorPosition);
  //  Serial.print("Line ");
  // Serial.print(line);
  if (separatorPosition >= 0) {

    if (line.substring(9, 12) == "200") stat = true;
    else stat = false;
    //   Serial.print("Status ");
    //   Serial.println(stat);
    return  stat;
  }
}

long getTime() {


  Serial.print("Time      ");
  Serial.print(DateTime.hour);

  Serial.print(" H ");
  Serial.print(DateTime.minute);

  Serial.print(" M ");
  Serial.println(DateTime.second);


  return ((DateTime.hour * 3600) + (DateTime.minute * 60) + DateTime.second);

}

long getDeparture(String lookupString, int pos) {
  int hour;
  int minute;
  int second;

  int separatorPosition = line.indexOf(lookupString);

  // Serial.print(" separatorPosition ");
  // Serial.println(separatorPosition);
  //  Serial.print("Line ");
  //  Serial.print(line);
  if (separatorPosition >= 0) {

    int hourStart = separatorPosition + pos;
    int minuteStart = hourStart + 3;
    int secondsStart = minuteStart + 3;


    hour = line.substring(hourStart, hourStart + 2).toInt();
    minute = line.substring(minuteStart, minuteStart + 2).toInt();
    second = line.substring(secondsStart, secondsStart + 2).toInt();

    // ----------------------- Spieldaten ------------------------------
  //  hour = 17;
  //  minute = 28;
  //  second = 0;

    // ----------------------- Spieldaten ------------------------------

    Serial.print("Departure ");
    Serial.print(hour);

    Serial.print(" H ");
    Serial.print(minute);

    Serial.print(" M ");
    Serial.println(second);
  }
  else {
    hour = 0;
    minute = 0;
    second = 0;
  }
  //  Serial.print(" Dep ");
  //  Serial.print(second + 60 * minute + 3600 * hour);

  return  second + 60 * minute + 3600 * hour;
}


defDirection readButton() {
  defDirection dir = none;
  if (!digitalRead(LEFTPIN)) dir = left;
  if (!digitalRead(RIGHTPIN)) dir = right;

  if (dir != none) beep(3);
  return dir;
}


int getWarningFrequency(int _min ) {
  int _frequency = -1;
  if (_min > 0) _frequency = intensity[_min];
  return _frequency;
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
  if (_timeTillDeparture > 10) {
    _loopTime = LOOP_SLOW;
  } else if (_timeTillDeparture = 0) _loopTime = 0;
  return _loopTime;
}

void storeDirToEEPROM(defDirection dir) {

  if (EEPROM.read(300) != dir) {
    Serial.print("EEPROM direction ");
    Serial.println(dir);
    EEPROM.write(300, dir);
    EEPROM.commit();
  }
}

void displayStatus() {
  Serial.print(" Status ");
  Serial.print(status);
  Serial.print(" minTillDep ");
  Serial.print(minTillDep);
  Serial.print(" loopTime ");
  Serial.print(loopTime);
  Serial.print(" freq ");
  Serial.print(freq);
  Serial.print(" beepOntimer ");
  Serial.print(beepOnTimer);
  Serial.print(" beepOffTimer ");
  Serial.print(beepOffTimer);
  Serial.print(" time ");
  Serial.println(millis() / 1000);
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


