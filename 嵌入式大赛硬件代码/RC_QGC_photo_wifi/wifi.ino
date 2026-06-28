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

extern SemaphoreHandle_t mutexState;

#if CAMERA_ENABLED
extern bool cameraReady;
extern void handleCameraCapture();
#endif

/** Wi-Fi热点名称 */
#define WIFI_SSID "杂牌12"
/** Wi-Fi密码 */
#define WIFI_PASSWORD "11111111"

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

extern float motors[4];
extern bool mavlinkConnected;
extern float motorVoltage;
extern float chipVoltage;
extern float pressureHpa;
extern float temperatureC;
extern float distanceMm;
extern float altitudeM;
extern bool pm280Ok;
extern bool vl53l0xOk;
extern int mode;
extern bool armed;

/**
 * @brief 生成Web监控页面HTML
 * @return HTML页面字符串
 */
String getPage() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='zh-CN'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>飞行监控</title>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box;}";
  html += "body{font-family:Consolas,monospace;background:#0a0a0f;color:#ddd;overflow-x:hidden;height:100vh;display:flex;flex-direction:column;}";
  html += ".header{background:#111;padding:6px 12px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #222;flex-shrink:0;}";
  html += ".header h2{font-size:15px;color:#0cf;}";
  html += ".status{font-size:10px;color:#888;}";
  html += ".status span{color:#0f0;}";
  html += ".main-content{flex:1;display:flex;overflow:hidden;min-height:0;}";
  html += ".camera-panel{flex:0 0 50%;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#0d0d15;border-right:1px solid #222;padding:12px;position:relative;}";
  html += ".camera-panel img{width:100%;height:100%;object-fit:contain;border-radius:6px;border:1px solid #1a1a25;}";
  html += ".camera-label{font-size:10px;color:#555;position:absolute;top:16px;left:16px;text-transform:uppercase;letter-spacing:1px;}";
  html += ".camera-status{font-size:10px;color:#0f0;position:absolute;top:16px;right:16px;}";
  html += ".rotate-btn{position:absolute;bottom:16px;right:16px;z-index:10;background:#1a1a25;color:#888;border:1px solid #333;border-radius:4px;padding:4px 10px;font-size:11px;cursor:pointer;font-family:Consolas,monospace;}";
  html += ".rotate-btn:hover{color:#0cf;border-color:#0cf;}";
  html += ".data-panel{flex:1;overflow-y:auto;padding:16px;display:flex;flex-direction:column;gap:10px;}";
  html += ".card{background:#111118;border:1px solid #1a1a25;border-radius:8px;padding:12px 16px;text-align:center;}";
  html += ".card-label{font-size:10px;color:#888;margin-bottom:4px;text-transform:uppercase;letter-spacing:1px;}";
  html += ".card-value{font-size:28px;font-weight:bold;color:#0cf;}";
  html += ".card-unit{font-size:11px;color:#666;margin-left:2px;}";
  html += ".card-bar{height:3px;border-radius:2px;margin-top:6px;transition:width 0.3s,background 0.3s;}";
  html += ".card-warn{font-size:8px;margin-top:2px;color:#555;}";
  html += ".card-row{display:flex;gap:10px;}";
  html += ".card-row .card{flex:1;}";
  html += ".footer{background:#111;padding:4px 12px;display:flex;gap:14px;border-top:1px solid #222;font-size:11px;flex-wrap:wrap;flex-shrink:0;}";
  html += ".motor-val{color:#ff0;}";
  html += "</style>";

  html += "<script>";
  html += "function init(){";
  html += "update();setInterval(update,1000);";
  html += "refreshCam();setInterval(refreshCam,150);";
  html += "}";

  html += "function voltColor(v){return v<3.0?'#f44':v<3.5?'#fa0':'#4f4';}";

  html += "function refreshCam(){";
  html += "document.getElementById('camImg').src='/capture?t='+Date.now();";
  html += "}";

  html += "var camRot=0;";
  html += "function rotateCam(){";
  html += "camRot=(camRot+90)%360;";
  html += "var img=document.getElementById('camImg');";
  html += "img.style.transform='rotate('+camRot+'deg)';";
  html += "img.style.transition='transform 0.3s';";
  html += "var btn=document.getElementById('rotBtn');";
  html += "btn.textContent=camRot+' deg';";
  html += "}";

  html += "function update(){";
  html += "fetch('/data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('m0').textContent=d.m0.toFixed(1);";
  html += "document.getElementById('m1').textContent=d.m1.toFixed(1);";
  html += "document.getElementById('m2').textContent=d.m2.toFixed(1);";
  html += "document.getElementById('m3').textContent=d.m3.toFixed(1);";
  html += "document.getElementById('conn').textContent=d.conn?'已连接':'未连接';";
  html += "document.getElementById('clients').textContent=d.clients;";
  html += "var mvEl=document.getElementById('mv');mvEl.textContent=d.mv.toFixed(2);";
  html += "var mBar=document.getElementById('mvbar');mBar.style.width=Math.min(d.mv/5*100,100)+'%';mBar.style.background=voltColor(d.mv);";
  html += "var cvEl=document.getElementById('cv');cvEl.textContent=d.cv.toFixed(2);";
  html += "var cBar=document.getElementById('cvbar');cBar.style.width=Math.min(d.cv/4*100,100)+'%';cBar.style.background=voltColor(d.cv);";
  html += "var pEl=document.getElementById('press');";
  html += "if(d.pOk){pEl.textContent=d.press.toFixed(1);}else{pEl.textContent='- -';}";
  html += "var aEl=document.getElementById('alt');";
  html += "if(d.pOk){aEl.textContent=d.alt.toFixed(1);}else{aEl.textContent='- -';}";
  html += "var dEl=document.getElementById('dist');";
  html += "if(d.vOk){dEl.textContent=d.dist;}else{dEl.textContent='- -';}";
  html += "var tEl=document.getElementById('temp');";
  html += "if(d.pOk){tEl.textContent=d.temp.toFixed(1);}else{tEl.textContent='- -';}";
  html += "var modes=['RAW','ACRO','STAB','AUTO','OBSTACLE'];";
  html += "var mEl=document.getElementById('mode');mEl.textContent=modes[d.mode]||'?';";
  html += "var aEl2=document.getElementById('armed');";
  html += "aEl2.textContent=d.armed?'已解锁':'锁定';";
  html += "aEl2.style.color=d.armed?'#0f0':'#f44';";
  html += "});}";
  html += "</script>";

  html += "</head><body onload='init()'>";

  html += "<div class='header'>";
  html += "<h2>飞行监控</h2>";
  html += "<div class='status'>模式: <span id='mode' style='color:#0cf'>-</span> | ";
  html += "解锁: <span id='armed' style='color:#f44'>-</span> | ";
  html += "MAVLink: <span id='conn'>-</span> | 设备: <span id='clients' style='color:#0cf'>0</span></div>";
  html += "</div>";

  html += "<div class='main-content'>";

  html += "<div class='camera-panel'>";
  html += "<div class='camera-label'>OV3660 实时画面</div>";
  html += "<div class='camera-status' id='camStatus'>LIVE</div>";
  html += "<button class='rotate-btn' id='rotBtn' onclick='rotateCam()'>0 deg</button>";
  html += "<img id='camImg' src='/capture' alt='Camera' onerror=\"document.getElementById('camStatus').textContent='OFF';document.getElementById('camStatus').style.color='#f44';\">";
  html += "</div>";

  html += "<div class='data-panel'>";

  html += "<div class='card-row'>";
  html += "<div class='card'>";
  html += "<div class='card-label'>电机电源电压</div>";
  html += "<div class='card-value' id='mv'>0.00</div><span class='card-unit'>V</span>";
  html += "<div class='card-bar' id='mvbar' style='width:0%;background:#4f4;'></div>";
  html += "<div class='card-warn'>正常: 3.5 ~ 5.0 V</div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<div class='card-label'>芯片供电电压</div>";
  html += "<div class='card-value' id='cv'>0.00</div><span class='card-unit'>V</span>";
  html += "<div class='card-bar' id='cvbar' style='width:0%;background:#4f4;'></div>";
  html += "<div class='card-warn'>正常: 3.5 ~ 4.5 V</div>";
  html += "</div>";
  html += "</div>";

  html += "<div class='card-row'>";
  html += "<div class='card'>";
  html += "<div class='card-label'>气压 (PM280)</div>";
  html += "<div class='card-value' id='press'>- -</div><span class='card-unit'>hPa</span>";
  html += "<div class='card-warn'>I2C: 0x76 | 300~1100 hPa</div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<div class='card-label'>海拔高度</div>";
  html += "<div class='card-value' id='alt' style='color:#0f0;'>- -</div><span class='card-unit'>m</span>";
  html += "<div class='card-warn'>气压换算 | 参考: 1013.25 hPa</div>";
  html += "</div>";
  html += "</div>";

  html += "<div class='card-row'>";
  html += "<div class='card'>";
  html += "<div class='card-label'>距离 (VL53L0X)</div>";
  html += "<div class='card-value' id='dist'>- -</div><span class='card-unit'>mm</span>";
  html += "<div class='card-warn'>I2C: 0x29 | 量程 30~2000 mm</div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<div class='card-label'>温度 (PM280)</div>";
  html += "<div class='card-value' id='temp'>- -</div><span class='card-unit'>°C</span>";
  html += "<div class='card-warn'>内置温度传感器</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";
  html += "</div>";

  html += "<div class='footer'>";
  html += "<span>电机占空比:</span>";
  html += "<span class='motor-val'>RL: <b id='m0'>0</b>%</span>";
  html += "<span class='motor-val'>RR: <b id='m1'>0</b>%</span>";
  html += "<span class='motor-val'>FR: <b id='m2'>0</b>%</span>";
  html += "<span class='motor-val'>FL: <b id='m3'>0</b>%</span>";
  html += "</div>";

  html += "</body></html>";
  return html;
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
  float press, alt, dist, temp;
  bool pOk, vOk;
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
  pOk = pm280Ok;
  vOk = vl53l0xOk;
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
  json += "\"pOk\":" + String(pOk ? "true" : "false") + ",";
  json += "\"vOk\":" + String(vOk ? "true" : "false") + ",";
  json += "\"mode\":" + String(_mode) + ",";
  json += "\"armed\":" + String(_armed ? "true" : "false");
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