#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ================== Wi-Fi Access Point ==================
const char* ssid = "ESP32_Car_Controller";
const char* password = "12345678";

// ================== Web Server ==================
WebServer server(80);

// ================== Brushless Motor (ESC) ==================
Servo esc;
int escPin = 4;
int throttle = 0; // 0–100%

// ================== TB6612FNG Motor Driver ==================
#define AIN1 13
#define AIN2 14
#define PWMA 26
#define BIN1 12
#define BIN2 27
#define PWMB 25
#define STBY 33

// ================== Brush Servos ==================
Servo brushServo1;
Servo brushServo2;
#define BRUSH_SERVO1_PIN 16
#define BRUSH_SERVO2_PIN 17
bool brushActive = false;

// ================== Setup ==================
void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // ----- ESC Setup -----
  esc.setPeriodHertz(50);
  esc.attach(escPin, 1000, 2000);
  esc.writeMicroseconds(1000); // minimum throttle
  delay(3000);
  Serial.println("ESC Armed!");

  // ----- TB6612FNG Setup -----
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH); // enable motor driver

  // ----- Brush Servo Setup -----
  brushServo1.attach(BRUSH_SERVO1_PIN);
  brushServo2.attach(BRUSH_SERVO2_PIN);
  brushServo1.writeMicroseconds(1500); // stop position
  brushServo2.writeMicroseconds(1500);

  // ----- WiFi Setup -----
  WiFi.softAP(ssid, password);
  Serial.print("Wi-Fi started. Connect to: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // ----- Web Routes -----
  server.on("/", handleRoot);
  server.on("/set", handleSet);       // Brushless speed
  server.on("/move", handleMove);     // Car movement
  server.on("/brush", handleBrush);   // Brush toggle

  server.begin();
  Serial.println("Web Server Running!");
}

void loop() {
  server.handleClient();
}

// ================== Webpage ==================
void handleRoot() {
  String html = R"=====(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Car + Brushless + Brush Servo</title>
  <style>
    body { font-family: Arial; text-align:center; background:#eef; padding:30px; }
    h2 { color:#333; }
    button { width:120px; height:50px; font-size:18px; margin:8px; border:none; border-radius:10px; background:#0078D7; color:white; }
    button:hover { background:#005fa3; }
    input[type=range] { width:80%; height:40px; margin-top:15px; }
    .val { font-size:1.3em; color:#0078D7; }
    .grid { display:grid; grid-template-columns: 1fr 1fr 1fr; justify-items:center; margin:20px auto; max-width:400px; }
  </style>
</head>
<body>
  <h2>🚗 ESP32 Car + 🌀 Brushless + 🧹 Brush Servo</h2>

  <div class="grid">
    <div></div>
    <button onclick="move('forward')">↑ Forward</button>
    <div></div>

    <button onclick="move('left')">← Left</button>
    <button onclick="move('stop')">⏹ Stop</button>
    <button onclick="move('right')">→ Right</button>

    <div></div>
    <button onclick="move('backward')">↓ Back</button>
    <div></div>
  </div>

  <hr>
  <h3>Brushless Motor Control</h3>
  <input type="range" min="0" max="100" value="0" id="slider" oninput="update(this.value)">
  <p>Throttle: <span id="val" class="val">0</span>%</p>

  <hr>
  <h3>🧹 Brush Servos</h3>
  <button onclick="toggleBrush()">Brush ON/OFF</button>

<script>
function move(dir){
  fetch('/move?dir='+dir);
}
function update(v){
  document.getElementById('val').innerText=v;
  fetch('/set?value='+v);
}
function toggleBrush(){
  fetch('/brush');
}
</script>
</body>
</html>)=====";

  server.send(200, "text/html", html);
}

// ================== Handle Brushless Speed ==================
void handleSet() {
  if (server.hasArg("value")) {
    throttle = constrain(server.arg("value").toInt(), 0, 100);
    int pulseWidth = map(throttle, 0, 100, 1000, 2000);
    esc.writeMicroseconds(pulseWidth);
    Serial.printf("Brushless Throttle: %d%% -> %dus\n", throttle, pulseWidth);
  }
  server.send(200, "text/plain", "OK");
}

// ================== Handle Car Movement ==================
void handleMove() {
  if (!server.hasArg("dir")) {
    server.send(400, "text/plain", "Missing direction");
    return;
  }

  String dir = server.arg("dir");
  dir.toLowerCase();

  if (dir == "forward")      moveForward(200);
  else if (dir == "backward") moveBackward(200);
  else if (dir == "left")     turnLeft(200);
  else if (dir == "right")    turnRight(200);
  else                        stopMotors();

  Serial.println("Car direction: " + dir);
  server.send(200, "text/plain", "OK");
}

// ================== Handle Brush Toggle ==================
void handleBrush() {
  brushActive = !brushActive;

  if (brushActive) {
    brushServo1.writeMicroseconds(1700); // forward continuous spin
    brushServo2.writeMicroseconds(1700);
    Serial.println("Brush Servos: ON");
  } else {
    brushServo1.writeMicroseconds(1500); // stop
    brushServo2.writeMicroseconds(1500);
    Serial.println("Brush Servos: OFF");
  }

  server.send(200, "text/plain", "OK");
}

// ================== Car Motor Functions ==================
void moveForward(int speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, speed);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, speed);
}

void moveBackward(int speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, speed);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, speed);
}

void turnLeft(int speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, speed);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, speed);
}

void turnRight(int speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, speed);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, speed);
}

void stopMotors() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 0);
}