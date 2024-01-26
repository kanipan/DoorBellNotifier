#include <Arduino.h>
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#define MIC_Unit 36
#define MAX_LEN 320
#define X_OFFSET 0
#define Y_OFFSET 75
#define X_SCALE 1

#define LIGHTGRAY 0xC618
#define FONT_SIZE 10

#define THRESHOLD 2500
#define NOTIFY_PERIOD 120

#define SSID "<<SET SSID HERE>>"
#define PASSWORD "<<SET PASSWORD HERE>>"
#define DISCORD_MESSAGE_URL "<<SET DISCORD_MESSAGE_URL HERE>>"
#define DISCORD_TOKEN "Bot <<SET DISCORD_TOKEN HERE>>"

#define PING_INTERVAL 900000 // 15min
unsigned long gLastLoopAt = 0;
long int gPingTimer = PING_INTERVAL;

static RTC_NOINIT_ATTR int reg_b;

String http_status_code = "";

bool gFirstLoop = true;
bool gEnabled = true;
bool gLastState = false;

time_t lastDetectedTime = 0;
int waitingTime = 0;

void notify() {
  String url = DISCORD_MESSAGE_URL;
  HTTPClient http;
  if (http.begin(url)) {
    http.setConnectTimeout(5000);
    http.addHeader("Authorization", DISCORD_TOKEN);
    http.addHeader("content-type", "application/json");
    String body = "{\"content\":\"call from doorbell\"}";
    Serial.println("begin succeed");
    int response = http.POST(body);
    http_status_code = response;
    http.end();
  } else {
    http.end();
  }
}

int get_test() {
  String url = "https://www.google.com";
  HTTPClient http;
  if (http.begin(url)) {
    http.setConnectTimeout(5000);
    Serial.println("begin succeed");
    int response = http.GET();
    http.end();
    return response;
  } else {
    http.end();
    return 1;
  }
}

static boolean draw_waveform() {
  static int16_t val_buf[MAX_LEN] = {0};
  static int16_t pt = MAX_LEN - 1;
  int micValue = analogRead(MIC_Unit);
  String micValueDup = String(micValue);
  val_buf[pt] = map((int16_t)(micValue * X_SCALE), 1800, 4095,  0, 50);

  if (--pt < 0) {
    pt = MAX_LEN - 1;
  }

  bool state = (micValue >= THRESHOLD);
  time_t now = time(NULL);
  double diff = difftime(now, lastDetectedTime);
  waitingTime = NOTIFY_PERIOD - diff;
  if (lastDetectedTime == 0) {
    diff = NOTIFY_PERIOD + 1;
  }
  if (gLastState != state) {
    gLastState = state;
    if (state) {
      M5.Lcd.fillRect(X_OFFSET, Y_OFFSET - 50, X_OFFSET+MAX_LEN, Y_OFFSET + 50, RED);
      if (diff > NOTIFY_PERIOD){
        lastDetectedTime = now;
        notify();
      }
    } else {
      M5.Lcd.fillRect(X_OFFSET, Y_OFFSET - 50, X_OFFSET+MAX_LEN, Y_OFFSET + 50, BLACK);
    }
  }

  for (int i = 1; i < (MAX_LEN); i++) {
    uint16_t now_pt = (pt + i) % (MAX_LEN);
    M5.Lcd.drawLine(i + X_OFFSET, val_buf[(now_pt + 1) % MAX_LEN] + Y_OFFSET, i + 1 + X_OFFSET, val_buf[(now_pt + 2) % MAX_LEN] + Y_OFFSET, TFT_BLACK);
    if (i < MAX_LEN - 1) {
      M5.Lcd.drawLine(i + X_OFFSET, val_buf[now_pt] + Y_OFFSET, i + 1 + X_OFFSET, val_buf[(now_pt + 1) % MAX_LEN] + Y_OFFSET, TFT_GREEN);
    }
  }
  return state;
}

void ensureWifi() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(LIGHTGRAY);
  M5.Lcd.drawString("Ensuring WiFi...", 0, 30);
  if (WiFi.waitForConnectResult() == WL_DISCONNECTED) {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Failed! rebooting...", 0, 70);
    delay(500);
    esp_restart();
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);

  WiFi.begin(SSID, PASSWORD);
  ensureWifi();

  Serial.begin(115200);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextDatum(TC_DATUM);

  pinMode(MIC_Unit, INPUT);

  gLastLoopAt = millis();
}

void loop() {
  M5.update();

  unsigned long currentTime = millis();
  int deltaMs = currentTime - gLastLoopAt;
  gLastLoopAt = currentTime;

  // 一定期間ごとにpingとしてリクエストを送る
  gPingTimer -= deltaMs;
  if (gPingTimer < 0) {
    if (get_test() == 200) {
      http_status_code = 201;
      gPingTimer = PING_INTERVAL;
    } else {
      http_status_code = 500;
      esp_restart();
    }
  }

  if (M5.BtnA.wasReleased()){
    esp_restart();
  }

  M5.Lcd.fillRect(0,0,240,40,BLACK);
  M5.Lcd.setTextSize(4);
  M5.Lcd.drawString(http_status_code, 10, 10);

  M5.Lcd.fillRect(160,0,80,30,BLACK);
  if (waitingTime > 0){
    M5.Lcd.setTextSize(3);
    M5.Lcd.drawString((String)waitingTime, 200, 0);
  }

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.fillRect(0,FONT_SIZE * 14,240,FONT_SIZE * 15,BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.drawString(WiFi.localIP().toString(), 0, FONT_SIZE * 14);
  } else {
    char* message = "UNKNOWN";
    switch (WiFi.status()) {
      case WL_IDLE_STATUS:
        message = "IDLE_STATUS";
        break;
      case WL_NO_SSID_AVAIL:
        message = "NO_SSID_AVAIL";
        break;
      case WL_SCAN_COMPLETED:
        message = "SCAN_COMPLETED";
        break;
      case WL_CONNECTED:
        message = "CONNECTED";
        break;
      case WL_CONNECT_FAILED:
        message = "CONNECT_FAILED";
        break;
      case WL_CONNECTION_LOST:
        message = "CONNECTION_LOST";
        break;
      case WL_DISCONNECTED:
        message = "DISCONNECTED";
        break;
    }
    M5.Lcd.drawString(message, 0, FONT_SIZE * 14);
  }
  draw_waveform();
  gFirstLoop = false;
}
