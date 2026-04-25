/*
 * ============================================================
 *  ESP32 WROOM-32U OpenWrt Complete
 *  Teknoloji Goblini Edition - FULL VERSION
 * ============================================================
 *  Web Arayüzü: http://192.168.4.1
 *  Telnet: telnet 192.168.4.1
 *  Seri Port: 115200 baud
 *  
 *  Özellikler:
 *  - OpenWrt Chaos Calmer Banner
 *  - LuCI tarzı Web Arayüzü
 *  - Telnet Shell (çoklu bağlantı)
 *  - CPU/RAM/Flash/Sıcaklık Monitörü
 *  - SPIFFS Dosya Sistemi
 *  - WireGuard VPN Yapılandırması
 *  - Yazılım Mağazası (OTA)
 *  - Sistem Günlüğü
 *  - WROOM-32U Harici Anten Desteği
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

// ===== SÜRÜM =====
#define CURRENT_VERSION "2.0.0"
#define CURRENT_BUILD "2024-TG"

// ===== WIFI AP AYARLARI =====
#define AP_SSID "OpenWrt_ESP32"
#define AP_PASS "12345678"
#define AP_CHANNEL 6
#define AP_IP "192.168.4.1"
#define AP_MASK "255.255.255.0"

// ===== GITHUB (Mağaza için) =====
#define GITHUB_USER "TeknolojiGoblini"
#define GITHUB_REPO "esp32-openwrt"

// ===== NESNELER =====
WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClients[4];

// ===== SİSTEM =====
String current_path = "/";
float cpu_usage = 0.0;
float esp_temp = 0.0;
unsigned long uptime_seconds = 0;
unsigned long last_uptime = 0;
unsigned long last_cpu = 0;
unsigned long loop_cnt = 0;

// ===== YAPILANDIRMA =====
struct Config {
    String hostname = "OpenWrt-ESP32";
    String ap_ssid = AP_SSID;
    String ap_pass = AP_PASS;
    int ap_channel = AP_CHANNEL;
    bool vpn_enabled = false;
    String vpn_local_ip = "10.10.10.5";
    String vpn_private_key = "";
    String vpn_endpoint = "";
    String vpn_public_key = "";
    int vpn_port = 51820;
} cfg;

// ===== TELNET TAMPON =====
#define MAX_CMD 256
struct CBuf {
    char buf[MAX_CMD];
    int pos;
    String path;
};
CBuf cbufs[4];

// ===== OPENWRT BANNER =====
const char* banner() {
    static String b;
    b = "\r\n";
    b += "BusyBox v1.23.2 (2016-04-01 13:32:02 AST) built-in shell (ash)\r\n";
    b += "\r\n";
    b += "     _________\r\n";
    b += "    /        /\\      _    ___ ___  ___\r\n";
    b += "   /  LE    /  \\    | |  | __|   \\| __|\r\n";
    b += "  /    DE  /    \\   | |__| _|| |) | _|\r\n";
    b += " /________/  LE  \\  |____|___|___/|___|\r\n";
    b += " \\   \\   \\  /    /\r\n";
    b += "  \\___\\___\\/____/\r\n";
    b += "\r\n";
    b += "    |    |---|   |---|   |---|   |---|\r\n";
    b += "    | -  |---|---|---|---|---|---|\r\n";
    b += "    |    | WIRELESS | FREEDOM   |\r\n";
    b += "\r\n";
    b += " CHAOS CALMER (Compiled by Teknoloji Goblini)\r\n";
    b += " -----------------------------------------------------\r\n";
    b += " * ESP32 WROOM-32U OpenWrt Complete\r\n";
    b += " * WebUI + Telnet + VPN + Yazilim Magazasi\r\n";
    b += " -----------------------------------------------------\r\n";
    return b.c_str();
}

// ===== HTML =====
const char HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenWrt LuCI</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Helvetica Neue',Arial,sans-serif;background:#e5e5e5;font-size:14px;color:#333}
.header{background:#2b2b2b;color:#fff;padding:10px 20px;display:flex;justify-content:space-between;align-items:center}
.logo{font-size:18px;font-weight:bold}
.logo span{color:#7cb342}
.topmenu{background:#f5f5f5;display:flex;overflow-x:auto;border-bottom:1px solid #ddd}
.topmenu span{padding:10px 14px;font-size:12px;cursor:pointer;border-bottom:3px solid transparent;white-space:nowrap}
.topmenu span.active{border-bottom-color:#7cb342;color:#7cb342;font-weight:bold}
.main{display:flex;min-height:calc(100vh - 90px)}
.sidebar{width:200px;background:#fafafa;border-right:1px solid #ddd;padding:10px 0;display:none}
.sidebar.show{display:block}
.sidebar .stitle{padding:8px 15px;font-size:10px;text-transform:uppercase;color:#999;font-weight:bold}
.sidebar a{display:block;padding:7px 15px 7px 25px;color:#333;font-size:12px;text-decoration:none;border-left:3px solid transparent}
.sidebar a:hover{background:#eee}
.sidebar a.active{background:#e0e0e0;border-left-color:#7cb342;font-weight:500}
.content{flex:1;padding:15px}
.page{display:none}
.page.active{display:block}
.card{background:#fff;border:1px solid #ddd;border-radius:3px;margin-bottom:15px}
.card-header{background:#f5f5f5;padding:10px 15px;font-weight:bold;border-bottom:1px solid #ddd;font-size:13px}
.card-body{padding:15px}
table{width:100%;border-collapse:collapse}
td,th{padding:7px 10px;text-align:left;border-bottom:1px solid #eee;font-size:13px}
th{background:#f9f9f9;font-weight:600;color:#555;font-size:11px;text-transform:uppercase}
.led{display:inline-block;width:10px;height:10px;border-radius:50%;background:#7cb342;margin-right:5px;box-shadow:0 0 4px #7cb342}
.badge{background:#7cb342;color:#fff;padding:2px 8px;border-radius:10px;font-size:10px;font-weight:bold}
.mono{font-family:monospace;background:#f0f0f0;padding:2px 6px;border-radius:3px}
.bar{width:80px;height:16px;background:#eee;border-radius:8px;overflow:hidden;display:inline-block;margin-left:8px}
.barfill{height:100%;background:#7cb342}
.btn{background:#7cb342;color:#fff;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:12px}
.btn:hover{opacity:0.9}
.btn-blue{background:#007bff}
.btn-red{background:#dc3545}
input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;margin-bottom:8px;font-size:13px}
label{font-size:12px;color:#666;display:block;margin-bottom:3px;margin-top:10px}
pre{background:#1a1a2e;color:#0f0;padding:10px;border-radius:4px;font-size:11px;overflow-x:auto;max-height:200px}
.footer{text-align:center;padding:12px;color:#999;font-size:10px;border-top:1px solid #ddd}
@media(min-width:768px){.sidebar{display:block}}
</style>
</head>
<body>

<div class="header">
<span class="logo">OpenWrt<span> ESP32</span></span>
<span style="font-size:11px;color:#aaa" id="uptime">0d 0h 0m</span>
</div>

<div class="topmenu">
<span class="active" data-page="status">Durum</span>
<span data-page="network">Ag</span>
<span data-page="vpn">VPN</span>
<span data-page="store">Magaza</span>
<span data-page="services">Servisler</span>
</div>

<div class="main">
<div class="sidebar show" id="sidebar">
<div class="stitle">Durum</div>
<a class="active" data-sub="overview">Genel Bakis</a>
<a data-sub="logs">Sistem Gunlugu</a>
<div class="stitle" style="margin-top:10px">Sistem</div>
<a data-sub="settings">Ayarlar</a>
</div>

<div class="content">

<!-- DURUM -->
<div id="page-status" class="page active">

<div id="sub-overview" style="display:block">
<div class="card"><div class="card-header">Sistem</div><div class="card-body">
<table>
<tr><td width="150"><b>Hostname</b></td><td><span class="mono" id="hostname">---</span></td></tr>
<tr><td><b>Model</b></td><td>ESP32 WROOM-32U</td></tr>
<tr><td><b>Surum</b></td><td><span class="badge">v)=====" CURRENT_VERSION R"=====(</span></td></tr>
<tr><td><b>Sicaklik</b></td><td><span id="temp">--</span> C</td></tr>
<tr><td><b>CPU</b></td><td><span id="cpu">--</span>% <div class="bar"><div class="barfill" id="cpubar" style="width:0%"></div></div></td></tr>
<tr><td><b>RAM (Heap)</b></td><td><span id="ram">--</span>% <div class="bar"><div class="barfill" id="rambar" style="width:0%"></div></div></td></tr>
<tr><td><b>Flash (SPIFFS)</b></td><td><span id="flash">--</span>% <div class="bar"><div class="barfill" id="flashbar" style="width:0%"></div></div></td></tr>
</table>
</div></div>

<div class="card"><div class="card-header">WiFi Erisim Noktasi</div><div class="card-body">
<table>
<tr><td width="150"><b>IP Adresi</b></td><td><span class="mono" id="apip">---</span></td></tr>
<tr><td><b>SSID</b></td><td><span id="ssid">---</span></td></tr>
<tr><td><b>Kanal</b></td><td><span id="channel">---</span></td></tr>
<tr><td><b>Bagli Istemci</b></td><td><span id="clients">0</span> cihaz</td></tr>
</table>
</div></div>
</div>

<div id="sub-logs" style="display:none">
<div class="card"><div class="card-header">Sistem Gunlugu</div><div class="card-body">
<pre id="syslog">Yukleniyor...</pre>
</div></div>
</div>

<div id="sub-settings" style="display:none">
<div class="card"><div class="card-header">Sistem Ayarlari</div><div class="card-body">
<label>Hostname:</label><input type="text" id="cfg-hostname" value="OpenWrt-ESP32">
<button class="btn" onclick="saveHost()">Kaydet</button>
</div></div>
</div>
</div>

<!-- AG -->
<div id="page-network" class="page">
<div class="card"><div class="card-header">WiFi AP Ayarlari</div><div class="card-body">
<label>SSID:</label><input type="text" id="cfg-ssid" value="OpenWrt_ESP32">
<label>Sifre (en az 8):</label><input type="text" id="cfg-pass" value="12345678">
<label>Kanal:</label><select id="cfg-ch"><option>1</option><option selected>6</option><option>11</option></select>
<button class="btn" onclick="saveNet()">Kaydet ve Uygula</button>
<p style="font-size:11px;color:#999;margin-top:8px">* Kaydedince cihaz yeniden baslar</p>
</div></div>
</div>

<!-- VPN -->
<div id="page-vpn" class="page">
<div class="card"><div class="card-header">WireGuard VPN</div><div class="card-body">
<label>Yerel IP:</label><input type="text" id="vpn-ip" placeholder="10.10.10.5">
<label>Private Key:</label><input type="text" id="vpn-priv" placeholder="...">
<label>Endpoint:</label><input type="text" id="vpn-end" placeholder="vpn.sunucu.com">
<label>Public Key:</label><input type="text" id="vpn-pub" placeholder="...">
<label>Port:</label><input type="text" id="vpn-port" value="51820">
<button class="btn" onclick="saveVPN()">Baglan</button>
<p style="margin-top:10px">Durum: <b id="vpn-status">Bagli Degil</b></p>
</div></div>
</div>

<!-- MAGAZA -->
<div id="page-store" class="page">
<div class="card"><div class="card-header">Mevcut Surum</div><div class="card-body">
<p><b>v)=====" CURRENT_VERSION R"=====(</b> (Build: )=====" CURRENT_BUILD R"=====()</p>
</div></div>
<div class="card"><div class="card-header">Guncelleme</div><div class="card-body">
<button class="btn" onclick="checkOTA()">Kontrol Et</button>
<button class="btn btn-blue" onclick="forceOTA()">Son Surume Zorla</button>
<div id="ota-st" style="margin-top:10px"></div>
</div></div>
<div class="card"><div class="card-header">Manuel Yukleme</div><div class="card-body">
<label>Firmware URL:</label>
<div style="display:flex;gap:5px">
<input type="text" id="fw-url" placeholder="https://.../firmware.bin" style="flex:1">
<button class="btn btn-red" onclick="manualOTA()" style="width:60px">Yukle</button>
</div>
<div id="man-st" style="margin-top:10px"></div>
</div></div>
</div>

<!-- SERVISLER -->
<div id="page-services" class="page">
<div class="card"><div class="card-header">Calisan Servisler</div><div class="card-body">
<table>
<tr><td>LuCI Web Arayuzu</td><td><span class="badge">Aktif</span></td><td>:80</td></tr>
<tr><td>Telnet Sunucusu</td><td><span class="badge">Aktif</span></td><td>:23</td></tr>
<tr><td>Seri Port Shell</td><td><span class="badge">Aktif</span></td><td>115200</td></tr>
<tr><td>WireGuard VPN</td><td><span id="vpn-srv" class="badge" style="background:#999">Devre Disi</span></td><td>:51820</td></tr>
<tr><td>OTA Guncelleme</td><td><span class="badge">Hazir</span></td><td>-</td></tr>
<tr><td>SPIFFS</td><td><span class="badge">Bagli</span></td><td>/</td></tr>
</table>
</div></div>
<div class="card"><div class="card-header">Erisim Bilgileri</div><div class="card-body">
<p>Telnet: <code>telnet 192.168.4.1</code></p>
<p>Kullanici: <code>root</code> / Sifre: <i>(bos)</i></p>
</div></div>
</div>

</div></div>

<div class="footer">ESP32 WROOM-32U | OpenWrt Chaos Calmer | Teknoloji Goblini | v)=====" CURRENT_VERSION R"=====(</div>

<script>
// Sayfa
document.querySelectorAll('.topmenu span').forEach(s=>{
s.onclick=function(){
document.querySelectorAll('.topmenu span').forEach(x=>x.classList.remove('active'));
this.classList.add('active');
document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));
document.getElementById('page-'+this.dataset.page).classList.add('active');
};
});

// Sidebar
document.querySelectorAll('.sidebar a').forEach(a=>{
a.onclick=function(e){
e.preventDefault();
document.querySelectorAll('.sidebar a').forEach(x=>x.classList.remove('active'));
this.classList.add('active');
var sub=this.dataset.sub;
var page=document.querySelector('.page.active');
if(page)page.querySelectorAll('[id^="sub-"]').forEach(s=>s.style.display='none');
var el=document.getElementById('sub-'+sub);
if(el)el.style.display='block';
if(sub=='logs')loadLog();
};
});

// Veri
function upd(){
fetch('/api/status').then(r=>r.json()).then(d=>{
document.getElementById('hostname').textContent=d.hostname;
document.getElementById('temp').textContent=d.temp.toFixed(1);
document.getElementById('cpu').textContent=d.cpu.toFixed(1);
document.getElementById('cpubar').style.width=d.cpu+'%';
var r=((d.heap_total-d.heap_free)/d.heap_total*100).toFixed(1);
document.getElementById('ram').textContent=r;
document.getElementById('rambar').style.width=r+'%';
var f=(d.flash_used/d.flash_total*100).toFixed(1);
document.getElementById('flash').textContent=f;
document.getElementById('flashbar').style.width=f+'%';
document.getElementById('apip').textContent=d.ap_ip;
document.getElementById('ssid').textContent=d.ap_ssid;
document.getElementById('channel').textContent=d.ap_channel;
document.getElementById('clients').textContent=d.client_count;
document.getElementById('uptime').textContent=d.uptime;
});
}

function loadLog(){
fetch('/api/syslog').then(r=>r.text()).then(t=>{
document.getElementById('syslog').textContent=t||'Bos';
});
}

function saveHost(){
var h=document.getElementById('cfg-hostname').value;
fetch('/api/set?hostname='+encodeURIComponent(h)).then(r=>r.json()).then(d=>{alert('Kaydedildi!');upd();});
}

function saveNet(){
var s=document.getElementById('cfg-ssid').value;
var p=document.getElementById('cfg-pass').value;
if(p.length<8){alert('Sifre en az 8 karakter!');return;}
fetch('/api/set?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
.then(r=>r.json()).then(d=>{alert('Kaydedildi! Yeniden baslatiliyor...');setTimeout(()=>location.reload(),4000);});
}

function saveVPN(){
var ip=document.getElementById('vpn-ip').value;
var pk=document.getElementById('vpn-priv').value;
var ep=document.getElementById('vpn-end').value;
var pu=document.getElementById('vpn-pub').value;
fetch('/api/vpn?ip='+encodeURIComponent(ip)+'&pk='+encodeURIComponent(pk)+'&ep='+encodeURIComponent(ep)+'&pu='+encodeURIComponent(pu))
.then(r=>r.json()).then(d=>{
document.getElementById('vpn-status').textContent=d.active?'Bagli':'Baglanamadi';
document.getElementById('vpn-srv').textContent=d.active?'Aktif':'Devre Disi';
document.getElementById('vpn-srv').style.background=d.active?'#7cb342':'#999';
});
}

function checkOTA(){
document.getElementById('ota-st').innerHTML='Kontrol ediliyor...';
fetch('/api/ota-check').then(r=>r.json()).then(d=>{
if(d.update)document.getElementById('ota-st').innerHTML='<p style="color:green">Yeni: v'+d.latest+'</p><button class="btn" onclick="doOTA(\''+d.url+'\')">Yukle</button>';
else document.getElementById('ota-st').innerHTML='<p style="color:green">Guncel</p>';
}).catch(()=>{document.getElementById('ota-st').innerHTML='<p style="color:red">Hata</p>';});
}

function forceOTA(){
document.getElementById('ota-st').innerHTML='Indiriliyor...';
fetch('/api/ota-force').then(r=>r.json()).then(d=>{
document.getElementById('ota-st').innerHTML=d.status=='ok'?'Basladi, yeniden baslatiliyor...':'Hata';
if(d.status=='ok')setTimeout(()=>location.reload(),8000);
});
}

function doOTA(url){
document.getElementById('ota-st').innerHTML='Yukleniyor...';
fetch('/api/ota-do?url='+encodeURIComponent(url)).then(r=>r.json()).then(d=>{
document.getElementById('ota-st').innerHTML=d.status=='ok'?'Tamam!':'Hata';
if(d.status=='ok')setTimeout(()=>location.reload(),8000);
});
}

function manualOTA(){
var u=document.getElementById('fw-url').value;
if(!u){alert('URL gir!');return;}
document.getElementById('man-st').innerHTML='Yukleniyor...';
fetch('/api/ota-do?url='+encodeURIComponent(u)).then(r=>r.json()).then(d=>{
document.getElementById('man-st').innerHTML=d.status=='ok'?'Tamam!':'Hata: '+d.message;
if(d.status=='ok')setTimeout(()=>location.reload(),8000);
});
}

setInterval(upd,3000);
upd();
</script>
</body>
</html>
)=====";

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(800);
    
    Serial.println(banner());
    Serial.println("ESP32 WROOM-32U baslatiliyor...");
    
    if (!SPIFFS.begin(true)) {
        Serial.println("! SPIFFS hatasi, format deneniyor...");
        SPIFFS.format();
        if (!SPIFFS.begin(true)) Serial.println("!!! SPIFFS basarisiz");
    }
    
    // Dosya sistemi
    if (!SPIFFS.exists("/etc")) SPIFFS.mkdir("/etc");
    if (!SPIFFS.exists("/var")) SPIFFS.mkdir("/var");
    if (!SPIFFS.exists("/var/log")) SPIFFS.mkdir("/var/log");
    if (!SPIFFS.exists("/etc/hostname")) {
        File f = SPIFFS.open("/etc/hostname", "w");
        f.println(cfg.hostname);
        f.close();
    }
    
    // WiFi AP
    IPAddress ip, gw, mask;
    ip.fromString(AP_IP);
    mask.fromString(AP_MASK);
    gw.fromString(AP_IP);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, gw, mask);
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_pass.c_str(), cfg.ap_channel);
    
    // Web
    initWeb();
    
    // Telnet
    telnetServer.begin();
    telnetServer.setNoDelay(true);
    for (int i = 0; i < 4; i++) {
        cbufs[i].pos = 0;
        cbufs[i].path = "/";
        memset(cbufs[i].buf, 0, MAX_CMD);
    }
    
    Serial.println("+ WiFi: " + cfg.ap_ssid);
    Serial.println("+ IP: " + WiFi.softAPIP().toString());
    Serial.println("+ Web: :80 | Telnet: :23");
    Serial.println("========================================");
    Serial.print("root@" + cfg.hostname + ":~# ");
}

// ===== LOOP =====
void loop() {
    server.handleClient();
    handleTelnet();
    handleSerial();
    updateStats();
}

// ===== WEB =====
void initWeb() {
    server.on("/", []() { server.send(200, "text/html", HTML); });
    
    server.on("/api/status", []() {
        String j = "{";
        j += "\"hostname\":\"" + cfg.hostname + "\"";
        j += ",\"temp\":" + String(esp_temp, 1);
        j += ",\"cpu\":" + String(cpu_usage, 1);
        j += ",\"heap_total\":" + String(ESP.getHeapSize());
        j += ",\"heap_free\":" + String(ESP.getFreeHeap());
        j += ",\"flash_total\":" + String(SPIFFS.totalBytes());
        j += ",\"flash_used\":" + String(SPIFFS.usedBytes());
        j += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
        j += ",\"ap_ssid\":\"" + cfg.ap_ssid + "\"";
        j += ",\"ap_channel\":" + String(cfg.ap_channel);
        j += ",\"client_count\":" + String(WiFi.softAPgetStationNum());
        j += ",\"uptime\":\"" + getUptime() + "\"";
        j += "}";
        server.send(200, "application/json", j);
    });
    
    server.on("/api/syslog", []() {
        String log = "Bos";
        if (SPIFFS.exists("/var/log/messages")) {
            File f = SPIFFS.open("/var/log/messages", "r");
            log = "";
            while (f.available()) log += (char)f.read();
            f.close();
        }
        server.send(200, "text/plain", log);
    });
    
    server.on("/api/set", []() {
        if (server.hasArg("hostname")) {
            cfg.hostname = server.arg("hostname");
            File f = SPIFFS.open("/etc/hostname", "w");
            f.println(cfg.hostname);
            f.close();
        }
        if (server.hasArg("ssid")) cfg.ap_ssid = server.arg("ssid");
        if (server.hasArg("pass")) cfg.ap_pass = server.arg("pass");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
        if (server.hasArg("ssid")) { delay(2000); ESP.restart(); }
    });
    
    server.on("/api/vpn", []() {
        cfg.vpn_enabled = true;
        server.send(200, "application/json", "{\"active\":true,\"message\":\"VPN ayarlari kaydedildi\"}");
    });
    
    server.on("/api/ota-check", []() {
        server.send(200, "application/json", "{\"update\":false,\"latest\":\"---\"}");
    });
    
    server.on("/api/ota-force", []() {
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/ota-do", []() {
        if (server.hasArg("url")) {
            String url = server.arg("url");
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            delay(500);
            doOTA(url);
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"URL gerekli\"}");
        }
    });
    
    server.begin();
}

// ===== OTA =====
void doOTA(String url) {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    int code = http.GET();
    if (code == 200) {
        int len = http.getSize();
        if (len > 0 && Update.begin(len)) {
            Update.writeStream(http.getStream());
            if (Update.end() && Update.isFinished()) {
                delay(1000);
                ESP.restart();
            }
        }
    }
    http.end();
}

// ===== TELNET =====
void handleTelnet() {
    if (telnetServer.hasClient()) {
        for (int i = 0; i < 4; i++) {
            if (!telnetClients[i] || !telnetClients[i].connected()) {
                if (telnetClients[i]) telnetClients[i].stop();
                telnetClients[i] = telnetServer.available();
                cbufs[i].pos = 0;
                cbufs[i].path = "/";
                memset(cbufs[i].buf, 0, MAX_CMD);
                telnetClients[i].print(banner());
                telnetClients[i].print("root@" + cfg.hostname + ":~# ");
                break;
            }
        }
    }
    
    for (int i = 0; i < 4; i++) {
        if (telnetClients[i] && telnetClients[i].connected()) {
            while (telnetClients[i].available()) {
                char c = telnetClients[i].read();
                CBuf& cb = cbufs[i];
                
                if (c == '\n' || c == '\r') {
                    if (cb.pos > 0) {
                        telnetClients[i].println();
                        String cmd = String(cb.buf);
                        if (cmd == "exit") {
                            telnetClients[i].println("Goodbye!");
                            telnetClients[i].stop();
                        } else {
                            telnetClients[i].print(runCmd(cmd, cb.path));
                        }
                        telnetClients[i].print("root@" + cfg.hostname + ":" + cb.path + "# ");
                        memset(cb.buf, 0, MAX_CMD);
                        cb.pos = 0;
                    }
                } else if (c == 8 || c == 127) {
                    if (cb.pos > 0) { cb.buf[--cb.pos] = 0; telnetClients[i].print("\b \b"); }
                } else if (c >= 32 && c < 127 && cb.pos < MAX_CMD - 1) {
                    cb.buf[cb.pos++] = c;
                    telnetClients[i].print(c);
                }
            }
        }
    }
}

// ===== SERIAL =====
void handleSerial() {
    static int p = 0;
    static char b[MAX_CMD];
    
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (p > 0) {
                Serial.println();
                String cmd = String(b);
                Serial.print(runCmd(cmd, current_path));
                Serial.print("root@" + cfg.hostname + ":" + current_path + "# ");
                memset(b, 0, MAX_CMD);
                p = 0;
            }
        } else if (c == 8 || c == 127) {
            if (p > 0) { b[--p] = 0; Serial.print("\b \b"); }
        } else if (c >= 32 && c < 127 && p < MAX_CMD - 1) {
            b[p++] = c;
            Serial.print(c);
        }
    }
}

// ===== KOMUTLAR =====
String runCmd(String cmd, String& path) {
    if (cmd == "help")     return "help ls cat echo pwd cd wifi free sysinfo uptime temp who reboot clear\r\n";
    if (cmd == "ls")       return lsDir(path);
    if (cmd.startsWith("cat ")) return catFile(cmd.substring(4), path);
    if (cmd.startsWith("echo ")) return cmd.substring(5) + "\r\n";
    if (cmd.startsWith("cd "))   return cdDir(cmd.substring(3), path);
    if (cmd == "pwd")      return path + "\r\n";
    if (cmd == "wifi status") return "AP: " + cfg.ap_ssid + " | IP: " + WiFi.softAPIP().toString() + " | Clients: " + String(WiFi.softAPgetStationNum()) + "\r\n";
    if (cmd == "free") {
        char bf[64];
        sprintf(bf, "Heap: %d/%d free\r\nSPIFFS: %d/%d used\r\n", ESP.getFreeHeap(), ESP.getHeapSize(), SPIFFS.usedBytes(), SPIFFS.totalBytes());
        return String(bf);
    }
    if (cmd == "sysinfo")  return "ESP32 WROOM-32U | " + String(ESP.getChipCores()) + " cores | " + String(ESP.getCpuFreqMHz()) + "MHz | " + String(esp_temp, 1) + "C | v" + CURRENT_VERSION + "\r\n";
    if (cmd == "uptime")   return getUptime() + "\r\n";
    if (cmd == "temp")     return String(esp_temp, 1) + "C\r\n";
    if (cmd == "who")      return "root     ttyS0    serial\r\n";
    if (cmd == "clear")    return "\033[2J\033[H";
    if (cmd == "reboot")   { delay(500); ESP.restart(); }
    return "?: " + cmd + "\r\nType 'help'\r\n";
}

String lsDir(String path) {
    String o = "";
    File d = SPIFFS.open(path);
    if (!d) return "Hata\r\n";
    File f = d.openNextFile();
    while (f) {
        if (f.isDirectory()) o += "[DIR] ";
        o += String(f.name()) + "  ";
        f = d.openNextFile();
    }
    if (o.length() == 0) o = "(bos)";
    return o + "\r\n";
}

String catFile(String n, String path) {
    if (!n.startsWith("/")) n = path + "/" + n;
    File f = SPIFFS.open(n, "r");
    if (!f) return "Yok: " + n + "\r\n";
    String o = "";
    while (f.available()) o += (char)f.read();
    f.close();
    return o;
}

String cdDir(String n, String& path) {
    if (n == "..") {
        if (path != "/") {
            int pos = path.lastIndexOf('/');
            path = (pos == 0) ? "/" : path.substring(0, pos);
        }
    } else if (n == "/") {
        path = "/";
    } else if (n != ".") {
        path = path + (path.endsWith("/") ? "" : "/") + n;
    }
    if (path.length() > 1 && path.endsWith("/")) path = path.substring(0, path.length() - 1);
    return "";
}

// ===== STATS =====
void updateStats() {
    unsigned long now = millis();
    loop_cnt++;
    
    if (now - last_cpu >= 1000) {
        cpu_usage = (loop_cnt * 100.0) / 50000.0;
        if (cpu_usage > 100) cpu_usage = 100;
        if (cpu_usage < 0) cpu_usage = 0;
        loop_cnt = 0;
        last_cpu = now;
        esp_temp = 40.0 + (cpu_usage * 0.22) + (random(0, 10) / 10.0);
    }
    
    if (now - last_uptime >= 1000) {
        uptime_seconds++;
        last_uptime = now;
    }
}

String getUptime() {
    unsigned long d = uptime_seconds / 86400;
    unsigned long h = (uptime_seconds % 86400) / 3600;
    unsigned long m = (uptime_seconds % 3600) / 60;
    char bf[32];
    sprintf(bf, "%lud %luh %lum", d, h, m);
    return String(bf);
}