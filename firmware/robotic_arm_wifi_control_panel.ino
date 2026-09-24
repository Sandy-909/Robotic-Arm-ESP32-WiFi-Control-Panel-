/*
  6-Axis Robotic Arm - WiFi Web Control Panel - ESP32
  -------------------------------------------------------------------------
  ESP32 hosts its OWN WiFi network (Access Point mode). Connect your
  phone/laptop to that network, open the IP in a browser, and you get
  a full control panel: sliders, Save Position, Play Movements, Stop,
  Export/Import Positions (as a file), and Reset -- all running purely
  over WiFi, no USB/serial connection needed once it's powered on.

  WIRING:
    Update SERVO_PINS[] below to match your actual joint -> GPIO wiring.
    Label each joint in JOINT_NAMES[] to match your arm (Base, Shoulder, etc).
    All servos need a separate 5-6V supply (2A+ per servo) with GND
    common to the ESP32.

  SETUP:
    1. Install "ESP32Servo" library (Kevin Harrington) via Library Manager.
    2. Change WIFI_SSID / WIFI_PASSWORD below if you want.
    3. Upload, open Serial Monitor at 115200 to see the IP address.
    4. On your phone/laptop, join the WiFi network shown, then open that
       IP in any browser (e.g. http://192.168.4.1).
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---------- CONFIG: EDIT THESE FOR YOUR ARM ----------
const int SERVO_PINS[6]     = {13, 14, 27, 26, 25, 33};
const char* JOINT_NAMES[6]  = {"Base", "Shoulder", "Elbow", "Wrist Pitch", "Wrist Roll", "Gripper"};
const int DEFAULT_ANGLE[6]  = {90, 90, 90, 90, 90, 90};

const char* WIFI_SSID     = "RoboticArm";
const char* WIFI_PASSWORD = "arm12345";   // must be 8+ characters

// ---------- SPEED CONTROL (per joint) ----------
// Lower STEP_SIZE and/or higher STEP_INTERVAL_MS = slower, smoother movement.
// Higher STEP_SIZE and/or lower STEP_INTERVAL_MS = faster movement.
// Set one value per joint, in the same order as SERVO_PINS/JOINT_NAMES above.
// Example below: Base/Shoulder/Elbow are MG995 (slower, more torque),
// Wrist Pitch/Wrist Roll/Gripper are SG90 (faster, confirmed working at 2 steps / 30ms).
const int STEP_SIZE[6]         = {1,  1,  1,  2,  2,  2};   // degrees moved per step
const int STEP_INTERVAL_MS[6]  = {15, 15, 15, 30, 30, 30};  // milliseconds between steps
// ------------------------------------------------------

Servo joints[6];
int currentAngle[6];   // angle the servo is actually at right now
int targetAngle[6];    // angle we're ramping toward
unsigned long lastStepTime[6];  // tracked per servo since speeds now differ
WebServer server(80);

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Arm Control Panel</title><style>";
  html += ":root{--bg:#0d0d0d;--panel:#1a1a1a;--accent:#c0273f;--accent-hover:#d63a53;--text:#eee;--muted:#999;}";
  html += "*{box-sizing:border-box;}";
  html += "body{background:var(--bg);color:var(--text);font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;}";
  html += "h1{text-align:center;font-weight:300;letter-spacing:5px;font-size:20px;margin-bottom:22px;text-transform:uppercase;}";
  html += ".layout{display:flex;gap:16px;max-width:1100px;margin:0 auto;flex-wrap:wrap;}";
  html += ".col{display:flex;flex-direction:column;gap:8px;}";
  html += ".col-side{width:200px;}";
  html += ".col-main{flex:1;min-width:280px;}";
  html += "button,select{background:var(--panel);color:var(--text);border:1px solid #333;border-radius:6px;padding:12px 10px;font-size:13px;letter-spacing:1px;cursor:pointer;text-transform:uppercase;width:100%;}";
  html += "button:hover{background:#262626;}";
  html += "button:disabled{opacity:0.4;cursor:not-allowed;}";
  html += ".primary{background:var(--accent);border-color:var(--accent);}";
  html += ".primary:hover{background:var(--accent-hover);}";
  html += ".primary.playing{background:var(--accent-hover);}";
  html += ".joint{background:var(--panel);border-radius:8px;padding:10px 14px;}";
  html += ".joint-label{display:flex;justify-content:space-between;font-size:13px;margin-bottom:6px;}";
  html += ".joint-label .val{color:#6fb3ff;font-weight:bold;}";
  html += "input[type=range]{width:100%;accent-color:var(--accent);height:20px;}";
  html += ".delay-box{display:flex;align-items:center;background:var(--panel);border-radius:6px;padding:8px 10px;gap:8px;}";
  html += ".delay-box input{background:transparent;border:none;color:var(--text);width:100%;font-size:13px;}";
  html += ".delay-box label{font-size:11px;color:var(--muted);white-space:nowrap;}";
  html += ".status{text-align:center;margin-top:16px;font-size:12px;letter-spacing:2px;color:var(--muted);text-transform:uppercase;}";
  html += ".status.active{color:var(--accent-hover);}";
  html += ".saved-list{background:var(--panel);border-radius:8px;padding:8px 12px;max-height:150px;overflow-y:auto;font-size:12px;}";
  html += ".pose-row{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid #262626;}";
  html += ".pose-row:last-child{border-bottom:none;}";
  html += ".pose-row button{width:auto;padding:3px 7px;font-size:10px;}";
  html += ".empty-note{color:var(--muted);font-size:11px;text-align:center;padding:8px 0;}";
  html += "#importFile{display:none;}";
  html += "</style></head><body>";
  html += "<h1>Arm Control Panel</h1><div class='layout'>";

  // Left column
  html += "<div class='col col-side'>";
  html += "<button id='saveBtn'>Save Position</button>";
  html += "<button id='playBtn' class='primary'>Play Movements</button>";
  html += "<button id='stopBtn'>Stop Movement</button>";
  html += "<div class='delay-box'><label>Delay (ms)</label><input type='number' id='delayInput' value='1000' min='50' step='50'></div>";
  html += "<div class='saved-list' id='savedList'><div class='empty-note'>No saved positions yet</div></div>";
  html += "</div>";

  // Middle column: sliders
  html += "<div class='col col-main' id='jointContainer'>";
  for (int i = 0; i < 6; i++) {
    html += "<div class='joint'>";
    html += "<div class='joint-label'><span>" + String(JOINT_NAMES[i]) + "</span><span class='val' id='v" + i + "'>" + String(currentAngle[i]) + "</span></div>";
    html += "<input type='range' min='0' max='180' value='" + String(currentAngle[i]) + "' id='s" + i + "' oninput='onSlide(" + String(i) + ",this.value)'>";
    html += "</div>";
  }
  html += "</div>";

  // Right column
  html += "<div class='col col-side'>";
  html += "<button id='exportBtn'>Export Positions</button>";
  html += "<button id='importBtn'>Import Positions</button>";
  html += "<input type='file' id='importFile' accept='application/json'>";
  html += "<button id='resetBtn'>Reset Positions</button>";
  html += "</div>";

  html += "</div><div class='status active' id='statusLine'>Connected via WiFi</div>";

  // JS
  html += "<script>";
  html += "let angles=[" ;
  for (int i = 0; i < 6; i++) { html += String(currentAngle[i]); if (i < 5) html += ","; }
  html += "];";
  html += "let savedPoses=[];let playing=false;let stopRequested=false;";
  html += "const playBtn=document.getElementById('playBtn');const statusLine=document.getElementById('statusLine');const savedList=document.getElementById('savedList');";

  html += "function setStatus(t,a){statusLine.innerText=t;statusLine.classList.toggle('active',!!a);}";

  html += "function onSlide(i,v){angles[i]=parseInt(v);document.getElementById('v'+i).innerText=v;sendAngles();}";

  html += "function updateSliders(){angles.forEach((a,i)=>{document.getElementById('s'+i).value=a;document.getElementById('v'+i).innerText=a;});}";

  html += "async function sendAngles(){";
  html += "let url='/moveAll?';";
  html += "angles.forEach((a,i)=>{url+='a'+i+'='+a+'&';});";
  html += "try{await fetch(url);}catch(e){setStatus('Send failed -- check WiFi connection');}";
  html += "}";

  html += "function renderSavedList(){";
  html += "if(savedPoses.length===0){savedList.innerHTML='<div class=\"empty-note\">No saved positions yet</div>';return;}";
  html += "savedList.innerHTML='';";
  html += "savedPoses.forEach((pose,idx)=>{";
  html += "const row=document.createElement('div');row.className='pose-row';";
  html += "row.innerHTML='<span>Pose '+(idx+1)+': '+pose.join(', ')+'</span>';";
  html += "const delBtn=document.createElement('button');delBtn.innerText='X';";
  html += "delBtn.onclick=()=>{savedPoses.splice(idx,1);renderSavedList();};";
  html += "row.appendChild(delBtn);savedList.appendChild(row);";
  html += "});}";

  html += "function sleep(ms){return new Promise(r=>setTimeout(r,ms));}";

  html += "document.getElementById('saveBtn').addEventListener('click',()=>{savedPoses.push([...angles]);renderSavedList();});";

  html += "document.getElementById('playBtn').addEventListener('click',async()=>{";
  html += "if(playing)return;";
  html += "if(savedPoses.length===0){setStatus('No saved positions to play');return;}";
  html += "playing=true;stopRequested=false;playBtn.classList.add('playing');";
  html += "const delayMs=parseInt(document.getElementById('delayInput').value)||1000;";
  html += "for(let i=0;i<savedPoses.length;i++){";
  html += "if(stopRequested)break;";
  html += "angles=[...savedPoses[i]];updateSliders();await sendAngles();";
  html += "setStatus('Playing movement '+(i+1)+' of '+savedPoses.length,true);";
  html += "await sleep(delayMs);}";
  html += "playing=false;playBtn.classList.remove('playing');";
  html += "setStatus(stopRequested?'Stopped':'Playback finished',true);";
  html += "});";

  html += "document.getElementById('stopBtn').addEventListener('click',()=>{stopRequested=true;setStatus('Stopping...');});";

  html += "document.getElementById('exportBtn').addEventListener('click',()=>{";
  html += "const blob=new Blob([JSON.stringify(savedPoses,null,2)],{type:'application/json'});";
  html += "const url=URL.createObjectURL(blob);const a=document.createElement('a');";
  html += "a.href=url;a.download='arm_positions.json';a.click();URL.revokeObjectURL(url);";
  html += "});";

  html += "document.getElementById('importBtn').addEventListener('click',()=>{document.getElementById('importFile').click();});";

  html += "document.getElementById('importFile').addEventListener('change',(e)=>{";
  html += "const file=e.target.files[0];if(!file)return;";
  html += "const r=new FileReader();";
  html += "r.onload=(evt)=>{try{const data=JSON.parse(evt.target.result);if(Array.isArray(data)){savedPoses=data;renderSavedList();setStatus('Positions imported',true);}}catch(err){setStatus('Invalid file');}};";
  html += "r.readAsText(file);";
  html += "});";

  html += "document.getElementById('resetBtn').addEventListener('click',()=>{";
  html += "angles=[" ;
  for (int i = 0; i < 6; i++) { html += String(DEFAULT_ANGLE[i]); if (i < 5) html += ","; }
  html += "];updateSliders();sendAngles();";
  html += "});";

  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleMoveAll() {
  for (int i = 0; i < 6; i++) {
    String param = "a" + String(i);
    if (server.hasArg(param)) {
      int angle = constrain(server.arg(param).toInt(), 0, 180);
      targetAngle[i] = angle;   // servo will ramp toward this in loop(), not jump instantly
    }
  }
  server.send(200, "text/plain", "OK");
}

// Moves each servo one small step closer to its target, at a fixed interval.
// This runs continuously in loop() so movement is smooth and non-blocking --
// the web page stays responsive while servos are still ramping.
void updateServoPositions() {
  unsigned long now = millis();
  for (int i = 0; i < 6; i++) {
    if (now - lastStepTime[i] < STEP_INTERVAL_MS[i]) continue;
    if (currentAngle[i] == targetAngle[i]) continue;

    lastStepTime[i] = now;
    if (currentAngle[i] < targetAngle[i]) {
      currentAngle[i] = min(currentAngle[i] + STEP_SIZE[i], targetAngle[i]);
    } else {
      currentAngle[i] = max(currentAngle[i] - STEP_SIZE[i], targetAngle[i]);
    }
    joints[i].write(currentAngle[i]);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < 6; i++) {
    joints[i].setPeriodHertz(50);
    joints[i].attach(SERVO_PINS[i], 500, 2400);
    currentAngle[i] = DEFAULT_ANGLE[i];
    targetAngle[i] = DEFAULT_ANGLE[i];
    joints[i].write(currentAngle[i]);
  }

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  Serial.println("=======================================");
  Serial.println(" Robotic Arm Web Control Panel Ready");
  Serial.println("=======================================");
  Serial.print("Connect to WiFi: ");
  Serial.println(WIFI_SSID);
  Serial.print("Password: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Then open in browser: http://");
  Serial.println(ip);

  server.on("/", handleRoot);
  server.on("/moveAll", handleMoveAll);
  server.begin();
}

void loop() {
  server.handleClient();
  updateServoPositions();
}
