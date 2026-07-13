/**
 * @file wifi.ino
 * @brief Wi-Fi通信与Web监控模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 实现ESP32的Wi-Fi STA模式连接、UDP通信、Web服务器和实时监控界面。
 * 监控界面包含摄像头实时画面、传感器数据和电机状态显示。
 */

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "globals.h"

/** Wi-Fi热点名称 */
#define WIFI_SSID "TP-LINK_64AE"
/** Wi-Fi密码 */
#define WIFI_PASSWORD "fdc65tkr"

/** 静态IP地址 */
#define WIFI_STATIC_IP      "192.168.1.100"
/** 网关地址 */
#define WIFI_STATIC_GATEWAY "192.168.1.1"
/** 子网掩码 */
#define WIFI_STATIC_SUBNET  "255.255.255.0"

/** UDP本地端口 */
#define WIFI_UDP_PORT 14550
/** UDP远程端口 */
#define WIFI_UDP_REMOTE_PORT 14550
/** UDP广播地址 */
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"

/** Web服务器对象 */
WebServer server(80);
/** UDP通信对象 */
WiFiUDP udp;

/** HTML page stored in Flash (PROGMEM) to save RAM - all dynamic data comes from /data JSON endpoint */
static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>飞行监控</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Consolas,monospace;background:#0a0a0f;color:#ddd;overflow-x:hidden;height:100vh;display:flex;flex-direction:column;}
.header{background:#111;padding:6px 12px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #222;flex-shrink:0;}
.header h2{font-size:15px;color:#00d4ff;text-shadow:0 0 8px rgba(0,212,255,0.3);}
.status{font-size:10px;color:#888;}
.status span{color:#0f0;}
.main-content{flex:1;display:flex;overflow:hidden;min-height:0;}
.camera-panel{flex:0 0 50%;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#0d0d15;border-right:1px solid #222;padding:12px;position:relative;}
.camera-panel img{width:100%;height:100%;object-fit:contain;border-radius:6px;border:1px solid #1a1a25;}
.camera-label{font-size:10px;color:#555;position:absolute;top:16px;left:16px;text-transform:uppercase;letter-spacing:1px;}
.camera-status{font-size:10px;color:#0f0;position:absolute;top:16px;right:16px;}
.rotate-btn{position:absolute;bottom:16px;right:16px;z-index:10;background:#1a1a25;color:#888;border:1px solid #333;border-radius:4px;padding:4px 10px;font-size:11px;cursor:pointer;font-family:Consolas,monospace;}
.rotate-btn:hover{color:#0cf;border-color:#0cf;}
.data-panel{flex:1;overflow-y:auto;padding:16px;display:flex;flex-direction:column;gap:10px;}
.card{background:#111118;border:1px solid #1a1a25;border-radius:6px;padding:6px 10px;text-align:center;}
.card-label{font-size:11px;color:#888;margin-bottom:2px;text-transform:uppercase;letter-spacing:1px;}
.card-value{font-size:19px;font-weight:bold;color:#00d4ff;}
.card-unit{font-size:11px;color:#666;margin-left:2px;}
.card-bar{height:2px;border-radius:2px;margin-top:3px;transition:width 0.3s,background 0.3s;}
.card-warn{display:none;}
.card-row{display:flex;gap:6px;}
.card-row .card{flex:1;}
.footer{background:#111;padding:4px 12px;display:flex;gap:14px;border-top:1px solid #222;font-size:11px;flex-wrap:wrap;flex-shrink:0;}
.motor-val{color:#ff0;}
.section-title-flight{font-size:10px;color:#29b6f6;margin:4px 0 2px 0;padding-bottom:2px;border-bottom:1px solid #1a3a5e;text-transform:uppercase;letter-spacing:1px;}
.section-title-env{font-size:10px;color:#66bb6a;margin:4px 0 2px 0;padding-bottom:2px;border-bottom:1px solid #1a3a1e;text-transform:uppercase;letter-spacing:1px;}
.val-flight{color:#1565c0;}
.val-alt{color:#00897b;}
.val-dist{color:#0277bd;}
.val-temp{color:#d84315;}
.val-hum{color:#1565c0;}
.val-press{color:#7b1fa2;}
.val-warn{color:#d32f2f;}
.val-ok{color:#2e7d32;}
</style>

<script>
function init(){
update();setInterval(update,1000);
refreshCam();setInterval(refreshCam,150);
}

function voltColor(v){return v<3.0?'#f44':v<3.5?'#fa0':'#4f4';}

var camLastSeq=-1;
function refreshCam(){
fetch('/capture-status').then(r=>r.json()).then(s=>{
if(!s.ready)return;
if(s.seq!=camLastSeq){
camLastSeq=s.seq;
document.getElementById('camImg').src='/capture?t='+Date.now();
document.getElementById('camStatus').textContent='LIVE';
document.getElementById('camStatus').style.color='#0f0';
}
}).catch(function(){});
}

var camRot=0;
function rotateCam(){
camRot=(camRot+90)%360;
var img=document.getElementById('camImg');
img.style.transform='rotate('+camRot+'deg)';
img.style.transition='transform 0.3s';
var btn=document.getElementById('rotBtn');
btn.textContent=camRot+' deg';
}

var _d={m0:'',m1:'',m2:'',m3:'',conn:null,clients:'',mv:'',cv:'',press:'',alt:'',dist:'',temp:'',hum:'',pOk:null,vOk:null,aOk:null,mode:'',armed:null,cpu:''};
function update(){
fetch('/data').then(r=>r.json()).then(d=>{
var v;
v=d.m0.toFixed(1);if(v!=_d.m0){document.getElementById('m0').textContent=v;_d.m0=v;}
v=d.m1.toFixed(1);if(v!=_d.m1){document.getElementById('m1').textContent=v;_d.m1=v;}
v=d.m2.toFixed(1);if(v!=_d.m2){document.getElementById('m2').textContent=v;_d.m2=v;}
v=d.m3.toFixed(1);if(v!=_d.m3){document.getElementById('m3').textContent=v;_d.m3=v;}
if(d.conn!=_d.conn){document.getElementById('conn').textContent=d.conn?'已连接':'未连接';_d.conn=d.conn;}
v=''+d.clients;if(v!=_d.clients){document.getElementById('clients').textContent=v;_d.clients=v;}
v=d.mv.toFixed(2);if(v!=_d.mv){var mvEl=document.getElementById('mv');mvEl.textContent=v;_d.mv=v;
 var mBar=document.getElementById('mvbar');mBar.style.width=Math.min(d.mv/5*100,100)+'%';mBar.style.background=voltColor(d.mv);}
v=d.cv.toFixed(2);if(v!=_d.cv){var cvEl=document.getElementById('cv');cvEl.textContent=v;_d.cv=v;
 var cBar=document.getElementById('cvbar');cBar.style.width=Math.min(d.cv/4*100,100)+'%';cBar.style.background=voltColor(d.cv);}
if(d.pOk!=_d.pOk){_d.pOk=d.pOk;if(d.pOk){document.getElementById('press').textContent=d.press.toFixed(1);}else{document.getElementById('press').textContent='- -';}}
if(d.pOk!=_d.pOk||d.press.toFixed(1)!=_d.press){v=d.pOk?d.press.toFixed(1):'- -';
 var pEl=document.getElementById('press');pEl.textContent=v;_d.press=v;}
if(d.pOk!=_d.pOk||d.alt.toFixed(1)!=_d.alt){v=d.pOk?d.alt.toFixed(1):'- -';
 var aEl=document.getElementById('alt');aEl.textContent=v;_d.alt=v;}
if(d.vOk!=_d.vOk||d.dist!=_d.dist){v=d.vOk?d.dist:'- -';
 var dEl=document.getElementById('dist');dEl.textContent=v;_d.dist=v;_d.vOk=d.vOk;}
if(d.aOk!=_d.aOk||d.temp.toFixed(1)!=_d.temp){v=d.aOk?d.temp.toFixed(1):'- -';
 var tEl=document.getElementById('temp');tEl.textContent=v;_d.temp=v;_d.aOk=d.aOk;}
if(d.aOk!=_d.aOk||d.hum.toFixed(1)!=_d.hum){v=d.aOk?d.hum.toFixed(1):'- -';
 var hEl=document.getElementById('hum');hEl.textContent=v;_d.hum=v;_d.aOk=d.aOk;}
var modes=['STAB','OBSTACLE'];v=modes[d.mode]||'?';
if(v!=_d.mode){document.getElementById('mode').textContent=v;_d.mode=v;}
if(d.armed!=_d.armed){var aEl2=document.getElementById('armed');_d.armed=d.armed;
 aEl2.textContent=d.armed?'已解锁':'锁定';aEl2.style.color=d.armed?'#0f0':'#f44';}
v=d.cpu.toFixed(1);if(v!=_d.cpu){var cpuEl=document.getElementById('cpu');cpuEl.textContent=v;
 var cpuBar=document.getElementById('cpubar');cpuBar.style.width=Math.min(d.cpu,100)+'%';
 cpuBar.style.background=d.cpu<50?'#4f4':d.cpu<80?'#fa0':'#f44';_d.cpu=v;}
});}
</script>

</head><body onload='init()'>

<div class='header'>
<h2>🛸 飞行监控</h2>
<div class='status'>模式: <span id='mode' style='color:#0cf'>-</span> | 
解锁: <span id='armed' style='color:#f44'>-</span> | 
MAVLink: <span id='conn'>-</span> | 设备: <span id='clients' style='color:#0cf'>0</span></div>
</div>

<div class='main-content'>

<div class='camera-panel'>
<div class='camera-label'>OV3660 实时画面</div>
<div class='camera-status' id='camStatus'>LIVE</div>
<button class='rotate-btn' id='rotBtn' onclick='rotateCam()'>0 deg</button>
<img id='camImg' src='/capture' alt='Camera' onerror="document.getElementById('camStatus').textContent='OFF';document.getElementById('camStatus').style.color='#f44';">
</div>

<div class='data-panel'>

<div class='section-title-flight'>🛸 飞行器数据</div>

<div class='card-row'>
<div class='card'>
<div class='card-label'>⚡ 电机电源电压</div>
<div class='card-value val-flight' id='mv'>0.00</div><span class='card-unit'>V</span>
<div class='card-bar' id='mvbar' style='width:0%;background:#4f4;'></div>
</div>
<div class='card'>
<div class='card-label'>⚡ 芯片供电电压</div>
<div class='card-value val-flight' id='cv'>0.00</div><span class='card-unit'>V</span>
<div class='card-bar' id='cvbar' style='width:0%;background:#4f4;'></div>
</div>
</div>

<div class='card-row'>
<div class='card'>
<div class='card-label'>📏 距离 (VL53L0X)</div>
<div class='card-value val-dist' id='dist'>- -</div><span class='card-unit'>mm</span>
</div>
<div class='card'>
<div class='card-label'>🏔️ 海拔高度</div>
<div class='card-value val-alt' id='alt'>- -</div><span class='card-unit'>m</span>
</div>
</div>

<div class='section-title-env'>🌍 环境数据</div>

<div class='card-row'>
<div class='card'>
<div class='card-label'>🌡️ 温度 (PM280)</div>
<div class='card-value val-temp' id='temp'>- -</div><span class='card-unit'>°C</span>
</div>
<div class='card'>
<div class='card-label'>💧 湿度</div>
<div class='card-value val-hum' id='hum'>63</div><span class='card-unit'>%</span>
</div>
<div class='card'>
<div class='card-label'>🌬️ 气压 (PM280)</div>
<div class='card-value val-press' id='press'>- -</div><span class='card-unit'>hPa</span>
</div>
</div>

</div>
</div>

<div class='footer'>
<span>电机占空比:</span>
<span class='motor-val'>RL: <b id='m0'>0</b>%</span>
<span class='motor-val'>RR: <b id='m1'>0</b>%</span>
<span class='motor-val'>FR: <b id='m2'>0</b>%</span>
<span class='motor-val'>FL: <b id='m3'>0</b>%</span>
<span>| CPU: <b id='cpu' style='color:#0cf'>0</b>%</span>
<span style='display:inline-block;width:60px;height:8px;background:#1a1a25;border-radius:2px;vertical-align:middle;margin-left:4px;'><span id='cpubar' style='display:block;height:100%;width:0%;border-radius:2px;transition:width 0.5s,background 0.5s;'></span></span>
</div>

</body></html>
)rawliteral";

/**
 * @brief 生成Web监控页面HTML (从Flash PROGMEM读取，零RAM堆分配)
 * @return HTML页面字符串
 */
String getPage() {
  return String(FPSTR(PAGE_HTML));
}

/**
 * @brief Web端点: /data
 * 
 * 返回飞行状态JSON数据，供前端实时更新显示。
 */
void handleData() {
  float m0, m1, m2, m3;
  bool conn;
  int clients;
  float mv, cv;
  float press, alt, dist, temp, hum;
  bool pOk, vOk, aOk;
  int _mode;
  bool _armed;

  xSemaphoreTake(mutexState, pdMS_TO_TICKS(5));
  m0 = motors[0] * 100;
  m1 = motors[1] * 100;
  m2 = motors[2] * 100;
  m3 = motors[3] * 100;
  conn = mavlinkConnected;
  clients = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  mv = motorVoltage;
  cv = chipVoltage;
  press = pressureHpa;
  alt = altitudeM;
  dist = distanceMm;
  temp = temperatureC;
  hum = humidity;
  pOk = pm280Ok;
  vOk = vl53l0xOk;
  aOk = aht20Ok;
  _mode = mode;
  _armed = armed;
  xSemaphoreGive(mutexState);

  String json = "{";
  json += "\"m0\":" + String(m0) + ",";
  json += "\"m1\":" + String(m1) + ",";
  json += "\"m2\":" + String(m2) + ",";
  json += "\"m3\":" + String(m3) + ",";
  json += "\"conn\":" + String(conn ? "true" : "false") + ",";
  json += "\"clients\":" + String(clients) + ",";
  json += "\"mv\":" + String(mv, 2) + ",";
  json += "\"cv\":" + String(cv, 2) + ",";
  json += "\"press\":" + String(press, 1) + ",";
  json += "\"alt\":" + String(alt, 1) + ",";
  json += "\"dist\":" + String(dist, 0) + ",";
  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"hum\":" + String(hum, 1) + ",";
  json += "\"pOk\":" + String(pOk ? "true" : "false") + ",";
  json += "\"vOk\":" + String(vOk ? "true" : "false") + ",";
  json += "\"aOk\":" + String(aOk ? "true" : "false") + ",";
  json += "\"mode\":" + String(_mode) + ",";
  json += "\"armed\":" + String(_armed ? "true" : "false");
  json += ",\"cpu\":" + String(cpuUsagePercent, 1);
  json += "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Web端点: /
 * 
 * 返回监控页面HTML。
 */
void handleRoot() {
  server.send(200, "text/html", getPage());
}

/**
 * @brief 初始化Wi-Fi和Web服务器
 */
void setupWiFi() {
  print("Setup Wi-Fi (STA, DHCP) + Web Server\n");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    print("WiFi connected!\n");
    print("================================\n");
    print("Web: http://%s\n", WiFi.localIP().toString().c_str());
    print("================================\n");
  } else {
    print("WiFi connection failed!\n");
  }

  udp.begin(WIFI_UDP_PORT);

  server.on("/", handleRoot);
  server.on("/data", handleData);
#if CAMERA_ENABLED
  server.on("/capture", handleCameraCapture);
  server.on("/capture-status", handleCameraStatus);
#endif
  server.begin();
}

/**
 * @brief 处理Web服务器客户端请求
 */
void handleWebServer() {
  server.handleClient();
}

/**
 * @brief 通过UDP发送数据
 * @param buf 数据缓冲区
 * @param len 数据长度
 */
void sendWiFi(const uint8_t *buf, int len) {
  if (WiFi.status() != WL_CONNECTED) return;
  udp.beginPacket(udp.remoteIP() ? udp.remoteIP() : WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
  udp.write(buf, len);
  udp.endPacket();
}

/**
 * @brief 通过UDP接收数据
 * @param buf 接收缓冲区
 * @param len 缓冲区最大长度
 * @return 实际接收的字节数
 */
int receiveWiFi(uint8_t *buf, int len) {
  udp.parsePacket();
  return udp.read(buf, len);
}

/**
 * @brief 打印Wi-Fi连接信息
 */
void printWiFiInfo() {
  print("MAC: %s\n", WiFi.macAddress().c_str());
  print("SSID: %s\n", WIFI_SSID);
  print("Password: %s\n", WIFI_PASSWORD);
  print("Status: %d\n", WiFi.status());
  print("IP: %s\n", WiFi.localIP().toString().c_str());
  print("Remote IP: %s\n", udp.remoteIP().toString().c_str());
  print("MAVLink connected: %d\n", mavlinkConnected);
}

#endif