#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
String getWebPage();
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
const char* ssid = "OFIS-2.4G";
const char* password = "55555555";

#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN 34
#define WATER_LEVEL_PIN 35
#define FAN_RELAY_PIN 26
#define PUMP_RELAY_PIN 27
#define LEFT_ROOF_SIGNAL_PIN 32
#define RIGHT_ROOF_SIGNAL_PIN 33

DHT dht(DHT_PIN, DHT_TYPE);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

struct GreenhouseState {
  float temperature = 0.0;
  float humidity = 0.0;
  int soilMoisture = 0;
  int waterLevel = 0;
  bool fanStatus = false;
  bool pumpStatus = false;
  bool leftRoofOpen = false;
  bool rightRoofOpen = false;
} state;

unsigned long lastSensorRead = 0;
unsigned long lastWebSocketUpdate = 0;
unsigned long lastButtonPress = 0;
const unsigned long SENSOR_INTERVAL = 2000;  // Read sensors every 2 seconds
const unsigned long WEBSOCKET_INTERVAL = 1000;  // Send updates every 1 second
const unsigned long DEBOUNCE_DELAY = 500;  // 500ms debounce for buttons

void initSensors();
void initWebServer();
void readSensors();
void sendWebSocketUpdate();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void controlRelay(int pin, bool state);
void sendRoofSignal(int pin);

void setup() {
  Serial.begin(115200);

  u8g2.begin();   // OLED start
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 16, "Smart Greenhouse");
  u8g2.drawStr(0, 32, "WiFi ulang...");
  u8g2.sendBuffer();

  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(LEFT_ROOF_SIGNAL_PIN, OUTPUT);
  pinMode(RIGHT_ROOF_SIGNAL_PIN, OUTPUT);

  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(LEFT_ROOF_SIGNAL_PIN, LOW);
  digitalWrite(RIGHT_ROOF_SIGNAL_PIN, LOW);

  initSensors();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");

  Serial.println(WiFi.localIP());
  u8g2.clearBuffer();
  u8g2.drawStr(0, 16, "WiFi Connected!");
  u8g2.setCursor(0, 32);
  u8g2.print("IP: ");
  u8g2.print(WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
  delay(2000);

  initWebServer();

  Serial.println("Smart Greenhouse System Ready!");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    readSensors();
    lastSensorRead = currentTime;
    updateOLED();
  }

  if (currentTime - lastWebSocketUpdate >= WEBSOCKET_INTERVAL) {
    sendWebSocketUpdate();
    lastWebSocketUpdate = currentTime;
  }

  ws.cleanupClients();
}

void updateOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  // IP manzil doimo 1-qator
  u8g2.setCursor(0, 10);
  u8g2.print("IP: ");
  u8g2.print(WiFi.localIP().toString().c_str());

  // Sensor ma'lumotlari
  u8g2.setCursor(0, 24);
  u8g2.print("T: ");
  u8g2.print(state.temperature, 1);
  u8g2.print(" C");

  u8g2.setCursor(0, 36);
  u8g2.print("H: ");
  u8g2.print(state.humidity, 1);
  u8g2.print(" %");

  u8g2.setCursor(0, 48);
  u8g2.print("Soil: ");
  u8g2.print(state.soilMoisture);
  u8g2.print(" %");

  u8g2.setCursor(0, 60);
  u8g2.print("Water: ");
  u8g2.print(state.waterLevel);
  u8g2.print(" %");

  u8g2.sendBuffer();
}

void initSensors() {
  dht.begin();
  Serial.println("Sensors initialized");
}

void readSensors() {
  state.temperature = dht.readTemperature();
  state.humidity = dht.readHumidity();

  if (isnan(state.temperature) || isnan(state.humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    state.temperature = 0.0;
    state.humidity = 0.0;
  }

  int soilRaw = analogRead(SOIL_MOISTURE_PIN);
  state.soilMoisture = map(soilRaw, 0, 4095, 100, 0);

  int waterRaw = analogRead(WATER_LEVEL_PIN);
  state.waterLevel = map(waterRaw, 0, 4095, 0, 100);

  // Debug output
  Serial.printf("Temp: %.1f°C, Humidity: %.1f%%, Soil: %d%%, Water: %d%%\n",
                state.temperature, state.humidity, state.soilMoisture, state.waterLevel);
}

void sendWebSocketUpdate() {
  StaticJsonDocument<300> doc;
  doc["temperature"] = state.temperature;
  doc["humidity"] = state.humidity;
  doc["soilMoisture"] = state.soilMoisture;
  doc["waterLevel"] = state.waterLevel;
  doc["fanStatus"] = state.fanStatus;
  doc["pumpStatus"] = state.pumpStatus;
  doc["leftRoofOpen"] = state.leftRoofOpen;
  doc["rightRoofOpen"] = state.rightRoofOpen;

  String message;
  serializeJson(doc, message);
  ws.textAll(message);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

    // Debounce check
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress < DEBOUNCE_DELAY) {
      return;  // Ignore rapid button presses
    }
    lastButtonPress = currentTime;

    data[len] = 0;
    String message = (char*)data;

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      return;
    }

    String command = doc["command"];

    if (command == "toggleFan") {
      state.fanStatus = !state.fanStatus;
      controlRelay(FAN_RELAY_PIN, state.fanStatus);
      Serial.println("Fan toggled: " + String(state.fanStatus ? "ON" : "OFF"));
    }
    else if (command == "togglePump") {
      state.pumpStatus = !state.pumpStatus;
      controlRelay(PUMP_RELAY_PIN, state.pumpStatus);
      Serial.println("Pump toggled: " + String(state.pumpStatus ? "ON" : "OFF"));
    }
    else if (command == "toggleLeftRoof") {
      state.leftRoofOpen = !state.leftRoofOpen;
      sendRoofSignal(LEFT_ROOF_SIGNAL_PIN);
      Serial.println("Left roof toggled: " + String(state.leftRoofOpen ? "OPEN" : "CLOSED"));
    }
    else if (command == "toggleRightRoof") {
      state.rightRoofOpen = !state.rightRoofOpen;
      sendRoofSignal(RIGHT_ROOF_SIGNAL_PIN);
      Serial.println("Right roof toggled: " + String(state.rightRoofOpen ? "OPEN" : "CLOSED"));
    }

    // Send immediate update after command
    sendWebSocketUpdate();
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendWebSocketUpdate();  // Send current state to new client
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebServer() {
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Serve the main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", getWebPage().c_str());
  });

  server.begin();
  Serial.println("Web server started");
}

void controlRelay(int pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

// Yangi funksiya: roof signal yuborish
void sendRoofSignal(int pin) {
  // Qisqa impulse signal yuborish
  digitalWrite(pin, HIGH);
  delay(100);  // 100ms signal
  digitalWrite(pin, LOW);
}

String getWebPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Greenhouse Control</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c28, #2d5a3d);
            color: #ffffff;
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        .header {
            text-align: center;
            margin-bottom: 30px;
            padding: 20px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 15px;
            backdrop-filter: blur(10px);
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
        }

        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }

        .status-card {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 15px;
            padding: 20px;
            text-align: center;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.2);
            transition: transform 0.3s ease;
        }

        .status-card:hover {
            transform: translateY(-5px);
        }

        .status-card h3 {
            color: #90EE90;
            margin-bottom: 10px;
            font-size: 1.2em;
        }

        .status-value {
            font-size: 2em;
            font-weight: bold;
            margin-bottom: 5px;
        }

        .status-unit {
            color: #cccccc;
            font-size: 0.9em;
        }

        .controls-section {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 15px;
            padding: 30px;
            backdrop-filter: blur(10px);
        }

        .controls-title {
            text-align: center;
            font-size: 1.8em;
            margin-bottom: 25px;
            color: #90EE90;
        }

        .control-group {
            margin-bottom: 25px;
        }

        .control-group h4 {
            color: #90EE90;
            margin-bottom: 15px;
            font-size: 1.3em;
        }

        .button-row {
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
            justify-content: center;
        }

        .btn {
            padding: 12px 25px;
            border: none;
            border-radius: 8px;
            font-size: 1em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            min-width: 140px;
            position: relative;
            overflow: hidden;
        }

        .btn:before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
            transition: left 0.5s;
        }

        .btn:hover:before {
            left: 100%;
        }

        .btn-toggle {
            background: linear-gradient(45deg, #2196F3, #42A5F5);
            color: white;
        }

        .btn-toggle:hover {
            background: linear-gradient(45deg, #1976D2, #2196F3);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(33, 150, 243, 0.4);
        }

        .btn-toggle.active {
            background: linear-gradient(45deg, #FF9800, #FFB74D);
        }

        .btn-roof {
            background: linear-gradient(45deg, #9C27B0, #BA68C8);
            color: white;
            min-width: 180px;
        }

        .btn-roof:hover {
            background: linear-gradient(45deg, #7B1FA2, #9C27B0);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(156, 39, 176, 0.4);
        }

        .status-indicator {
            display: inline-block;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 0.9em;
            font-weight: bold;
            margin-left: 10px;
        }

        .status-on {
            background: #4CAF50;
            color: white;
        }

        .status-off {
            background: #757575;
            color: white;
        }

        .connection-status {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 10px 20px;
            border-radius: 25px;
            font-weight: bold;
            transition: all 0.3s ease;
        }

        .connected {
            background: #4CAF50;
            color: white;
        }

        .disconnected {
            background: #f44336;
            color: white;
        }

        @media (max-width: 768px) {
            .button-row {
                flex-direction: column;
                align-items: center;
            }

            .btn {
                width: 100%;
                max-width: 300px;
            }
        }
    </style>
</head>
<body>
    <div class="connection-status" id="connectionStatus">Disconnected</div>

    <div class="container">
        <div class="header">
            <h1>🌱 Smart Greenhouse Boshqaruvi</h1>
            <p>Real vadtda monitoring qilish tizimi</p>
        </div>

        <div class="status-grid">
            <div class="status-card">
                <h3>🌡️ Havo Harorati</h3>
                <div class="status-value" id="temperature">--</div>
                <div class="status-unit">°C</div>
            </div>

            <div class="status-card">
                <h3>💧 Havo Namligi</h3>
                <div class="status-value" id="humidity">--</div>
                <div class="status-unit">%</div>
            </div>

            <div class="status-card">
                <h3>🌱 Tuproq Namligi</h3>
                <div class="status-value" id="soilMoisture">--</div>
                <div class="status-unit">%</div>
            </div>

            <div class="status-card">
                <h3>🚰 Suv Miqdori</h3>
                <div class="status-value" id="waterLevel">--</div>
                <div class="status-unit">%</div>
            </div>
        </div>

        <div class="controls-section">
            <h2 class="controls-title">Tizim Boshqaruvlari</h2>

            <div class="control-group">
                <h4>🏠 Lyuk Boshqaruvi (On/Off)</h4>
                <div class="button-row">
                    <button class="btn btn-roof" onclick="sendCommand('toggleLeftRoof')">
                        Chap Lyuk
                        <span class="status-indicator status-off" id="leftRoofStatus">OFF</span>
                    </button>
                    <button class="btn btn-roof" onclick="sendCommand('toggleRightRoof')">
                        O'ng Lyuk
                        <span class="status-indicator status-off" id="rightRoofStatus">OFF</span>
                    </button>
                </div>
            </div>

            <div class="control-group">
                <h4>💨 Havo va Suv Tizimlari</h4>
                <div class="button-row">
                    <button class="btn btn-toggle" id="fanBtn" onclick="sendCommand('toggleFan')">
                        Ventelyatsiya <span class="status-indicator" id="fanStatus">OFF</span>
                    </button>
                    <button class="btn btn-toggle" id="pumpBtn" onclick="sendCommand('togglePump')">
                        Suv nasos <span class="status-indicator" id="pumpStatus">OFF</span>
                    </button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let socket;
        let isConnected = false;
        let lastCommandTime = 0;
        const DEBOUNCE_DELAY = 500;

        function initWebSocket() {
            const wsUrl = `ws://${window.location.hostname}/ws`;
            socket = new WebSocket(wsUrl);

            socket.onopen = function() {
                isConnected = true;
                updateConnectionStatus();
                console.log('WebSocket connected');
            };

            socket.onclose = function() {
                isConnected = false;
                updateConnectionStatus();
                console.log('WebSocket disconnected');
                // Attempt to reconnect after 3 seconds
                setTimeout(initWebSocket, 3000);
            };

            socket.onerror = function(error) {
                console.log('WebSocket error:', error);
                isConnected = false;
                updateConnectionStatus();
            };

            socket.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    updateDisplay(data);
                } catch (error) {
                    console.error('Error parsing WebSocket message:', error);
                }
            };
        }

        function updateConnectionStatus() {
            const statusElement = document.getElementById('connectionStatus');
            if (isConnected) {
                statusElement.textContent = 'Connected';
                statusElement.className = 'connection-status connected';
            } else {
                statusElement.textContent = 'Disconnected';
                statusElement.className = 'connection-status disconnected';
            }
        }

        function updateDisplay(data) {
            document.getElementById('temperature').textContent = data.temperature.toFixed(1);
            document.getElementById('humidity').textContent = data.humidity.toFixed(1);
            document.getElementById('soilMoisture').textContent = data.soilMoisture;
            document.getElementById('waterLevel').textContent = data.waterLevel;

            // Update status indicators - doimiy ON/OFF yozuvi
            updateStatusIndicator('fanStatus', data.fanStatus);
            updateStatusIndicator('pumpStatus', data.pumpStatus);
            updateStatusIndicator('leftRoofStatus', data.leftRoofOpen);
            updateStatusIndicator('rightRoofStatus', data.rightRoofOpen);

            // Update button states
            updateButtonState('fanBtn', data.fanStatus);
            updateButtonState('pumpBtn', data.pumpStatus);
        }

        function updateStatusIndicator(elementId, status) {
            const element = document.getElementById(elementId);
            element.textContent = status ? 'ON' : 'OFF';
            element.className = `status-indicator ${status ? 'status-on' : 'status-off'}`;
        }

        function updateButtonState(buttonId, isActive) {
            const button = document.getElementById(buttonId);
            if (isActive) {
                button.classList.add('active');
            } else {
                button.classList.remove('active');
            }
        }

        function sendCommand(command) {
            const currentTime = Date.now();
            if (currentTime - lastCommandTime < DEBOUNCE_DELAY) {
                return; // Ignore rapid button presses
            }
            lastCommandTime = currentTime;

            if (socket && isConnected) {
                const message = JSON.stringify({ command: command });
                socket.send(message);
                console.log('Sent command:', command);
            } else {
                alert('WebSocket not connected. Please wait for connection.');
            }
        }

        // Initialize WebSocket connection when page loads
        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";
}