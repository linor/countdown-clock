#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0
#include "FastLED.h"
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>

const char* OTApass = "countdown";

#define SYNC_INTERVAL   1200
#define NUM_LEDS        42
#define DATA_PIN        4 // D2 Pin on Wemos mini

CRGB leds[NUM_LEDS];
CRGB segmentColor;

char currentChars[3];

IPAddress   timeServerIP; 
const char* ntpServerName   = "nl.pool.ntp.org";
const int   NTP_PACKET_SIZE = 48; 
byte        packetBuffer[ NTP_PACKET_SIZE];
WiFiUDP     Udp;

time_t  targetTime = 1508572800;

void setup() {
  Serial.begin(115200);
  Serial.println("Countdown clock ready");

  WiFiManager wifiManager;
  String ssid = "Countdown-" + String(ESP.getChipId());
  wifiManager.autoConnect(ssid.c_str());

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Udp.begin(8888);


  ArduinoOTA.setPassword(OTApass);
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
    ESP.restart();
  });
  ArduinoOTA.begin();
  Serial.println("Over the air update enabled.");

  setSyncProvider(getNtpTime);
  setSyncInterval(SYNC_INTERVAL);

  segmentColor = CRGB::Blue;

  FastLED.addLeds<NEOPIXEL,DATA_PIN>(leds,NUM_LEDS);
  FastLED.setBrightness(87);
  showLedStartSequence();

  displayCharacter('u', 2);

}

void showSegments(bool segments[7], int offset) {
  for(int i = 0; i < 7; i++) {
    int ledNum = (offset * 14) + (i * 2);
    if (segments[i]) {
      leds[ledNum] = segmentColor;
      leds[ledNum + 1] = segmentColor;
    } else {
      leds[ledNum] = CRGB::Black;
      leds[ledNum + 1] = CRGB::Black;
    }
  }  

  FastLED.show();
}

void displayCharacter(char ch, int offset) {
  if (currentChars[offset] == ch) return;
  currentChars[offset] = ch;
  
  bool ledChar1[7] = { false, false, true,  false, false, false, true };
  bool ledChar2[7] = { true,  true,  false, true,  false, true,  true };
  bool ledChar3[7] = { false, true,  true,  true,  false, true,  true };
  bool ledChar4[7] = { false, false, true,  true,  true,  false, true };
  bool ledChar5[7] = { false, true,  true,  true,  true,  true,  false };
  bool ledChar6[7] = { true,  true,  true,  true,  true,  true,  false };
  bool ledChar7[7] = { false, false, true,  false, false, true,  true  };
  bool ledChar8[7] = { true,  true,  true,  true,  true,  true,  true  };
  bool ledChar9[7] = { false, true,  true,  true,  true,  true,  true  };
  bool ledChar0[7] = { true,  true,  true,  false, true,  true,  true  };
  bool ledCharU[7] = { true,  true,  true,  false, false, false, false };
  bool ledsOff[7] = { false, false, false, false, false, false, false };
  
  showSegments(ledChar1, offset);

  switch(ch) {
    case '1': showSegments(ledChar1, offset); break;
    case '2': showSegments(ledChar2, offset); break;
    case '3': showSegments(ledChar3, offset); break;
    case '4': showSegments(ledChar4, offset); break;
    case '5': showSegments(ledChar5, offset); break;
    case '6': showSegments(ledChar6, offset); break;
    case '7': showSegments(ledChar7, offset); break;
    case '8': showSegments(ledChar8, offset); break;
    case '9': showSegments(ledChar9, offset); break;
    case '0': showSegments(ledChar0, offset); break;
    case 'u': showSegments(ledCharU, offset); break;
    default: showSegments(ledsOff, offset); break;
  }
}

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP server's ip address

    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            // New time in seconds since Jan 1, 1970
            unsigned long newTime = secsSince1900 - 2208988800UL;

            setSyncInterval(SYNC_INTERVAL);
            return newTime;
        }
    }
    Serial.println("No NTP Response :-(");
    // Retry soon
    setSyncInterval(10);
    return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

void showLedStartSequence() {
  for(int i=0;i<=NUM_LEDS;i=i+2) {
    if(i>1) {
      leds[i - 1] = CRGB::Black;
      leds[i - 2] = CRGB::Black;
    }
    if(i<NUM_LEDS) {
      leds[i] = segmentColor;
      leds[i+1] = segmentColor;
    }
    FastLED.show();
    delay(200);
  }
}

void loop() {
  char num[3];

  time_t current = now();
  time_t diff = targetTime - current;

  int hours_diff = diff / 3600;
  if (hours_diff < 24) {
    sprintf(num, "%2du", hours_diff);
  } else {
    int days = hours_diff / 24;
    sprintf(num, "%3d", days);
  }

  displayCharacter(num[0], 0);  
  displayCharacter(num[1], 1);  
  displayCharacter(num[2], 2);  
  delay(500);
}
