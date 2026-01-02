#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>   // WiFiManager for easy Wi-Fi setup [web:61][web:62][web:71]

#include <OneWire.h>
#include <DallasTemperature.h>     // DS18B20 [web:16][web:18][web:28]
#include <TinyGPSPlus.h>           // GPS [web:33]

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>      // OLED [web:51][web:54]

// ===================== Pins =====================
// L298N
#define IN1 13
#define IN2 14
#define IN3 27
#define IN4 34   // change if your board cannot drive 34 as output

// Relays (active LOW)
#define RELAY_FAN_PIN     25   // Relay IN1 (fan)
#define RELAY_HEATER_PIN  32   // Relay IN2 (heater)
const bool RELAY_ACTIVE_LOW = true;

// DS18B20
#define DS18B20_PIN 26        // data wire, 4.7k to 3.3V

// Switches (to GND, INPUT_PULLUP)
#define SW_FAN_PIN    15      // manual fan
#define SW_HEATER_PIN 4       // manual heater

// GPS (UART1)
#define GPS_RX_PIN 16         // ESP32 RX  <- GPS TX
#define GPS_TX_PIN 17         // ESP32 TX  -> GPS RX

// OLED SSD1306 I2C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1       // shared reset

// ===================== Objects =====================
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);     // DS18B20

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WebServer server(80);

// ===================== State variables =====================
float currentTempC = NAN;
float currentLat   = 0.0;
float currentLon   = 0.0;

bool fanOn         = false;
bool heaterOn      = false;
bool systemLocked  = false;

bool fanManual     = false;
bool heaterManual  = false;

String staSSID     = "";

unsigned long lastTempRead = 0;
unsigned long lastOLED     = 0;
unsigned long lastIPShow   = 0;
bool ipShownOnce           = false;

// ===================== Helpers =====================
inline void setRelay(uint8_t pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

// L298N fan motor control
void fanMotorOn() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void fanMotorOff() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ---------------- DS18B20 read ----------------
void readTemperature() {
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  if (!isnan(t) && t > -100 && t < 125) {
    currentTempC = t;
  }
}

// ---------------- Control logic ----------------
void updateControlLogic() {
  // Read switches (active LOW)
  fanManual    = (digitalRead(SW_FAN_PIN)    == LOW);
  heaterManual = (digitalRead(SW_HEATER_PIN) == LOW);

  bool fanAuto    = false;
  bool heaterAuto = false;

  systemLocked = false;

  // 1) Lock condition: both switches pressed
  if (fanManual && heaterManual) {
    systemLocked = true;
    fanOn = false;
    heaterOn = false;

    setRelay(RELAY_FAN_PIN, false);
    setRelay(RELAY_HEATER_PIN, false);
    fanMotorOff();
    return;
  }

  // 2) Manual overrides (only one switch pressed)
  if (fanManual) {
    fanOn = true;
    heaterOn = false;
  }
  else if (heaterManual) {
    heaterOn = true;
    fanOn = false;
  }
  else {
    // 3) Automatic temperature logic
    if (!isnan(currentTempC)) {
      if (currentTempC > 28.0) {
        fanAuto = true;
        heaterAuto = false;
      } else if (currentTempC < 24.0) {
        heaterAuto = true;
        fanAuto = false;
      } else {
        fanAuto = false;
        heaterAuto = false;
      }
    }

    fanOn    = fanAuto;
    heaterOn = heaterAuto;
  }

  // Apply to hardware
  if (fanOn) {
    setRelay(RELAY_FAN_PIN, true);
    fanMotorOn();
  } else {
    setRelay(RELAY_FAN_PIN, false);
    fanMotorOff();
  }

  if (heaterOn) {
    setRelay(RELAY_HEATER_PIN, true);
  } else {
    setRelay(RELAY_HEATER_PIN, false);
  }
}

// ---------------- OLED update ----------------
void showIPOnOLED() {
  IPAddress ip = WiFi.localIP();
  if (ip.toString() == "0.0.0.0") return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Connected WiFi:");
  display.setCursor(0, 16);
  display.print(staSSID);
  display.setCursor(0, 32);
  display.print("IP:");
  display.setCursor(0, 48);
  display.print(ip.toString());
  display.display();
}

void updateOLED() {
  // If just connected and IP not shown yet, show IP for a few seconds
  if (WiFi.status() == WL_CONNECTED && !ipShownOnce) {
    showIPOnOLED();
    ipShownOnce = true;
    lastIPShow = millis();
    return;
  }

  // After 5 seconds from IP display, switch to normal temp/status screen
  if (ipShownOnce && (millis() - lastIPShow < 5000)) {
    // keep showing IP for the full period
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Big temperature in middle
  display.setTextSize(2);
  display.setCursor(0, 8);
  if (!isnan(currentTempC)) {
    display.print(currentTempC, 1);
    display.print(" C");
  } else {
    display.print("--.- C");
  }

  // Status line at bottom
  display.setTextSize(1);
  display.setCursor(0, 40);

  if (systemLocked) {
    display.print("System Locked");
  } else if (fanManual) {
    display.print("Manual Fan ON");
  } else if (heaterManual) {
    display.print("Manual Heater ON");
  } else {
    if (fanOn) {
      display.print("Cooling Fan: ON");
    } else if (heaterOn) {
      display.print("Heater: ON");
    } else {
      display.print("System Idle");
    }
  }

  display.display();
}

// ---------------- GPS handling ----------------
void handleGPS() {
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }

  if (gps.location.isValid()) {
    currentLat = gps.location.lat();
    currentLon = gps.location.lng();
  }
}

// ===================== Web UI =====================

// Dashboard HTML (similar style to your friend, but simpler)
const char DASH_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Army Jacket</title>
<style>
:root{
  --bg:#111827;--panel:#1f2937;--accent:#2563eb;
  --ok:#22c55e;--danger:#ef4444;--lock:#fbbf24;--text:#e5e7eb
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:12px}
header{padding:12px 0;font-weight:700;font-size:22px;text-align:center}
.container{max-width:960px;margin:0 auto}
.card{background:var(--panel);border-radius:16px;padding:16px;margin:10px 0;box-shadow:0 8px 24px rgba(0,0,0,.35)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.row{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;background:#111827;border-radius:12px}
.label{opacity:.85;font-size:14px}
.value{font-weight:600;font-size:14px}
.chip{display:inline-flex;align-items:center;gap:6px;padding:6px 10px;border-radius:999px;font-size:12px}
.chip.ok{background:rgba(34,197,94,.1);color:var(--ok)}
.chip.bad{background:rgba(239,68,68,.1);color:var(--danger)}
.chip.lock{background:rgba(251,191,36,.1);color:var(--lock)}
.dot{width:10px;height:10px;border-radius:999px}
.dot.ok{background:var(--ok)}
.dot.bad{background:var(--danger)}
.dot.lock{background:var(--lock)}
.btn{border:0;border-radius:999px;padding:8px 14px;background:var(--accent);color:white;font-weight:600;font-size:13px;cursor:pointer;text-decoration:none;display:inline-block}
.btn.secondary{background:#374151}
.btn-row{display:flex;gap:10px;flex-wrap:wrap;margin-top:8px}
.footer{opacity:.7;text-align:center;margin-top:12px;font-size:11px}
.small{font-size:12px;opacity:.9}
</style></head><body>
<header>Smart Army Jacket</header>
<div class="container">

  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap">
      <div>
        <div class="small">Wi-Fi Network</div>
        <div id="ssid" class="value">--</div>
      </div>
      <div>
        <div class="small">Device IP</div>
        <div id="ip" class="value">--</div>
      </div>
    </div>
    <div class="btn-row">
      <button id="refresh" class="btn">Refresh</button>
      <button id="map" class="btn secondary">Open Map</button>
    </div>
    <div id="status" class="small" style="margin-top:6px">Status: <b>Connecting...</b></div>
  </div>

  <div class="card">
    <h3 style="margin-bottom:8px;font-size:16px;">Sensor Data</h3>
    <div class="grid">
      <div class="row"><div class="label">Temperature</div><div id="temp" class="value">-- &deg;C</div></div>
      <div class="row"><div class="label">Mode</div><div id="mode" class="value">--</div></div>
      <div class="row"><div class="label">Fan</div><div id="fan" class="value"><span class="chip bad"><span class="dot bad"></span>OFF</span></div></div>
      <div class="row"><div class="label">Heater</div><div id="heater" class="value"><span class="chip bad"><span class="dot bad"></span>OFF</span></div></div>
      <div class="row"><div class="label">Latitude</div><div id="lat" class="value">--</div></div>
      <div class="row"><div class="label">Longitude</div><div id="lon" class="value">--</div></div>
    </div>
  </div>

  <div class="footer">Served by Smart Army Jacket ESP32</div>
</div>

<script>
const el = id => document.getElementById(id);

function chipHTML(on, lock=false){
  if(lock) return '<span class="chip lock"><span class="dot lock"></span>LOCKED</span>';
  return on
    ? '<span class="chip ok"><span class="dot ok"></span>ON</span>'
    : '<span class="chip bad"><span class="dot bad"></span>OFF</span>';
}

function updateUI(d){
  el('ssid').textContent = d.sta_ssid || '(not connected)';
  el('ip').textContent   = d.ip || '--';

  if(d.temp_c != null){
    el('temp').innerHTML = d.temp_c.toFixed(1) + ' &deg;C';
  } else {
    el('temp').innerHTML = '-- &deg;C';
  }

  el('lat').textContent = d.lat != null ? d.lat.toFixed(6) : '--';
  el('lon').textContent = d.lon != null ? d.lon.toFixed(6) : '--';

  if(d.lock){
    el('mode').textContent = 'System Locked';
    el('fan').innerHTML    = chipHTML(false,true);
    el('heater').innerHTML = chipHTML(false,true);
  }else{
    el('mode').textContent = d.mode || '--';
    el('fan').innerHTML    = chipHTML(d.fan === 'ON');
    el('heater').innerHTML = chipHTML(d.heater === 'ON');
  }
}

async function fetchData(){
  try{
    const r = await fetch('/data',{cache:'no-store'});
    const j = await r.json();
    updateUI(j);
    el('status').innerHTML = 'Status: <b>Connected</b>';
  }catch(e){
    el('status').innerHTML = 'Status: <b>Offline</b>';
  }
}

el('refresh').addEventListener('click', fetchData);
el('map').addEventListener('click', ()=>{
  const lat = el('lat').textContent, lon = el('lon').textContent;
  if(lat && lon && lat !== '--' && lon !== '--'){
    window.open(`https://www.google.com/maps?q=${lat},${lon}`,'_blank');
  }else{
    alert('No GPS data yet.');
  }
});

fetchData();
setInterval(fetchData, 2000);
</script>
</body></html>
)HTML";

// JSON data for dashboard
void handleDataJson() {
  String ssidNow = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "";
  IPAddress ip = WiFi.localIP();

  String modeStr = "--";
  if (systemLocked) {
    modeStr = "Locked";
  } else if (fanManual) {
    modeStr = "Manual Fan";
  } else if (heaterManual) {
    modeStr = "Manual Heater";
  } else {
    modeStr = "Auto";
  }

  String json = "{";
  json += "\"lat\":" + String(currentLat, 6) + ",";
  json += "\"lon\":" + String(currentLon, 6) + ",";
  if (isnan(currentTempC)) {
    json += "\"temp_c\":null,";
  } else {
    json += "\"temp_c\":" + String(currentTempC, 1) + ",";
  }
  json += "\"fan\":\"" + String(fanOn ? "ON" : "OFF") + "\",";
  json += "\"heater\":\"" + String(heaterOn ? "ON" : "OFF") + "\",";
  json += "\"lock\":" + String(systemLocked ? "true" : "false") + ",";
  json += "\"mode\":\"" + modeStr + "\",";
  json += "\"sta_ssid\":\"" + ssidNow + "\",";
  json += "\"ip\":\"" + ip.toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", DASH_HTML);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  // Pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(RELAY_FAN_PIN, OUTPUT);
  pinMode(RELAY_HEATER_PIN, OUTPUT);

  pinMode(SW_FAN_PIN, INPUT_PULLUP);
  pinMode(SW_HEATER_PIN, INPUT_PULLUP);

  fanMotorOff();
  setRelay(RELAY_FAN_PIN, false);
  setRelay(RELAY_HEATER_PIN, false);

  // DS18B20
  ds18b20.begin();

  // GPS
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed");
  } else {
    display.clearDisplay();
    display.display();
  }

  // WiFiManager
  WiFiManager wifiManager;
  // wifiManager.resetSettings(); // uncomment once if you want to clear saved Wi-Fi
  if (!wifiManager.autoConnect("Smart Army Dress")) {
    Serial.println("WiFi failed, restarting...");
    delay(3000);
    ESP.restart();
  }

  staSSID = WiFi.SSID();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleDataJson);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

// ===================== Loop =====================
void loop() {
  // GPS
  handleGPS();

  unsigned long now = millis();

  // Temp read & control every 2s
  if (now - lastTempRead >= 2000) {
    lastTempRead = now;
    readTemperature();
    updateControlLogic();
  } else {
    // still check switches frequently
    updateControlLogic();
  }

  // OLED refresh
  if (now - lastOLED >= 300) {
    lastOLED = now;
    updateOLED();
  }

  server.handleClient();
}
