#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// WIFI ACCESS POINT
// ============================================================

const char* AP_SSID = "ESP32_ROVER";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

// ============================================================
// BTS7960 PIN MAP
// ============================================================

// Front Left
const int FL_REN  = 1;
const int FL_LEN  = 2;
const int FL_RPWM = 4;
const int FL_LPWM = 5;

// Front Right
const int FR_REN  = 6;
const int FR_LEN  = 7;
const int FR_RPWM = 8;
const int FR_LPWM = 9;

// Rear Left
const int RL_REN  = 10;
const int RL_LEN  = 11;
const int RL_RPWM = 12;
const int RL_LPWM = 13;

// Rear Right
const int RR_REN  = 14;
const int RR_LEN  = 15;
const int RR_RPWM = 16;
const int RR_LPWM = 17;

// ============================================================
// PWM
// ============================================================

const int PWM_FREQUENCY = 1000;
const int PWM_RESOLUTION = 8;

// ============================================================
// SPEED
// ============================================================

int roverSpeed = 160;

String currentMotion = "STOP";

unsigned long lastCommand = 0;

const unsigned long SAFETY_TIMEOUT = 1500;

// ============================================================
// MOTOR SETUP
// ============================================================

void setupMotor(
  int ren,
  int len,
  int rpwm,
  int lpwm
) {

  pinMode(ren, OUTPUT);
  pinMode(len, OUTPUT);

  digitalWrite(ren, HIGH);
  digitalWrite(len, HIGH);

  // Arduino ESP32 Core 3.x
  ledcAttach(
    rpwm,
    PWM_FREQUENCY,
    PWM_RESOLUTION
  );

  ledcAttach(
    lpwm,
    PWM_FREQUENCY,
    PWM_RESOLUTION
  );

  ledcWrite(rpwm, 0);
  ledcWrite(lpwm, 0);
}

// ============================================================
// STOP ONE MOTOR
// ============================================================

void stopMotor(
  int rpwm,
  int lpwm
) {

  ledcWrite(rpwm, 0);
  ledcWrite(lpwm, 0);
}

// ============================================================
// FORWARD ONE MOTOR
// ============================================================

void forwardMotor(
  int rpwm,
  int lpwm,
  int speed
) {

  ledcWrite(rpwm, speed);
  ledcWrite(lpwm, 0);
}

// ============================================================
// REVERSE ONE MOTOR
// ============================================================

void reverseMotor(
  int rpwm,
  int lpwm,
  int speed
) {

  ledcWrite(rpwm, 0);
  ledcWrite(lpwm, speed);
}

// ============================================================
// STOP ALL
// ============================================================

void stopAllMotors() {

  stopMotor(FL_RPWM, FL_LPWM);
  stopMotor(FR_RPWM, FR_LPWM);
  stopMotor(RL_RPWM, RL_LPWM);
  stopMotor(RR_RPWM, RR_LPWM);

  currentMotion = "STOP";
}

// ============================================================
// FORWARD
// ============================================================

void roverForward() {

  forwardMotor(
    FL_RPWM,
    FL_LPWM,
    roverSpeed
  );

  forwardMotor(
    FR_RPWM,
    FR_LPWM,
    roverSpeed
  );

  forwardMotor(
    RL_RPWM,
    RL_LPWM,
    roverSpeed
  );

  forwardMotor(
    RR_RPWM,
    RR_LPWM,
    roverSpeed
  );

  currentMotion = "FORWARD";
}

// ============================================================
// REVERSE
// ============================================================

void roverReverse() {

  reverseMotor(
    FL_RPWM,
    FL_LPWM,
    roverSpeed
  );

  reverseMotor(
    FR_RPWM,
    FR_LPWM,
    roverSpeed
  );

  reverseMotor(
    RL_RPWM,
    RL_LPWM,
    roverSpeed
  );

  reverseMotor(
    RR_RPWM,
    RR_LPWM,
    roverSpeed
  );

  currentMotion = "REVERSE";
}

// ============================================================
// LEFT
// ============================================================

void roverLeft() {

  reverseMotor(
    FL_RPWM,
    FL_LPWM,
    roverSpeed
  );

  reverseMotor(
    RL_RPWM,
    RL_LPWM,
    roverSpeed
  );

  forwardMotor(
    FR_RPWM,
    FR_LPWM,
    roverSpeed
  );

  forwardMotor(
    RR_RPWM,
    RR_LPWM,
    roverSpeed
  );

  currentMotion = "LEFT";
}

// ============================================================
// RIGHT
// ============================================================

void roverRight() {

  forwardMotor(
    FL_RPWM,
    FL_LPWM,
    roverSpeed
  );

  forwardMotor(
    RL_RPWM,
    RL_LPWM,
    roverSpeed
  );

  reverseMotor(
    FR_RPWM,
    FR_LPWM,
    roverSpeed
  );

  reverseMotor(
    RR_RPWM,
    RR_LPWM,
    roverSpeed
  );

  currentMotion = "RIGHT";
}

// ============================================================
// WEBPAGE
// ============================================================

const char MAIN_PAGE[] PROGMEM = R"HTML(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>ESP32 Rover</title>

<style>

body {
  background:#111;
  color:white;
  font-family:Arial;
  text-align:center;
  margin:0;
}

.container {
  max-width:500px;
  margin:auto;
  padding:20px;
}

h1 {
  font-size:28px;
}

.status {
  background:#222;
  border-radius:15px;
  padding:15px;
  margin-bottom:20px;
}

#motion {
  font-size:28px;
  font-weight:bold;
  margin:10px;
}

.connected {
  color:#00ff66;
}

.speedBox {
  background:#222;
  padding:15px;
  border-radius:15px;
  margin-bottom:20px;
}

input {
  width:90%;
}

.speedValue {
  font-size:22px;
  margin-top:10px;
}

.controls {
  display:grid;
  grid-template-columns:1fr 1fr 1fr;
  gap:12px;
}

button {
  height:80px;
  border:none;
  border-radius:18px;
  background:#333;
  color:white;
  font-size:25px;
  font-weight:bold;
  touch-action:none;
}

button:active {
  transform:scale(.95);
}

.forward {
  grid-column:2;
}

.left {
  grid-column:1;
  grid-row:2;
}

.stop {
  grid-column:2;
  grid-row:2;
  background:#c00020;
}

.right {
  grid-column:3;
  grid-row:2;
}

.reverse {
  grid-column:2;
  grid-row:3;
}

.motors {
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:10px;
  margin-top:20px;
}

.motor {
  background:#222;
  border-radius:12px;
  padding:15px;
}

</style>

</head>

<body>

<div class="container">

<h1>🚙 ESP32-S3 ROVER</h1>

<div class="status">

<div>Motion</div>

<div id="motion">
STOP
</div>

<div class="connected">
● ESP32 Connected
</div>

</div>

<div class="speedBox">

<h3>Speed</h3>

<input
type="range"
min="0"
max="255"
value="160"
oninput="setSpeed(this.value)">

<div class="speedValue">

<span id="speed">
63
</span>%

</div>

</div>

<div class="controls">

<button
class="forward"
onpointerdown="start('forward')"
onpointerup="stop()"
onpointercancel="stop()">
▲
</button>

<button
class="left"
onpointerdown="start('left')"
onpointerup="stop()"
onpointercancel="stop()">
◀
</button>

<button
class="stop"
onclick="send('stop')">
STOP
</button>

<button
class="right"
onpointerdown="start('right')"
onpointerup="stop()"
onpointercancel="stop()">
▶
</button>

<button
class="reverse"
onpointerdown="start('reverse')"
onpointerup="stop()"
onpointercancel="stop()">
▼
</button>

</div>

<div class="motors">

<div class="motor">
<b>FL</b><br>
Front Left<br>
<span id="fl">STOP</span>
</div>

<div class="motor">
<b>FR</b><br>
Front Right<br>
<span id="fr">STOP</span>
</div>

<div class="motor">
<b>RL</b><br>
Rear Left<br>
<span id="rl">STOP</span>
</div>

<div class="motor">
<b>RR</b><br>
Rear Right<br>
<span id="rr">STOP</span>
</div>

</div>

</div>

<script>

let timer = null;

function send(command) {

  fetch("/cmd?move=" + command);

  let state =
    command.toUpperCase();

  document.getElementById("motion")
    .innerText = state;

  document.getElementById("fl")
    .innerText = state;

  document.getElementById("fr")
    .innerText = state;

  document.getElementById("rl")
    .innerText = state;

  document.getElementById("rr")
    .innerText = state;
}

function start(command) {

  send(command);

  if (timer)
    clearInterval(timer);

  timer = setInterval(
    function() {
      send(command);
    },
    300
  );
}

function stop() {

  if (timer) {

    clearInterval(timer);

    timer = null;
  }

  send("stop");
}

function setSpeed(value) {

  let percent =
    Math.round(
      value * 100 / 255
    );

  document.getElementById("speed")
    .innerText = percent;

  fetch(
    "/speed?value=" + value
  );
}

</script>

</body>

</html>

)HTML";

// ============================================================
// WEB ROOT
// ============================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    MAIN_PAGE
  );
}

// ============================================================
// COMMAND HANDLER
// ============================================================

void handleCommand() {

  if (!server.hasArg("move")) {

    server.send(
      400,
      "text/plain",
      "Missing move command"
    );

    return;
  }

  String command =
    server.arg("move");

  lastCommand = millis();

  if (command == "forward") {

    roverForward();

  } else if (command == "reverse") {

    roverReverse();

  } else if (command == "left") {

    roverLeft();

  } else if (command == "right") {

    roverRight();

  } else {

    stopAllMotors();
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SPEED HANDLER
// ============================================================

void handleSpeed() {

  if (!server.hasArg("value")) {

    server.send(
      400,
      "text/plain",
      "Missing speed"
    );

    return;
  }

  roverSpeed =
    constrain(
      server.arg("value").toInt(),
      0,
      255
    );

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// 404
// ============================================================

void handleNotFound() {

  server.send(
    404,
    "text/plain",
    "Page not found"
  );
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "================================="
  );
  Serial.println(
    " ESP32-S3 4WD ROVER"
  );
  Serial.println(
    " 4 x BTS7960"
  );
  Serial.println(
    " WiFi ACCESS POINT"
  );
  Serial.println(
    "================================="
  );


  // ----------------------------------------------------------
  // MOTORS
  // ----------------------------------------------------------

  setupMotor(
    FL_REN,
    FL_LEN,
    FL_RPWM,
    FL_LPWM
  );

  setupMotor(
    FR_REN,
    FR_LEN,
    FR_RPWM,
    FR_LPWM
  );

  setupMotor(
    RL_REN,
    RL_LEN,
    RL_RPWM,
    RL_LPWM
  );

  setupMotor(
    RR_REN,
    RR_LEN,
    RR_RPWM,
    RR_LPWM
  );

  stopAllMotors();


  Serial.println(
    "Motors initialized"
  );


  // ----------------------------------------------------------
  // WIFI ACCESS POINT
  // ----------------------------------------------------------

  WiFi.mode(WIFI_AP);

  delay(500);

  bool result =
    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );

  if (result) {

    Serial.println();
    Serial.println(
      "WiFi AP started"
    );

    Serial.print(
      "SSID: "
    );

    Serial.println(
      AP_SSID
    );

    Serial.print(
      "Password: "
    );

    Serial.println(
      AP_PASSWORD
    );

    Serial.print(
      "IP: "
    );

    Serial.println(
      WiFi.softAPIP()
    );

  } else {

    Serial.println(
      "ERROR: WiFi AP FAILED"
    );
  }


  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/cmd",
    HTTP_GET,
    handleCommand
  );

  server.on(
    "/speed",
    HTTP_GET,
    handleSpeed
  );

  server.onNotFound(
    handleNotFound
  );

  server.begin();


  Serial.println();
  Serial.println(
    "Web server started"
  );

  Serial.println();
  Serial.println(
    "Connect to:"
  );

  Serial.println(
    "ESP32_ROVER"
  );

  Serial.println(
    "Password: 12345678"
  );

  Serial.println();
  Serial.println(
    "Open:"
  );

  Serial.println(
    "http://192.168.4.1"
  );

  Serial.println(
    "================================="
  );

  lastCommand = millis();
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  server.handleClient();


  // ----------------------------------------------------------
  // SAFETY STOP
  // ----------------------------------------------------------

  if (
    currentMotion != "STOP" &&
    millis() - lastCommand >
    SAFETY_TIMEOUT
  ) {

    stopAllMotors();

    Serial.println(
      "SAFETY STOP"
    );
  }
}