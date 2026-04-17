#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIFI_SSID ""
#define WIFI_PASS ""
#define HOST "broker.hivemq.com"
#define TOPIC "phantasm/iot/display"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
AsyncMqttClient mqtt;

bool isIdle = false;
unsigned long lastFrame = 0;
int frameCount = 0;

void connectWifi() { WiFi.begin(WIFI_SSID, WIFI_PASS); }
void connectMqtt() { mqtt.connect(); }

void onConnect(bool session) {
  mqtt.subscribe(TOPIC, 1);
  isIdle = true;
}

void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties prop, size_t len, size_t idx, size_t total) {
  isIdle = false; // Stop the idle animation
  display.clearDisplay();

  // Mode 1: Visual Cryptography Share (1024 bytes for 128x64 bitmap)
  if (total == 1024) {
    display.drawBitmap(0, 0, (const unsigned char*)payload, 128, 64, WHITE);
  } 
  // Mode 2: PIN/Challenge Code (Small text payloads)
  else if (total > 0 && total <= 10) {
    // Create a null-terminated string for the PIN to prevent memory artifacts
    char message[len + 1];
    memcpy(message, payload, len);
    message[len] = '\0';

    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("SECURE PIN:");
    
    display.setTextSize(4);
    // Center the 4-digit PIN roughly in the middle of the 128x64 screen
    display.setCursor(15, 25); 
    display.print(message);
  }
  
  display.display();
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.println("Connecting...");
  display.display();

WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP) connectMqtt();
});

  mqtt.onConnect(onConnect);
  mqtt.onMessage(onMessage);
  mqtt.setServer(HOST, 1883);

  connectWifi();
}

void loop() {
  if (isIdle && millis() - lastFrame > 60) {
    lastFrame = millis();
    display.clearDisplay();

    // CipherSight Title
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(30, 8);
    display.print("CipherSight");

    // Draw stylized "Cipher Eye"
    int centerX = 64;
    int centerY = 32;
    
    // Outer eye shape (simplified with arcs/lines)
    display.drawLine(centerX - 15, centerY, centerX, centerY - 8, WHITE);
    display.drawLine(centerX, centerY - 8, centerX + 15, centerY, WHITE);
    display.drawLine(centerX - 15, centerY, centerX, centerY + 8, WHITE);
    display.drawLine(centerX, centerY + 8, centerX + 15, centerY, WHITE);
    
    // Scanning Pupil
    int pupilX = centerX + (sin(frameCount * 0.15) * 8);
    display.drawCircle(pupilX, centerY, 3, WHITE);
    
    // Animated dots for "Awaiting Signal..."
    display.setCursor(22, 50);
    display.print("Awaiting Signal");
    int dots = (frameCount / 5) % 4;
    for(int i = 0; i < dots; i++) display.print(".");

    display.display();
    frameCount++;
  }
}
