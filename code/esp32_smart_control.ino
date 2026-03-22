#include <WiFi.h>
#include "DHT.h"

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";
String apiKey = "YOUR_API_KEY";

WiFiServer server(80);

int ledPin = 2;
const int pwmFreq = 5000;
const int pwmResolution = 8;

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  ledcAttach(ledPin, pwmFreq, pwmResolution);
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting...");
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {

  // ThingSpeak Upload every 15s
  if (millis() - lastUpdate > 15000) {
    lastUpdate = millis();

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    if (!isnan(temp) && !isnan(hum)) {
      WiFiClient tsClient;
      if (tsClient.connect("api.thingspeak.com", 80)) {
        String url = "/update?api_key=" + apiKey;
        url += "&field1=" + String(temp);
        url += "&field2=" + String(hum);

        tsClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: api.thingspeak.com\r\n" +
                       "Connection: close\r\n\r\n");

        Serial.println("Sent to ThingSpeak");
        Serial.print("Temp: "); Serial.print(temp);
        Serial.print(" | Hum: "); Serial.println(hum);
      }
    } else {
      Serial.println("DHT read failed");
    }
  }

  // Web Server
  WiFiClient client = server.available();

  if (client) {
    String request = client.readString();

    // ✅ JSON endpoint for live data
    if (request.indexOf("GET /data") >= 0) {
      float temp = dht.readTemperature();
      float hum  = dht.readHumidity();

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      client.print("{\"temp\":");
      client.print(isnan(temp) ? "null" : String(temp, 1));
      client.print(",\"hum\":");
      client.print(isnan(hum)  ? "null" : String(hum, 1));
      client.print("}");

      client.stop();
      return;
    }

    // LED ON/OFF
    if (request.indexOf("/LED=ON") != -1)  ledcWrite(ledPin, 255);
    if (request.indexOf("/LED=OFF") != -1) ledcWrite(ledPin, 0);

    // Brightness
    if (request.indexOf("/brightness?value=") != -1) {
      int idx = request.indexOf("value=") + 6;
      int brightness = request.substring(idx).toInt();
      ledcWrite(ledPin, brightness);
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();

    client.println(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>ESP32 Smart Control</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }

    body {
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      background: #0f0f0f;
      font-family: 'Segoe UI', Arial, sans-serif;
      color: #fff;
    }

    .card {
      background: #1a1a1a;
      border: 1px solid #2a2a2a;
      border-radius: 24px;
      padding: 48px 56px;
      text-align: center;
      width: 380px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.5);
    }

    h1 {
      font-size: 1.6rem;
      letter-spacing: 1px;
      color: #fff;
      margin-bottom: 8px;
    }

    #status {
      font-size: 0.85rem;
      letter-spacing: 2px;
      text-transform: uppercase;
      margin-bottom: 28px;
      color: #d50000;
    }
    #status.on  { color: #00c853; }

    .btn-row {
      display: flex;
      gap: 16px;
      justify-content: center;
      margin-bottom: 28px;
    }

    .btn {
      flex: 1;
      padding: 14px 0;
      font-size: 1rem;
      font-weight: 600;
      letter-spacing: 1px;
      border: none;
      border-radius: 12px;
      cursor: pointer;
      transition: opacity 0.2s, transform 0.1s;
    }

    .btn:hover  { opacity: 0.85; }
    .btn:active { transform: scale(0.97); }
    .on  { background: #00c853; color: #fff; }
    .off { background: #d50000; color: #fff; }

    .section-label {
      font-size: 0.75rem;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: #555;
      margin-bottom: 12px;
    }

    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      background: #333;
      border-radius: 3px;
      outline: none;
      margin-bottom: 28px;
    }

    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px; height: 20px;
      background: #ff6b35;
      border-radius: 50%;
      cursor: pointer;
    }

    /* ✅ Sensor cards row */
    .sensor-row {
      display: flex;
      gap: 12px;
      margin-bottom: 16px;
    }

    .sensor-card {
      flex: 1;
      background: #111;
      border: 1px solid #222;
      border-radius: 14px;
      padding: 16px 12px;
    }

    .sensor-card.temp { border-top: 3px solid #ff6b35; }
    .sensor-card.hum  { border-top: 3px solid #00b4d8; }

    .sensor-icon  { font-size: 1.5rem; margin-bottom: 6px; }

    .sensor-label {
      font-size: 0.65rem;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: #555;
      margin-bottom: 6px;
    }

    .sensor-value {
      font-size: 1.6rem;
      font-weight: 700;
    }

    .sensor-card.temp .sensor-value { color: #ff6b35; }
    .sensor-card.hum  .sensor-value { color: #00b4d8; }

    .sensor-unit {
      font-size: 0.85rem;
      color: #666;
      margin-left: 2px;
    }

    .thingspeak-box {
      background: #111;
      border: 1px solid #222;
      border-radius: 14px;
      padding: 14px;
    }

    .thingspeak-box h2 {
      font-size: 0.9rem;
      font-weight: 600;
      color: #00b4d8;
      margin-bottom: 4px;
    }

    .thingspeak-box p {
      font-size: 0.75rem;
      color: #555;
    }

    .dot {
      display: inline-block;
      width: 8px; height: 8px;
      background: #00e676;
      border-radius: 50%;
      margin-right: 6px;
      animation: pulse 1.5s infinite;
    }

    @keyframes pulse {
      0%,100% { opacity:1; }
      50%      { opacity:0.2; }
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP32 Smart Control</h1>
    <p id="status">&#9679; Status: OFF</p>

    <div class="btn-row">
      <button class="btn on"  onclick="toggleLED('on')">ON</button>
      <button class="btn off" onclick="toggleLED('off')">OFF</button>
    </div>

    <div class="section-label">Brightness</div>
    <input type="range" min="0" max="255" value="0"
           oninput="setBrightness(this.value)">

    <!-- ✅ Live sensor cards -->
    <div class="sensor-row">
      <div class="sensor-card temp">
        <div class="sensor-icon">&#x1F321;</div>
        <div class="sensor-label">Temperature</div>
        <div class="sensor-value">
          <span id="temp">--</span>
          <span class="sensor-unit">°C</span>
        </div>
      </div>
      <div class="sensor-card hum">
        <div class="sensor-icon">&#x1F4A7;</div>
        <div class="sensor-label">Humidity</div>
        <div class="sensor-value">
          <span id="hum">--</span>
          <span class="sensor-unit">%</span>
        </div>
      </div>
    </div>

    <div class="thingspeak-box">
      <h2>Temperature &amp; Humidity</h2>
      <p><span class="dot"></span>Logging to ThingSpeak every 15s</p>
    </div>
  </div>

  <script>
    function toggleLED(state) {
      fetch('/LED=' + state.toUpperCase());
      const s = document.getElementById('status');
      s.innerHTML = '&#9679; Status: ' + state.toUpperCase();
      s.className  = state === 'on' ? 'on' : '';
    }

    function setBrightness(value) {
      fetch('/brightness?value=' + value);
    }

    // ✅ Fetch live sensor data every 3 seconds
    function updateSensors() {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          document.getElementById('temp').innerText = data.temp !== null ? data.temp : '--';
          document.getElementById('hum').innerText  = data.hum  !== null ? data.hum  : '--';
        })
        .catch(() => {
          document.getElementById('temp').innerText = '--';
          document.getElementById('hum').innerText  = '--';
        });
    }

    setInterval(updateSensors, 3000);
    updateSensors();
  </script>
</body>
</html>
)rawliteral");

    client.stop();
  }
}
