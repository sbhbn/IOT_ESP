#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "55Y8817";
const char* password = "HN73108378";
#define IR_PIN D3
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(IR_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 40) {
      delay(500);
      Serial.print(".");
      Serial.print(" status=");
      Serial.println(WiFi.status());
      retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi 連線成功");
      Serial.print("🌐 IP: http://");
      Serial.println(WiFi.localIP());
  } else {
      Serial.println("\n❌ WiFi 連線失敗");
      Serial.println("⚠ Web Server 不啟動");
      return;
  }

server.on("/", []() {
  int val = digitalRead(IR_PIN);
  bool detected = (val == LOW);

  const char* statusText = detected ? "偵測到物體" : "無物體";
  const char* statusDot  = detected ? "●" : "○";

  String html;
  html.reserve(2048);

  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<title>ESP8266 紅外線感測</title>";
  html += "<style>";
  html += "body{margin:0;height:100vh;display:flex;align-items:center;justify-content:center;";
  html += "background:radial-gradient(circle at top,#1e293b,#020617);color:#e5e7eb;font-family:Arial;}";
  html += ".card{background:#020617;padding:32px 40px;border-radius:18px;";
  html += "box-shadow:0 0 40px rgba(56,189,248,.35);text-align:center;}";
  html += "h1{margin-bottom:20px;font-size:22px;letter-spacing:1px;color:#38bdf8;}";
  html += ".on{color:#f87171;font-size:30px;font-weight:bold;";
  html += "text-shadow:0 0 8px #f87171,0 0 20px rgba(248,113,113,.8);}";
  html += ".off{color:#4ade80;font-size:30px;font-weight:bold;";
  html += "text-shadow:0 0 8px #4ade80,0 0 20px rgba(74,222,128,.8);}";
  html += "</style></head><body>";

  html += "<div class='card'>";
  html += "<h1>ESP8266 紅外線感測</h1>";
  html += "<div class='status ";
  html += detected ? "on'>" : "off'>";
  html += statusDot;
  html += " ";
  html += statusText;
  html += "</div>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
});
  server.begin();
  Serial.println("🚀 Web Server 啟動完成");
}

void loop() {
  server.handleClient();
}
