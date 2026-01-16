#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========== CẤU HÌNH OLED ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== CẤU HÌNH WIFI ==========
const char* ssid = "Wifi chùa";
const char* password = "3thanghe";
WebServer server(80);

// ========== ĐỊA CHỈ IP CỦA ESP32-CAM ==========
const char* camIP = "10.105.11.103";

// ========== CẤU HÌNH CHÂN ==========
const int in1 = 26, in2 = 27, in3 = 14, in4 = 12;
const int enA = 25, enB = 33; // enA thường là bên Trái, enB là bên Phải
const int sensorT1 = 19, sensorT2 = 18, sensorS = 5;
const int sensorP1 = 17, sensorP2 = 16;

// ========== SERVO + SIÊU ÂM ==========
const int servoPin = 13;
const int trigPin = 32;
const int echoPin = 35;
Servo myServo;
int distanceThreshold = 50;
bool obstacleAvoidance = false;

// ========== CÂN BẰNG ĐỘNG CƠ (SỬA Ở ĐÂY) ==========
// Vì bánh TRÁI nhanh hơn, ta giảm nó xuống còn 85% (0.85)
// Nếu vẫn nhanh hơn, giảm xuống 0.8. Nếu yếu quá thì tăng lên 0.9
float leftFactor = 0.85; 
float rightFactor = 1.0; 

// ========== BIẾN ĐIỀU KHIỂN ==========
int speedPWM = 190;
int turnSpeed = 200;  // Tốc độ khi rẽ
int curveSpeed = 140; // Tốc độ khi vào cua
bool autoMode = false; 
String currentAction = "IDLE";
int currentDistance = 0;
String currentMode = "MANUAL";

// Biến nhớ hướng rẽ để tìm lại line
int lastTurnDirection = 0; 
unsigned long lastLineTime = 0; 
const unsigned long lineTimeout = 300; 

// ========== HÀM CẬP NHẬT OLED ==========
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Hiển thị chế độ ở giữa màn hình
  int16_t x1, y1;
  uint16_t w, h;
  
  if (autoMode) {
    display.getTextBounds("DO LINE", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 10);
    display.println("DO LINE");
  } 
  else if (obstacleAvoidance) {
    display.getTextBounds("VAT CAN", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 10);
    display.println("VAT CAN");
  } 
  else {
    display.getTextBounds("MANUAL", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 10);
    display.println("MANUAL");
  }
  
  // Hiển thị khoảng cách
  display.setTextSize(1);
  String distText = "Dist: " + String(currentDistance) + " cm";
  display.getTextBounds(distText.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 40);
  display.println(distText);
  
  // Hiển thị tốc độ
  String speedText = "Speed: " + String(speedPWM);
  display.getTextBounds(speedText.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 52);
  display.println(speedText);
  
  display.display();
}

// ========== HTML GIAO DIỆN ==========
String getMainPage() {
  String page = R"=====(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Robot Control</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: 'Segoe UI', Tahoma, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px;
}
.container {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 20px;
  padding: 30px;
  max-width: 500px;
  width: 100%;
  box-shadow: 0 20px 60px rgba(0,0,0,0.3);
}
h1 {
  text-align: center;
  color: #667eea;
  margin-bottom: 10px;
  font-size: 24px;
}
.status {
  text-align: center;
  padding: 15px;
  background: #f0f4ff;
  border-radius: 10px;
  margin-bottom: 20px;
}
.status-text {
  font-size: 18px;
  font-weight: bold;
  color: #333;
}
.mode-badge {
  display: inline-block;
  padding: 5px 15px;
  border-radius: 20px;
  font-size: 14px;
  margin-top: 5px;
}
.auto { background: #4CAF50; color: white; }
.manual { background: #ff9800; color: white; }
.distance-box {
  background: #000;
  color: #0f0;
  padding: 20px;
  border-radius: 10px;
  text-align: center;
  font-size: 32px;
  font-weight: bold;
  margin-bottom: 20px;
  font-family: 'Courier New', monospace;
  border: 2px solid #0f0;
}
.distance-warning {
  background: #ff5722;
  color: white;
  border-color: #ff5722;
}
.camera-box {
  width: 100%;
  height: 240px;
  background: #000;
  border-radius: 10px;
  margin-bottom: 20px;
  overflow: hidden;
  border: 2px solid #667eea;
  position: relative;
}
.camera-box img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.camera-error {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: #ff5722;
  font-size: 14px;
  text-align: center;
  display: none;
}
.controls {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 10px;
  margin-bottom: 20px;
}
.btn {
  padding: 20px;
  font-size: 24px;
  border: none;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.3s;
  color: white;
  font-weight: bold;
  box-shadow: 0 4px 10px rgba(0,0,0,0.2);
}
.btn:active { transform: scale(0.95); }
.btn-forward { background: #4CAF50; grid-column: 2; }
.btn-left { background: #2196F3; grid-row: 2; }
.btn-stop { background: #f44336; grid-row: 2; grid-column: 2; }
.btn-right { background: #2196F3; grid-row: 2; grid-column: 3; }
.btn-back { background: #FF9800; grid-column: 2; }
.settings {
  display: flex;
  gap: 10px;
  margin-bottom: 20px;
}
.settings button {
  flex: 1;
  padding: 15px;
  border: none;
  border-radius: 10px;
  cursor: pointer;
  font-size: 16px;
  font-weight: bold;
  transition: all 0.3s;
}
.btn-auto { background: #4CAF50; color: white; }
.btn-manual { background: #ff9800; color: white; }
.btn-obstacle { background: #9C27B0; color: white; }
.speed-control, .threshold-control {
  display: flex;
  align-items: center;
  gap: 10px;
  background: #f0f4ff;
  padding: 15px;
  border-radius: 10px;
  margin-bottom: 15px;
}
.speed-btn, .threshold-btn {
  padding: 10px 20px;
  border: none;
  border-radius: 8px;
  background: #667eea;
  color: white;
  font-size: 20px;
  cursor: pointer;
  font-weight: bold;
}
.threshold-btn { background: #e74c3c; }
.speed-value, .threshold-value {
  flex: 1;
  text-align: center;
  font-size: 18px;
  font-weight: bold;
  color: #333;
}
.threshold-label {
  font-size: 14px;
  color: #666;
  display: block;
  margin-bottom: 5px;
}
</style>
</head>
<body>
<div class="container">
  <h1>🤖 ESP32 Robot Control</h1>
  <div class="status">
    <div class="status-text">Status: <span id="action">IDLE</span></div>
    <span id="modeBadge" class="mode-badge auto">AUTO MODE</span>
  </div>
  <div id="distanceBox" class="distance-box">
    📏 <span id="distance">--</span> cm
  </div>
  <div class="camera-box">
    <img id="camStream" src="http://)=====";
  page += String(camIP);
  page += R"=====(/" alt="Camera Stream">
    <div class="camera-error" id="camError">
      ⚠️ Không kết nối được camera<br>
      Kiểm tra IP: )=====";
  page += String(camIP);
  page += R"=====(
    </div>
  </div>
  <div class="controls">
    <button class="btn btn-forward" onmousedown="sendCmd('F')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('F')" ontouchend="sendCmd('S')">▲</button>
    <button class="btn btn-left" onmousedown="sendCmd('L')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('L')" ontouchend="sendCmd('S')">◄</button>
    <button class="btn btn-stop" onclick="sendCmd('S')">●</button>
    <button class="btn btn-right" onmousedown="sendCmd('R')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('R')" ontouchend="sendCmd('S')">►</button>
    <button class="btn btn-back" onmousedown="sendCmd('B')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('B')" ontouchend="sendCmd('S')">▼</button>
  </div>
  <div class="settings">
    <button class="btn-auto" onclick="sendCmd('A')">DÒ LINE</button>
    <button class="btn-manual" onclick="sendCmd('M')">ĐIỀU KHIỂN</button>
    <button class="btn-obstacle" onclick="sendCmd('O')">VẬT CẢN</button>
  </div>
  <div class="speed-control">
    <button class="speed-btn" onclick="sendCmd('DOWN')">-</button>
    <div class="speed-value">Speed: <span id="speed">170</span></div>
    <button class="speed-btn" onclick="sendCmd('UP')">+</button>
  </div>
  <div class="threshold-control">
    <button class="threshold-btn" onclick="sendCmd('TDOWN')">-</button>
    <div class="threshold-value">
      <span class="threshold-label">🚨 Ngưỡng vật cản</span>
      <span id="threshold">50</span> cm
    </div>
    <button class="threshold-btn" onclick="sendCmd('TUP')">+</button>
  </div>
</div>
<script>
const camImg = document.getElementById('camStream');
const camError = document.getElementById('camError');
camImg.onerror = function() { camError.style.display = 'block'; };
camImg.onload = function() { camError.style.display = 'none'; };

function sendCmd(cmd) {
  fetch('/cmd?action=' + cmd)
    .then(response => response.json())
    .then(data => {
      document.getElementById('action').textContent = data.action;
      document.getElementById('speed').textContent = data.speed;
      document.getElementById('threshold').textContent = data.threshold;
      updateDistance(data.distance, data.threshold);
      const badge = document.getElementById('modeBadge');
      if (data.mode === 'AUTO') {
        badge.textContent = 'AUTO LINE MODE';
        badge.className = 'mode-badge auto';
      } else if (data.mode === 'OBSTACLE') {
        badge.textContent = 'OBSTACLE AVOIDANCE';
        badge.className = 'mode-badge manual';
      } else {
        badge.textContent = 'MANUAL MODE';
        badge.className = 'mode-badge manual';
      }
    });
}

function updateDistance(dist, threshold) {
  const distEl = document.getElementById('distance');
  const boxEl = document.getElementById('distanceBox');
  distEl.textContent = dist;
  if (dist < threshold) {
    boxEl.className = 'distance-box distance-warning';
  } else {
    boxEl.className = 'distance-box';
  }
}

setInterval(() => {
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('action').textContent = data.action;
      document.getElementById('speed').textContent = data.speed;
      document.getElementById('threshold').textContent = data.threshold;
      updateDistance(data.distance, data.threshold);
    });
}, 500);
</script>
</body>
</html>
)=====";
  return page;
}

// Forward declarations
void avoidObstacle();
void brake();
void turnUp();
void turnDown();
void turnLeft();
void turnRight();
void idle();
int getDistance();
void followLine();

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("❌ SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 28);
  display.println("ESP32 Robot");
  display.display();
  delay(2000);
  
  // Kết nối WiFi
  Serial.print("Connecting to WiFi");
  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  display.clearDisplay();
  display.setCursor(5, 20);
  display.println("WiFi Connected!");
  display.setCursor(5, 35);
  display.println(WiFi.localIP());
  display.display();
  delay(2000);
  
  // Khởi tạo Servo và cảm biến
  myServo.attach(servoPin);
  myServo.write(90);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // ESP32 core v3.0 syntax
  ledcAttach(enA, 1000, 8);
  ledcAttach(enB, 1000, 8);
  ledcWrite(enA, speedPWM);
  ledcWrite(enB, speedPWM);
  
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  
  pinMode(sensorT1, INPUT_PULLUP);
  pinMode(sensorT2, INPUT_PULLUP);
  pinMode(sensorS, INPUT_PULLUP);
  pinMode(sensorP1, INPUT_PULLUP);
  pinMode(sensorP2, INPUT_PULLUP);
  
  brake();
  
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getMainPage());
  });
  server.on("/cmd", HTTP_GET, handleCommand);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  
  Serial.println("Web Server Started!");
  Serial.println("Mở trình duyệt: http://" + WiFi.localIP().toString());
  
  updateOLED();
}

void loop() {
  server.handleClient();
  
  static unsigned long lastOLEDUpdate = 0;
  currentDistance = getDistance();
  
  // Cập nhật OLED mỗi 200ms
  if (millis() - lastOLEDUpdate > 200) {
    updateOLED();
    lastOLEDUpdate = millis();
  }
  
  if (autoMode) {
    followLine();
  } else if (obstacleAvoidance) {
    if (currentDistance < distanceThreshold) {
      brake();
      delay(50);
      avoidObstacle();
    } else {
      turnUp();
      currentAction = "FORWARD";
    }
  }
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("servo")) {
      int angle = cmd.substring(5).toInt();
      if (angle >= 0 && angle <= 180) {
        myServo.write(angle);
        Serial.print("✅ Servo moved to angle: ");
        Serial.println(angle);
      } else {
        Serial.println("⚠️ Góc không hợp lệ! (0-180)");
      }
    } else if (cmd == "left") {
      myServo.write(170);
      Serial.println("↩️ Servo quay trái (170°)");
    } else if (cmd == "right") {
      myServo.write(10);
      Serial.println("↪️ Servo quay phải (10°)");
    } else if (cmd == "center") {
      myServo.write(90);
      Serial.println("🎯 Servo về giữa (90°)");
    }
  }
  
  delay(50);
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  int distance = duration * 0.034 / 2;
  
  if (distance == 0 || distance > 200) {
    return 999;
  }
  return distance;
}

void avoidObstacle() {
  currentAction = "OBSTACLE SCAN";
  Serial.println("↩️ Scanning LEFT...");
  myServo.write(170);
  delay(1000);
  int leftDist = getDistance();
  Serial.print("Left: ");
  Serial.println(leftDist);
  
  Serial.println("↪️ Scanning RIGHT...");
  myServo.write(10);
  delay(1000);
  int rightDist = getDistance();
  Serial.print("Right: ");
  Serial.println(rightDist);
  
  Serial.println("🎯 Return to CENTER");
  myServo.write(90);
  delay(1000);
  
  if (leftDist > rightDist && leftDist > distanceThreshold) {
    Serial.println("⬅️ Turning LEFT");
    currentAction = "TURN LEFT";
    turnLeft();
    delay(500);
  } else if (rightDist > distanceThreshold) {
    Serial.println("➡️ Turning RIGHT");
    currentAction = "TURN RIGHT";
    turnRight();
    delay(500);
  } else {
    Serial.println("⬇️ BACKWARD + TURN");
    currentAction = "BACKWARD";
    turnDown();
    delay(1000);
    turnRight();
    delay(1000);
  }
  
  brake();
  delay(500);
}

void handleCommand() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    
    if (action == "F") {
      autoMode = false;
      obstacleAvoidance = false;
      turnUp();
      currentAction = "FORWARD";
    } else if (action == "B") {
      autoMode = false;
      obstacleAvoidance = false;
      turnDown();
      currentAction = "BACKWARD";
    } else if (action == "L") {
      autoMode = false;
      obstacleAvoidance = false;
      turnLeft();
      currentAction = "LEFT";
    } else if (action == "R") {
      autoMode = false;
      obstacleAvoidance = false;
      turnRight();
      currentAction = "RIGHT";
    } else if (action == "S") {
      autoMode = false;
      obstacleAvoidance = false;
      brake();
      currentAction = "STOP";
    } else if (action == "A") {
      autoMode = true;
      obstacleAvoidance = false;
      turnSpeed = 200; 
      currentAction = "AUTO LINE";
    } else if (action == "M") {
      autoMode = false;
      turnSpeed = 250;
      obstacleAvoidance = false;
      brake();
      currentAction = "MANUAL";
    } else if (action == "O") {
      autoMode = false;
      turnSpeed = 250;
      obstacleAvoidance = true;
      myServo.write(90);
      brake();
      currentAction = "AVOID OBSTACLE";
    } else if (action == "UP") {
      speedPWM = min(255, speedPWM + 20);
      turnSpeed = min(255, turnSpeed + 15);
      curveSpeed = min(200, curveSpeed + 10);
      // Khi tăng tốc cũng phải áp dụng cân bằng
      ledcWrite(enA, speedPWM * leftFactor);
      ledcWrite(enB, speedPWM * rightFactor);
    } else if (action == "DOWN") {
      speedPWM = max(30, speedPWM - 20);
      turnSpeed = max(30, turnSpeed - 15);
      curveSpeed = max(50, curveSpeed - 10);
      // Khi giảm tốc cũng phải áp dụng cân bằng
      ledcWrite(enA, speedPWM * leftFactor);
      ledcWrite(enB, speedPWM * rightFactor);
    } else if (action == "TUP") {
      distanceThreshold = min(200, distanceThreshold + 5);
      Serial.print("🚨 Threshold increased to: ");
      Serial.println(distanceThreshold);
    } else if (action == "TDOWN") {
      distanceThreshold = max(10, distanceThreshold - 5);
      Serial.print("🚨 Threshold decreased to: ");
      Serial.println(distanceThreshold);
    }
    
    updateOLED();
  }
  
  String modeStr = autoMode ? "AUTO" : (obstacleAvoidance ? "OBSTACLE" : "MANUAL");
  String json = "{\"action\":\"" + currentAction + "\",\"speed\":" + String(speedPWM) + 
                ",\"distance\":" + String(currentDistance) + ",\"threshold\":" + 
                String(distanceThreshold) + ",\"mode\":\"" + modeStr + "\"}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  String modeStr = autoMode ? "AUTO" : (obstacleAvoidance ? "OBSTACLE" : "MANUAL");
  String json = "{\"action\":\"" + currentAction + "\",\"speed\":" + String(speedPWM) + 
                ",\"distance\":" + String(currentDistance) + ",\"threshold\":" + 
                String(distanceThreshold) + ",\"mode\":\"" + modeStr + "\"}";
  server.send(200, "application/json", json);
}

// ============================================
// HÀM DÒ LINE - ĐÃ TỐI ƯU + CÂN BẰNG ĐỘNG CƠ
// ============================================
void followLine() {
  // Đọc cảm biến
  int t1 = digitalRead(sensorT1);
  int t2 = digitalRead(sensorT2);
  int ss = digitalRead(sensorS);
  int p1 = digitalRead(sensorP1);
  int p2 = digitalRead(sensorP2);
  
  bool lineDetected = (t1==1 || t2==1 || ss==1 || p1==1 || p2==1);
  if (lineDetected) {
    lastLineTime = millis(); 
  }
  
  // 1. Mất line
  if (t1==0 && t2==0 && ss==0 && p1==0 && p2==0) {
    if (millis() - lastLineTime < lineTimeout) {
      if (lastTurnDirection < 0) {
        turnLeft(); 
        currentAction = "FINDING LEFT";
      } else if (lastTurnDirection > 0) {
        turnRight(); 
        currentAction = "FINDING RIGHT";
      } else {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        digitalWrite(in3, HIGH);
        digitalWrite(in4, LOW);
        // Cân bằng khi đi chậm tìm line
        ledcWrite(enA, (speedPWM * 0.4) * leftFactor);
        ledcWrite(enB, (speedPWM * 0.4) * rightFactor);
        currentAction = "FINDING FORWARD";
      }
    } else {
      idle();
      currentAction = "LINE LOST";
      lastTurnDirection = 0;
    }
  }
  
  // 2. Ngã tư (Toàn line)
  else if (t1==1 && t2==1 && ss==1 && p1==1 && p2==1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    // Cân bằng khi đi qua ngã tư
    ledcWrite(enA, curveSpeed * leftFactor);
    ledcWrite(enB, curveSpeed * rightFactor);
    currentAction = "CROSS LINE";
    lastTurnDirection = 0;
  }
  
  // 3. Lệch TRÁI NHIỀU
  else if (t1==1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);
    ledcWrite(enA, curveSpeed); 
    ledcWrite(enB, curveSpeed);
    currentAction = "SHARP LEFT";
    lastTurnDirection = -1; 
  }
  
  // 4. Lệch TRÁI VỪA
  else if (t2==1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    ledcWrite(enA, curveSpeed * 0.6); 
    ledcWrite(enB, curveSpeed); // Giữ nguyên bên phải để đẩy sang trái
    currentAction = "SOFT LEFT";
    lastTurnDirection = -1;
  }
  
  // 5. Lệch PHẢI NHIỀU
  else if (p2==1) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    ledcWrite(enA, curveSpeed); 
    ledcWrite(enB, curveSpeed);
    currentAction = "SHARP RIGHT";
    lastTurnDirection = 1; 
  }
  
  // 6. Lệch PHẢI VỪA
  else if (p1==1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    ledcWrite(enA, curveSpeed);
    ledcWrite(enB, curveSpeed * 0.6); 
    currentAction = "SOFT RIGHT";
    lastTurnDirection = 1;
  }
  
  // 7. LINE Ở GIỮA -> CHẠY THẲNG (Đã sửa để cân bằng)
  else if (ss==1) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    
    // *** QUAN TRỌNG: Cân bằng tốc độ tại đây ***
    ledcWrite(enA, speedPWM * leftFactor);
    ledcWrite(enB, speedPWM * rightFactor);
    
    currentAction = "FORWARD";
    lastTurnDirection = 0;
  }
  
  else {
    idle();
    currentAction = "IDLE";
    lastTurnDirection = 0;
  }
}

void idle() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  ledcWrite(enA, 0);
  ledcWrite(enB, 0);
}

void brake() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, HIGH);
}

// ĐI THẲNG - CÓ CÂN BẰNG
void turnUp() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  ledcWrite(enA, speedPWM * leftFactor);
  ledcWrite(enB, speedPWM * rightFactor);
}

void turnDown() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  ledcWrite(enA, speedPWM * leftFactor);
  ledcWrite(enB, speedPWM * rightFactor);
}

void turnLeft() {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  ledcWrite(enA, turnSpeed);
  ledcWrite(enB, turnSpeed);
}

void turnRight() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  // Vì bánh trái mạnh hơn nên khi rẽ phải (dùng bánh trái) cũng nên giảm nhẹ
  ledcWrite(enA, turnSpeed * leftFactor);
  ledcWrite(enB, turnSpeed);
}