#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIFI_SSID "Your_SSID"
#define WIFI_PASS "Your_Password"
#define HOST "broker.hivemq.com"
#define TOPIC "phantasm/iot/display"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
AsyncMqttClient mqtt;

void connectWifi() { WiFi.begin(WIFI_SSID, WIFI_PASS); }
void connectMqtt() { mqtt.connect(); }

void onConnect(bool session) {
  mqtt.subscribe(TOPIC, 1);
  display.fillRect(110, 0, 18, 10, BLACK);
  display.setCursor(110, 0);
  display.println("OK");
  display.display();
}

void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties prop, size_t len, size_t idx, size_t total) {
  if (total == 1024) {
    display.clearDisplay();
    display.drawBitmap(0, 0, (const unsigned char*)payload, 128, 64, WHITE);
    display.display();
  }
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.println("Connecting...");
  display.display();

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if(event == SYSTEM_EVENT_STA_GOT_IP) connectMqtt();
  });

  mqtt.onConnect(onConnect);
  mqtt.onMessage(onMessage);
  mqtt.setServer(HOST, 1883);

  connectWifi();
}

void loop() {}
