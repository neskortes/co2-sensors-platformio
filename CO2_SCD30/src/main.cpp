#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SCD30.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "secrets.h" // WIFI_SSID, WIFI_PASSWORD, THINGSPEAK_WRITE_KEY

// --- I2C pins for ESP32 ---
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;

// --- ThingSpeak ---
static const char* TS_URL = "http://api.thingspeak.com/update";
static const unsigned long TS_INTERVAL_MS = 20000; // ThingSpeak: min ~15s

// --- WiFi retry ---
static const unsigned long WIFI_RETRY_MS = 10000;

// --- Globals ---
Adafruit_SCD30 scd30;

unsigned long lastWifiTry = 0;
unsigned long lastTsSend  = 0;

const char* wlStatusName(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

#include <esp_system.h>

void printResetReason() {
  esp_reset_reason_t r = esp_reset_reason();
  Serial.print("Reset reason: ");
  switch (r) {
    case ESP_RST_POWERON:  Serial.println("POWERON"); break;
    case ESP_RST_BROWNOUT: Serial.println("BROWNOUT"); break;
    case ESP_RST_SW:       Serial.println("SW"); break;
    case ESP_RST_PANIC:    Serial.println("PANIC"); break;
    case ESP_RST_WDT:      Serial.println("WDT"); break;
    default:               Serial.println((int)r); break;
  }
}


void wifiConnectDebug() {
  Serial.println("\n[WiFi] Starting...");

  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  delay(200);

  Serial.print("[WiFi] SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  wl_status_t last = WL_IDLE_STATUS;

  while (millis() - start < 20000 && WiFi.status() != WL_CONNECTED) {
    wl_status_t s = WiFi.status();
    if (s != last) {
      Serial.print("[WiFi] status: ");
      Serial.print(wlStatusName(s));
      Serial.print(" (");
      Serial.print((int)s);
      Serial.println(")");
      last = s;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  wl_status_t finalS = WiFi.status();
  Serial.print("[WiFi] Final: ");
  Serial.print(wlStatusName(finalS));
  Serial.print(" (");
  Serial.print((int)finalS);
  Serial.println(")");

  if (finalS == WL_CONNECTED) {
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("[WiFi] Will retry later.");
  }
}

bool sendToThingSpeak(float co2ppm, float tempC, float rhPct) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;

  // field1=CO2, field2=Temp, field3=RH
  String url = String(TS_URL) +
               "?api_key=" + THINGSPEAK_WRITE_KEY +
               "&field1=" + String(co2ppm, 1) +
               "&field2=" + String(tempC, 2) +
               "&field3=" + String(rhPct, 1);

  http.begin(url);
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();

  if (httpCode == 200 && payload.toInt() > 0) {
    Serial.print("[TS] OK entry: ");
    Serial.println(payload);
    return true;
  } else {
    Serial.print("[TS] FAIL HTTP=");
    Serial.print(httpCode);
    Serial.print(" body=");
    Serial.println(payload);
    return false;
  }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout (test only)

  Serial.begin(115200);
  delay(1500);
  printResetReason();

  Serial.println("\nBOOT: SCD30 + WiFi + ThingSpeak");

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  // SCD30
  if (!scd30.begin()) {
    Serial.println("ERROR: SCD30 not detected. Check wiring (SDA=21, SCL=22).");
    while (true) delay(1000);
  }

  scd30.setMeasurementInterval(2);
  scd30.selfCalibrationEnabled(true);

  // First WiFi try
  wifiConnectDebug();
  lastWifiTry = millis();
}

void loop() {
  // --- WiFi keepalive with retries (doesn't hammer continuously) ---
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastWifiTry >= WIFI_RETRY_MS) {
      lastWifiTry = now;
      wifiConnectDebug();
    }
    delay(50);
    return;
  }

  // --- Read SCD30 ---
  if (scd30.dataReady()) {
    if (!scd30.read()) {
      Serial.println("Read error from SCD30.");
      delay(200);
      return;
    }

    float co2 = scd30.CO2;
    float t   = scd30.temperature;
    float rh  = scd30.relative_humidity;

    Serial.print("CO2: ");
    Serial.print(co2, 1);
    Serial.print(" ppm | T: ");
    Serial.print(t, 2);
    Serial.print(" C | RH: ");
    Serial.print(rh, 1);
    Serial.println(" %");

    // --- Send to ThingSpeak every TS_INTERVAL_MS ---
    unsigned long now = millis();
    if (now - lastTsSend >= TS_INTERVAL_MS) {
      lastTsSend = now;
      sendToThingSpeak(co2, t, rh);
    }
  }

  delay(200);
}
