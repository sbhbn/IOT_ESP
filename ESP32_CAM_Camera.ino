#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
// ================= WiFi =================
const char* ssid = "55Y8817";
const char* password = "HN73108378";
WebServer server(80);

// ================= 雷射顯示 =================
volatile bool laserDetectedFlag = false;
unsigned long lastLaserTime = 0;
const unsigned long LASER_HOLD_MS = 3000; // 顯示 3 秒
const unsigned long HTTP_NOTIFY_INTERVAL = 3000; // 每 3 秒最多發一次通知
unsigned long lastNotifyTime = 0;

int laserHitCount = 0;
const int HIT_CONFIRM = 3; // 連續 3 幀才判定命中
int clearCount = 0;
const int CLEAR_CONFIRM = 5;  

// ============ AI Thinker Pins ============
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ============ Laser Detection Params ============
const int DIFF_THRESHOLD = 20; // diff 門檻，可依現場調整
int lastSum = 0;

// ================= Detect Laser =================
int computeDiff(camera_fb_t* fb) {
    int w = fb->width;
    int h = fb->height;
    int cx = w / 2;
    int cy = h / 2;

    int sum = 0;
    int maxVal = 0;

    for (int y = cy - 10; y <= cy + 10; y++) {
        for (int x = cx - 10; x <= cx + 10; x++) {
            int idx = y * w + x;
            uint8_t v = fb->buf[idx];
            sum += v;
            if (v > maxVal) maxVal = v;
        }
    }

    int diff = sum - lastSum;
    lastSum = sum;

    Serial.printf("sum=%d diff=%d max=%d\n", sum, diff, maxVal);
    return diff;
}
//==================Send To Thingboard============
void sendToThingsBoard(bool laser) {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure(); // ESP32 用 https 一定要

    HTTPClient http;

    String url = "https://thingsboard.cloud/api/v1/";
    url += "g3LNdQxUSyMvc9cNGECj";  
    url += "/telemetry";

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"laser\":";
    payload += laser ? "true" : "false";
    payload += ",";
    payload += "\"timestamp\":";
    payload += millis();
    payload += "}";

    int code = http.POST(payload);
    Serial.printf("ThingsBoard HTTP code: %d\n", code);

    if (code > 0) {
        Serial.println(http.getString());
    }

    http.end();
}


// ================= HTTP Notify =================
void sendHttpNotify() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - lastNotifyTime < HTTP_NOTIFY_INTERVAL) return;

    lastNotifyTime = millis();
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000);
    http.begin(client, "http://192.168.1.104:8000/laser");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"source\":\"esp32-cam\"}";
    int httpCode = -1;
    for (int i = 0; i < 3; i++) {
        httpCode = http.POST(payload);
        if (httpCode > 0) break;
        delay(200);
    }

    Serial.print("HTTP notify code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
        String response = http.getString();
        Serial.println("Response: " + response);
    }

    http.end();
}

// ================= Setup =================
void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println();

    // -------- WiFi --------
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // -------- Web Server --------
server.on("/", []() {
    String html;
    html.reserve(1600);

    html += "<!DOCTYPE html><html><head>";
    html += "<meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>感測整合頁面</title>";

    html += "<style>";
    html += "body{margin:0;min-height:100vh;display:flex;";
    html += "align-items:center;justify-content:center;";
    html += "background:radial-gradient(circle at top,#1e293b,#020617);";
    html += "font-family:Arial;color:#e5e7eb;}";

    html += ".panel{width:90%;max-width:900px;";
    html += "background:#020617;padding:24px;";
    html += "border-radius:20px;";
    html += "box-shadow:0 0 40px rgba(56,189,248,.35);}";

    html += "h1{text-align:center;margin-bottom:20px;";
    html += "color:#38bdf8;}";

    html += ".section{margin-bottom:22px;";
    html += "padding:16px;border-radius:14px;";
    html += "background:#020617;border:1px solid #1e293b;}";

    html += ".title{font-size:18px;margin-bottom:10px;";
    html += "color:#93c5fd;}";

    html += ".status{font-size:26px;font-weight:bold;text-align:center;}";

    html += ".laser.on{color:#f87171;}";
    html += ".laser.off{color:#4ade80;}";

    html += "iframe{width:100%;height:220px;";
    html += "border:none;border-radius:12px;";
    html += "background:#000;}";

    html += "</style></head><body>";

    html += "<div class='panel'>";
    html += "<h1>ESP 感測整合頁面</h1>";

    // ===== 上半部：雷射偵測 =====
    html += "<div class='section'>";
    html += "<div class='title'>📷 雷射偵測（ESP32-CAM）</div>";

    if (laserDetectedFlag) {
        html += "<div class='status laser on'>🔴 偵測到雷射</div>";
    } else {
        html += "<div class='status laser off'>🟢 無雷射</div>";
    }
    html += "</div>";

    // ===== 下半部：物體偵測 =====
    html += "<div class='section'>";
    html += "<div class='title'>📡 物體偵測（ESP8266 紅外線）</div>";
    html += "<iframe src='http://192.168.1.103/'></iframe>";
    html += "</div>";

    html += "</div></body></html>";

    server.send(200, "text/html; charset=utf-8", html);
});

server.on("/status", []() {
    String json = "{";
    json += "\"laser\":";
    json += laserDetectedFlag ? "true" : "false";
    json += ",";
    json += "\"lastLaserTime\":";
    json += lastLaserTime;
    json += "}";
    server.send(200, "application/json; charset=utf-8", json);
});

    server.begin();
    Serial.println("Web server started");

    // -------- Camera --------http://192.168.1.104/(Google)
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("❌ Camera init failed");
        delay(200);
        return;
    }
    Serial.println("📷 Camera ready");

    sensor_t *s = esp_camera_sensor_get();
    s->set_gain_ctrl(s, 0);     // 關 AGC
    s->set_exposure_ctrl(s, 0); // 關 AEC
    s->set_agc_gain(s, 3);      // 固定增益
    s->set_aec_value(s, 120);   // 固定曝光
}

// ================= Loop =================
void loop() {
  server.handleClient();
    static unsigned long lastDetectTime = 0;
    server.handleClient();

    if (millis() - lastDetectTime > 500) {
        lastDetectTime = millis();

        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            delay(20);
          return;
        }

        int diff = computeDiff(fb);

        if (diff > DIFF_THRESHOLD) {
            clearCount = 0;
            laserHitCount++;

            if (!laserDetectedFlag && laserHitCount >= HIT_CONFIRM) {
                laserDetectedFlag = true;
                lastLaserTime = millis();
                Serial.println("🔴 LASER CONFIRMED");
                sendHttpNotify();
                sendToThingsBoard(true);
            }

            if (laserDetectedFlag) {
                lastLaserTime = millis(); // 持續刷新
            }

        } else {
            laserHitCount = 0;
            clearCount++;

            if (laserDetectedFlag &&
                clearCount >= CLEAR_CONFIRM &&
                millis() - lastLaserTime > LASER_HOLD_MS) {
                laserDetectedFlag = false;
                Serial.println("⚪ LASER CLEARED");
                 sendToThingsBoard(false);
            }
        }

        esp_camera_fb_return(fb);
    }
    delay(1); 
}
