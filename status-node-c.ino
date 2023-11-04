/*
  Von Alexey und Michael(Pluto)
  Dieser Code kann mit NodeMCU 1.0 Kompiliert werden in der Arduino IDE 2.0.3

  Zeigt den Space Status im Fenster in Konferenz Raum an.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include "cert.h"
#include "str.h"

#define LED_BLUE  D2
#define LED_GREEN D6
#define LED_RED D5

const char *const SSID0 = "mainframe.iot";
const char *const SSID0_PASSWORD = "TODO";

const unsigned long MAX_NOT_CONNECTED_TIME = 20*1000; // milliseconds

const char *const TIMEZONE = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // Europe/Berlin

const char *const MQTT_HOST = "spacegate.mainframe.io"; // "mainframe.io";
const int MQTT_PORT = 8884;
const char* const STATUS_TOPIC = "/access-control-system/space-state";
const char* const STATUS_NEXT_TOPIC = "/access-control-system/space-state-next";

ESP8266WiFiMulti wifi;
WiFiClientSecure secure;
const X509List caCertX509(CA_CERT);        /* X.509 parsed CA Cert */
PubSubClient mqtt_client(MQTT_HOST, MQTT_PORT, secure);

const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* const NTP_SERVER_1 = "ntp.lan.mainframe.io";
const char* const NTP_SERVER_2 = "de.pool.ntp.org";

enum class Color {
  Off = 0,
  Red = 4,
  Green = 2,
  Blue = 1,
  Yellow = 6,
  Cyan = 3,
  Magenta = 5,
  White = 7,
};

enum class SpaceStatus {
  Unknown,
  Closed,
  Keyholder,
  Member,
  Open,
  OpenPlus,
};

static void set_color(Color color) {
  digitalWrite(LED_RED, ((unsigned)color & 4) ? HIGH : LOW);
  digitalWrite(LED_GREEN, ((unsigned)color & 2) ? HIGH : LOW);
  digitalWrite(LED_BLUE, ((unsigned)color & 1) ? HIGH : LOW);
}

static SpaceStatus from_string(const char* const status, const unsigned int length) {
    Str s(status, length);
    if (s == "open") {
      return SpaceStatus::Open;
    } else if (s == "open+") {
      return SpaceStatus::OpenPlus;
    } else if (s == "member") {
      return SpaceStatus::Member;
    } else if (s == "keyholder") {
      return SpaceStatus::Keyholder;
    } else if (s == "none") {
      return SpaceStatus::Closed;
    } else {
      return SpaceStatus::Unknown;
    }
}

SpaceStatus current_status = SpaceStatus::Unknown;
SpaceStatus next_status = SpaceStatus::Unknown;

// callback for subscribed topics
static void mqtt_callback(const char* const topic, const byte* const payload, const unsigned int length) {
  const char* const status = (const char*) payload;

  Serial.printf("topic='%s' payload='%s' length=%u\n", topic, payload, length);
  if (!strcmp(topic, STATUS_TOPIC)) {
    current_status = from_string(status, length);
  } else if (!strcmp(topic, STATUS_NEXT_TOPIC)) {
    next_status = from_string(status, length);
  }
} // callback

void setup() {
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Self-test
  set_color(Color::Red);
  delay(250);
  set_color(Color::Green);
  delay(250);
  set_color(Color::Blue);
  delay(250);

  Serial.begin(115200);
  {
    const auto mac = WiFi.macAddress();
    Serial.printf("\n\n");
    Serial.printf("MAC address: %s\n", mac.c_str());
  }

  mqtt_client.setCallback(mqtt_callback);
  secure.setTrustAnchors(&caCertX509);
  wifi.addAP(SSID0, SSID0_PASSWORD);
  mqtt_client.setServer(MQTT_HOST, MQTT_PORT);  
}

int32_t not_connected_since = 0;
bool ntp_connected = false;
int hour = -1;
unsigned long last_cycle = 0;
bool active = false;

void loop() {
  if (wifi.run() == WL_CONNECTED) {
    if (not_connected_since >= 0) {
      Serial.println("Wi-Fi connection succeeded");
    }
    not_connected_since = -1;
  } else {
    ntp_connected = false;
    unsigned long ts = millis();
    if (not_connected_since < 0) {
      not_connected_since = ts;
      Serial.println("No Wi-Fi connection");
    } else {
      if (ts - (unsigned long)not_connected_since > MAX_NOT_CONNECTED_TIME) {
        not_connected_since = ts - MAX_NOT_CONNECTED_TIME;
        current_status = SpaceStatus::Unknown;
      }
    }
  }

  if (not_connected_since < 0 && !ntp_connected) {
    Serial.println("Wi-Fi connection established, connecting to NTP server");
    configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
    ntp_connected = true;
  }

  if (not_connected_since < 0 && !mqtt_client.loop()) {
    active = false;
    if (mqtt_client.connect("status-node-c")) {
      Serial.println("Connected to MQTT server");
      set_color(Color::Magenta);
      mqtt_client.subscribe(STATUS_TOPIC);
      mqtt_client.subscribe(STATUS_NEXT_TOPIC);
    } else {
      int status = mqtt_client.state();
      Serial.printf("MQTT connection failed, status = %i\n", status);
      delay(1000);
    }
  } else {
    active = true;
  }

  if (ntp_connected) {
    unsigned long t = millis();
    if (t - last_cycle > 5000) {
      last_cycle = t;
      time_t now;
      time(&now);
      struct tm current_time;
      localtime_r(&now, &current_time);
      if (current_time.tm_year > (2020 - 1900)) {
        hour = current_time.tm_hour;
        Serial.printf("NTP time = %02u:%02u\n", hour, current_time.tm_min);
      } else {
        Serial.println("No NTP time");
      }
    }
  }

  if (active) {
    if (hour >= 7 && hour <= 16) {
      set_color(Color::Off);
    } else {
      switch (current_status) {
        case SpaceStatus::Open:
        case SpaceStatus::OpenPlus:
          switch (next_status) {
            case SpaceStatus::Closed:
            case SpaceStatus::Keyholder:
              set_color(Color::Yellow);
              break;
            default:
              set_color(Color::Green);
              break;
          }
          break;
        case SpaceStatus::Member:
          set_color(Color::Cyan);
          break;
        case SpaceStatus::Keyholder:
          set_color(Color::Magenta);
          break;
        case SpaceStatus::Closed:
          switch (next_status) {
            case SpaceStatus::Open:
            case SpaceStatus::OpenPlus:
              set_color(Color::Cyan);
              break;
            default:
              set_color(Color::Red);
              break;
          }
          break;
        case SpaceStatus::Unknown:
          set_color(Color::Off);
          break;
      }
    }
  }
}
