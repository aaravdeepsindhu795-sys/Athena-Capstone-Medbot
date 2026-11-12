// MEDBOT ROVER - 4WD BTS7960 + ESP32-S3 - WiFi joystick control
// ESP32 Arduino core 3.x   |   Serial Monitor: 115200
// WiFi AP: "MedbotRover" / "12345678"  ->  open http://192.168.4.1

#include <WiFi.h>
#include <WebServer.h>

// ---------- Motor table (matches your physical wiring) ----------
struct Motor {
  const char* name;
  int ren, len, rpwm, lpwm;
  bool invert;            // true if RPWM spins this wheel backward
};

Motor motors[] = {
  //  name  R_EN L_EN RPWM LPWM invert
  {"FL",   1,   2,   4,   5,  false},
  {"FR",  10,  11,  12,  13,  false },
  {"RL",   6,   7,   8,   9,  false},
  {"RR",  15,  16,  17,  18,  true }
};
const int MOTOR_COUNT = 4;
const int FL = 0, FR = 1, RL = 2, RR = 3;

// ---------- Settings ----------
const uint32_t PWM_FREQ = 20000;
const uint8_t  PWM_RES  = 8;
const char* AP_SSID = "MedbotRover";
const char* AP_PASS = "12345678";

const unsigned long FAILSAFE_MS = 500;
const float DEADZONE   = 0.08;
const float RAMP_STEP  = 0.06;
const unsigned long RAMP_INTERVAL_MS = 20;

WebServer server(80);

float targetL = 0, targetR = 0;
float curL = 0, curR = 0;
unsigned long lastCmd = 0;
unsigned long lastRamp = 0;

// ---------- Motor control ----------
void setMotor(int i, float speed) {
  speed = constrain(speed, -1.0f, 1.0f);
  if (motors[i].invert) speed = -speed;
  int duty = (int)(fabs(speed) * 255.0f);
  if (speed >= 0) {
    ledcWrite(motors[i].lpwm, 0);
    ledcWrite(motors[i].rpwm, duty);
  } else {
    ledcWrite(motors[i].rpwm, 0);
    ledcWrite(motors[i].lpwm, duty);
  }
}

void applySides(float l, float r) {
  setMotor(FL, l); setMotor(RL, l);
  setMotor(FR, r); setMotor(RR, r);
}

void stopNow() {
  targetL = targetR = curL = curR = 0;
  applySides(0, 0);
}

float stepToward(float cur, float tgt) {
  if (cur < tgt) return min(cur + RAMP_STEP, tgt);
  if (cur > tgt) return max(cur - RAMP_STEP, tgt);
  return cur;
}

// ---------- Web page ----------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Medbot Rover</title>
<style>
 body{margin:0;background:#111;color:#eee;font-family:sans-serif;text-align:center;touch-action:none;user-select:none}
 #pad{width:260px;height:260px;border-radius:50%;background:#222;margin:25px auto;position:relative;border:2px solid #444}
 #knob{width:90px;height:90px;border-radius:50%;background:#3a8;position:absolute;left:85px;top:85px}
 input[type=range]{width:80%}
 button{background:#c33;color:#fff;border:0;border-radius:8px;padding:14px 40px;font-size:18px;margin:10px}
</style></head><body>
<h2>Medbot Rover</h2>
<div id="pad"><div id="knob"></div></div>
<p id="s">x:0.00 y:0.00</p>
<p>Max speed: <span id="sv">60</span>%<br>
<input id="spd" type="range" min="10" max="100" value="60"></p>
<button id="stop">STOP</button>
<script>
const pad=document.getElementById('pad'),knob=document.getElementById('knob'),
      s=document.getElementById('s'),spd=document.getElementById('spd'),sv=document.getElementById('sv');
let x=0,y=0,active=false;
spd.oninput=()=>{sv.textContent=spd.value;};
function upd(e){
  const r=pad.getBoundingClientRect(),t=e.touches?e.touches[0]:e;
  let dx=(t.clientX-r.left-130)/130, dy=(t.clientY-r.top-130)/130;
  const m=Math.hypot(dx,dy); if(m>1){dx/=m;dy/=m;}
  x=dx; y=-dy;
  knob.style.left=(85+dx*85)+'px'; knob.style.top=(85+dy*85)+'px';
}
function end(){active=false;x=0;y=0;knob.style.left='85px';knob.style.top='85px';send();}
pad.addEventListener('touchstart',e=>{active=true;upd(e);e.preventDefault()});
pad.addEventListener('touchmove',e=>{upd(e);e.preventDefault()});
pad.addEventListener('touchend',end);
pad.addEventListener('touchcancel',end);
pad.addEventListener('mousedown',e=>{active=true;upd(e)});
window.addEventListener('mousemove',e=>{if(active)upd(e)});
window.addEventListener('mouseup',()=>{if(active)end();});
document.getElementById('stop').onclick=()=>{end();fetch('/stop').catch(()=>{});};
function send(){
  const k=spd.value/100, sx=x*k, sy=y*k;
  s.textContent='x:'+sx.toFixed(2)+' y:'+sy.toFixed(2);
  fetch('/drive?x='+sx.toFixed(2)+'&y='+sy.toFixed(2)).catch(()=>{});
}
setInterval(()=>{if(active)send();},100);
</script></body></html>
)rawliteral";

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(motors[i].ren, OUTPUT);
    pinMode(motors[i].len, OUTPUT);
    digitalWrite(motors[i].ren, LOW);
    digitalWrite(motors[i].len, LOW);
    ledcAttach(motors[i].rpwm, PWM_FREQ, PWM_RES);
    ledcAttach(motors[i].lpwm, PWM_FREQ, PWM_RES);
    ledcWrite(motors[i].rpwm, 0);
    ledcWrite(motors[i].lpwm, 0);
  }
  delay(300);
  for (int i = 0; i < MOTOR_COUNT; i++) {
    digitalWrite(motors[i].ren, HIGH);
    digitalWrite(motors[i].len, HIGH);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("WiFi AP: "); Serial.println(AP_SSID);
  Serial.print("Open http://"); Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });

  server.on("/drive", []() {
    float x = server.arg("x").toFloat();   // turn: + = right
    float y = server.arg("y").toFloat();   // forward: + = up
    if (fabs(x) < DEADZONE) x = 0;
    if (fabs(y) < DEADZONE) y = 0;

    // Mixing with scaling (keeps turn ratio instead of clipping)
    float l = y + x;
    float r = y - x;
    float m = max(fabs(l), fabs(r));
    if (m > 1.0f) { l /= m; r /= m; }
    targetL = l;
    targetR = r;

    lastCmd = millis();
    server.send(200, "text/plain", "ok");
  });

  server.on("/stop", []() {
    stopNow();
    server.send(200, "text/plain", "stopped");
  });

  server.begin();
  stopNow();
  lastCmd = millis();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();

  if (millis() - lastCmd > FAILSAFE_MS) {
    if (curL != 0 || curR != 0 || targetL != 0 || targetR != 0) stopNow();
    return;
  }

  if (millis() - lastRamp >= RAMP_INTERVAL_MS) {
    lastRamp = millis();
    curL = stepToward(curL, targetL);
    curR = stepToward(curR, targetR);
    applySides(curL, curR);
  }
}
