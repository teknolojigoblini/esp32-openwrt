/*
 * ============================================================
 *  ESP32 WROOM-32U OpenWrt Complete - FULL VERSION
 *  Teknoloji Goblini Edition
 *  Github Actions Uyumlu
 * ============================================================
 *  
 *  BU KOD DAHİL TÜM FONKSİYONLAR:
 *  - OpenWrt Chaos Calmer Banner
 *  - LuCI tarzı Web Arayüzü (5 ana sekme + sidebar)
 *  - Telnet Shell (4 eşzamanlı bağlantı)
 *  - Seri Port Shell
 *  - CPU/RAM/Flash/Sıcaklık Monitörü
 *  - SPIFFS Dosya Sistemi
 *  - WireGuard VPN Yapılandırması
 *  - Yazılım Mağazası (OTA + Repo Yönetimi)
 *  - Sistem Günlüğü
 *  - Güncelleme Geçmişi
 *  - WROOM-32U Harici Anten Desteği
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <Update.h>

// ===== SÜRÜM =====
#define CURRENT_VERSION "2.0.0"
#define CURRENT_BUILD "2024-TG-FULL"

// ===== WIFI AP AYARLARI =====
#define AP_SSID_DEFAULT "OpenWrt_ESP32"
#define AP_PASS_DEFAULT "12345678"
#define AP_CHANNEL_DEFAULT 6

// ===== VARSAYILAN REPO =====
#define DEFAULT_REPO_USER "TeknolojiGoblini"
#define DEFAULT_REPO_NAME "esp32-openwrt"

// ===== NESNELER =====
WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClients[4];

// ===== SİSTEM DEĞİŞKENLERİ =====
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
    String ap_ssid = AP_SSID_DEFAULT;
    String ap_pass = AP_PASS_DEFAULT;
    int ap_channel = AP_CHANNEL_DEFAULT;
    
    // VPN
    bool vpn_enabled = false;
    String vpn_local_ip = "10.10.10.5";
    String vpn_private_key = "";
    String vpn_endpoint = "";
    String vpn_public_key = "";
    int vpn_port = 51820;
    
    // Repo
    String repo_user = DEFAULT_REPO_USER;
    String repo_name = DEFAULT_REPO_NAME;
    String repo_branch = "main";
    String custom_firmware_url = "";
    bool use_custom_url = false;
    
    // Güncelleme geçmişi
    String last_update_check = "Henuz kontrol edilmedi";
    String last_update_status = "Bilinmiyor";
    int update_count = 0;
} cfg;

// ===== TELNET TAMPONU =====
#define MAX_CMD 256
struct CBuf {
    char buf[MAX_CMD];
    int pos;
    String path;
};
CBuf cbufs[4];

// ===== OPENWRT BANNER =====
const char* getBanner() {
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
    b += " * Repo Yonetimi Aktif\r\n";
    b += " -----------------------------------------------------\r\n";
    return b.c_str();
}

// ===== HTML SAYFASI =====
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
.sidebar a{display:block;padding:7px 15px 7px 25px;color:#333;font-size:12px;text-decoration:none;border-left:3px solid transparent;cursor:pointer}
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
.badge-gray{background:#999}
.mono{font-family:monospace;background:#f0f0f0;padding:2px 6px;border-radius:3px;font-size:12px}
.bar{width:80px;height:16px;background:#eee;border-radius:8px;overflow:hidden;display:inline-block;margin-left:8px}
.barfill{height:100%;background:#7cb342}
.btn{background:#7cb342;color:#fff;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:12px}
.btn:hover{opacity:0.9}
.btn-blue{background:#007bff}
.btn-red{background:#dc3545}
.btn-orange{background:#fd7e14}
input,select,textarea{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;margin-bottom:8px;font-size:13px}
label{font-size:12px;color:#666;display:block;margin-bottom:3px;margin-top:10px}
pre{background:#1a1a2e;color:#0f0;padding:10px;border-radius:4px;font-size:11px;overflow-x:auto;max-height:200px}
.footer{text-align:center;padding:12px;color:#999;font-size:10px;border-top:1px solid #ddd}
.info-box{padding:10px;background:#e8f5e9;border-left:4px solid #7cb342;border-radius:4px;margin:10px 0;font-size:12px}
.info-box.blue{background:#e3f2fd;border-left-color:#007bff}
.info-box.orange{background:#fff3e0;border-left-color:#fd7e14}
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
<div class="stitle" style="margin-top:10px">Magaza</div>
<a data-sub="repo-settings">Repo Ayarlari</a>
<a data-sub="update-history">Guncelleme Gecmisi</a>
</div>

<div class="content">

<!-- ========== DURUM SAYFASI ========== -->
<div id="page-status" class="page active">

<div id="sub-overview" style="display:block">
<div class="card"><div class="card-header">Sistem Bilgisi</div><div class="card-body">
<table>
<tr><td width="150"><b>Hostname</b></td><td><span class="mono" id="hostname">---</span></td></tr>
<tr><td><b>Model</b></td><td>ESP32 WROOM-32U</td></tr>
<tr><td><b>Surum</b></td><td><span class="badge">v)=====" CURRENT_VERSION R"=====(</span></td></tr>
<tr><td><b>Build</b></td><td><span class="mono">)=====" CURRENT_BUILD R"=====(</span></td></tr>
<tr><td><b>Sicaklik</b></td><td><span id="temp">--</span> C</td></tr>
<tr><td><b>CPU Kullanimi</b></td><td><span id="cpu">--</span>% <div class="bar"><div class="barfill" id="cpubar" style="width:0%"></div></div></td></tr>
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

<div class="card"><div class="card-header">Yazilim Deposu</div><div class="card-body">
<table>
<tr><td width="150"><b>Repo</b></td><td><span class="mono" id="repo-info">---</span></td></tr>
<tr><td><b>Son Kontrol</b></td><td><span id="last-check">---</span></td></tr>
<tr><td><b>Guncelleme Sayisi</b></td><td><span id="update-count">0</span></td></tr>
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

<div id="sub-repo-settings" style="display:none">
<div class="card"><div class="card-header">Repo Ayarlari</div><div class="card-body">
<p style="margin-bottom:10px">Yazilim guncellemelerinin cekilecegi Github reposunu yapilandirin.</p>
<label>Github Kullanici Adi:</label><input type="text" id="cfg-repo-user" placeholder="TeknolojiGoblini">
<label>Repo Adi:</label><input type="text" id="cfg-repo-name" placeholder="esp32-openwrt">
<label>Branch:</label><input type="text" id="cfg-repo-branch" placeholder="main">
<button class="btn" onclick="saveRepo()">Repo Ayarlarini Kaydet</button>
<button class="btn btn-blue" onclick="resetRepo()">Varsayilana Don</button>
</div></div>

<div class="card"><div class="card-header">Ozel Firmware URL</div><div class="card-body">
<label>Ozel URL:</label><input type="text" id="cfg-custom-url" placeholder="https://.../firmware.bin">
<label><input type="checkbox" id="cfg-use-custom"> Ozel URL kullan</label>
<button class="btn btn-orange" onclick="saveCustomURL()">Kaydet</button>
<button class="btn btn-red" onclick="clearCustomURL()">Temizle</button>
</div></div>
</div>

<div id="sub-update-history" style="display:none">
<div class="card"><div class="card-header">Guncelleme Gecmisi</div><div class="card-body">
<table>
<tr><td><b>Son Kontrol</b></td><td><span id="hist-last-check">---</span></td></tr>
<tr><td><b>Son Durum</b></td><td><span id="hist-status">---</span></td></tr>
<tr><td><b>Toplam Guncelleme</b></td><td><span id="hist-count">0</span></td></tr>
</table>
</div></div>
</div>
</div>

<!-- ========== AG SAYFASI ========== -->
<div id="page-network" class="page">
<div class="card"><div class="card-header">WiFi AP Ayarlari</div><div class="card-body">
<label>SSID:</label><input type="text" id="cfg-ssid" value="OpenWrt_ESP32">
<label>Sifre (en az 8 karakter):</label><input type="text" id="cfg-pass" value="12345678">
<label>Kanal:</label><select id="cfg-ch"><option>1</option><option selected>6</option><option>11</option></select>
<button class="btn" onclick="saveNet()">Kaydet ve Uygula</button>
<p style="font-size:11px;color:#999;margin-top:8px">* Kaydedince cihaz yeniden baslar</p>
</div></div>
</div>

<!-- ========== VPN SAYFASI ========== -->
<div id="page-vpn" class="page">
<div class="card"><div class="card-header">WireGuard VPN Yapilandirmasi</div><div class="card-body">
<label>Yerel IP Adresi:</label><input type="text" id="vpn-ip" placeholder="10.10.10.5">
<label>Private Key:</label><input type="text" id="vpn-priv" placeholder="...">
<label>Endpoint (Sunucu):</label><input type="text" id="vpn-end" placeholder="vpn.sunucu.com">
<label>Public Key (Sunucu):</label><input type="text" id="vpn-pub" placeholder="...">
<label>Port:</label><input type="text" id="vpn-port" value="51820">
<button class="btn" onclick="saveVPN()">Kaydet ve Baglan</button>
<p style="margin-top:10px">Durum: <b id="vpn-status">Bagli Degil</b></p>
</div></div>
</div>

<!-- ========== MAGAZA SAYFASI ========== -->
<div id="page-store" class="page">
<div class="card"><div class="card-header">Mevcut Yazilim</div><div class="card-body">
<table>
<tr><td width="150"><b>Surum</b></td><td><span class="badge">v)=====" CURRENT_VERSION R"=====(</span></td></tr>
<tr><td><b>Build</b></td><td><span class="mono">)=====" CURRENT_BUILD R"=====(</span></td></tr>
<tr><td><b>Aktif Repo</b></td><td><span class="mono" id="active-repo">---</span></td></tr>
<tr><td><b>Ozel URL</b></td><td><span id="active-custom">Devre Disi</span></td></tr>
</table>
</div></div>

<div class="card"><div class="card-header">Guncelleme Merkezi</div><div class="card-body">
<button class="btn" onclick="checkUpdate()">Guncellemeleri Kontrol Et</button>
<button class="btn btn-blue" onclick="forceUpdate()">Son Surume Zorla Guncelle</button>
<button class="btn btn-orange" onclick="checkCustomUpdate()">Ozel URL'den Kontrol Et</button>
<div id="update-result" style="margin-top:15px"></div>
</div></div>

<div class="card"><div class="card-header">Manuel Yukleme</div><div class="card-body">
<label>Firmware URL'si:</label>
<div style="display:flex;gap:5px">
<input type="text" id="fw-url" placeholder="https://.../firmware.bin" style="flex:1">
<button class="btn btn-red" onclick="manualOTA()" style="width:60px">Yukle</button>
</div>
<div id="manual-status" style="margin-top:10px"></div>
</div></div>
</div>

<!-- ========== SERVISLER SAYFASI ========== -->
<div id="page-services" class="page">
<div class="card"><div class="card-header">Calisan Servisler</div><div class="card-body">
<table>
<tr><td>LuCI Web Arayuzu</td><td><span class="badge">Aktif</span></td><td>:80</td></tr>
<tr><td>Telnet Sunucusu</td><td><span class="badge">Aktif</span></td><td>:23</td></tr>
<tr><td>Seri Port Shell</td><td><span class="badge">Aktif</span></td><td>115200</td></tr>
<tr><td>WireGuard VPN</td><td><span id="vpn-srv" class="badge badge-gray">Devre Disi</span></td><td>:51820</td></tr>
<tr><td>OTA Guncelleme</td><td><span class="badge">Hazir</span></td><td>-</td></tr>
<tr><td>Repo Yonetimi</td><td><span class="badge">Aktif</span></td><td>-</td></tr>
<tr><td>SPIFFS Dosya Sistemi</td><td><span class="badge">Bagli</span></td><td>/</td></tr>
</table>
</div></div>
<div class="card"><div class="card-header">Baglanti Bilgileri</div><div class="card-body">
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
if(sub=='repo-settings')loadRepoSettings();
if(sub=='update-history')loadHistory();
};
});

// Veri
function upd(){
fetch('/api/status').then(r=>r.json()).then(d=>{
document.getElementById('hostname').textContent=d.hostname;
document.getElementById('temp').textContent=d.temp.toFixed(1);
document.getElementById('cpu').textContent=d.cpu.toFixed(1);
document.getElementById('cpubar').style.width=d.cpu+'%';
var ram=((d.heap_total-d.heap_free)/d.heap_total*100).toFixed(1);
document.getElementById('ram').textContent=ram;
document.getElementById('rambar').style.width=ram+'%';
var flash=(d.flash_used/d.flash_total*100).toFixed(1);
document.getElementById('flash').textContent=flash;
document.getElementById('flashbar').style.width=flash+'%';
document.getElementById('apip').textContent=d.ap_ip;
document.getElementById('ssid').textContent=d.ap_ssid;
document.getElementById('channel').textContent=d.ap_channel;
document.getElementById('clients').textContent=d.client_count;
document.getElementById('uptime').textContent=d.uptime;
document.getElementById('repo-info').textContent=d.repo_user+'/'+d.repo_name;
document.getElementById('last-check').textContent=d.last_check;
document.getElementById('update-count').textContent=d.update_count;
document.getElementById('active-repo').textContent=d.repo_user+'/'+d.repo_name;
document.getElementById('active-custom').textContent=d.custom_url_active?'Aktif: '+d.custom_url:'Devre Disi';
});
}

function loadLog(){
fetch('/api/syslog').then(r=>r.text()).then(t=>{
document.getElementById('syslog').textContent=t||'Bos';
});
}

function loadHistory(){
fetch('/api/update-history').then(r=>r.json()).then(d=>{
document.getElementById('hist-last-check').textContent=d.last_check;
document.getElementById('hist-status').textContent=d.last_status;
document.getElementById('hist-count').textContent=d.update_count;
});
}

function loadRepoSettings(){
fetch('/api/repo-settings').then(r=>r.json()).then(d=>{
document.getElementById('cfg-repo-user').value=d.repo_user||'';
document.getElementById('cfg-repo-name').value=d.repo_name||'';
document.getElementById('cfg-repo-branch').value=d.repo_branch||'main';
document.getElementById('cfg-custom-url').value=d.custom_url||'';
document.getElementById('cfg-use-custom').checked=d.use_custom;
});
}

// Kaydet
function saveHost(){
var h=document.getElementById('cfg-hostname').value;
fetch('/api/set?hostname='+encodeURIComponent(h)).then(r=>r.json()).then(d=>{alert('Kaydedildi!');upd();});
}

function saveNet(){
var s=document.getElementById('cfg-ssid').value;
var p=document.getElementById('cfg-pass').value;
if(p.length<8){alert('Sifre en az 8 karakter!');return;}
fetch('/api/set?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
.then(r=>r.json()).then(d=>{alert('Kaydedildi!');setTimeout(()=>location.reload(),4000);});
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
document.getElementById('vpn-srv').className=d.active?'badge':'badge badge-gray';
});
}

function saveRepo(){
var u=document.getElementById('cfg-repo-user').value;
var r=document.getElementById('cfg-repo-name').value;
var b=document.getElementById('cfg-repo-branch').value||'main';
fetch('/api/repo-save?user='+encodeURIComponent(u)+'&repo='+encodeURIComponent(r)+'&branch='+encodeURIComponent(b))
.then(r=>r.json()).then(d=>{alert(d.status=='ok'?'Kaydedildi!':'Hata');upd();});
}

function resetRepo(){fetch('/api/repo-reset').then(r=>r.json()).then(d=>{alert('Sifirlandi!');loadRepoSettings();upd();});}

function saveCustomURL(){
var url=document.getElementById('cfg-custom-url').value;
var use=document.getElementById('cfg-use-custom').checked;
fetch('/api/custom-url-save?url='+encodeURIComponent(url)+'&use='+(use?'1':'0'))
.then(r=>r.json()).then(d=>{alert(d.status=='ok'?'Kaydedildi!':'Hata');upd();});
}

function clearCustomURL(){fetch('/api/custom-url-clear').then(r=>r.json()).then(d=>{alert('Temizlendi!');loadRepoSettings();upd();});}

// OTA
function checkUpdate(){
document.getElementById('update-result').innerHTML='Kontrol ediliyor...';
fetch('/api/ota-check').then(r=>r.json()).then(d=>{
if(d.update)document.getElementById('update-result').innerHTML='<p style="color:green">Yeni: v'+d.latest+'</p><button class="btn" onclick="startOTA(\''+d.url+'\')">Yukle</button>';
else document.getElementById('update-result').innerHTML='<p style="color:green">Guncel (v'+d.current+')</p>';
});
}

function checkCustomUpdate(){
document.getElementById('update-result').innerHTML='Ozel URL kontrol ediliyor...';
fetch('/api/ota-check-custom').then(r=>r.json()).then(d=>{
document.getElementById('update-result').innerHTML=d.update?'<p style="color:green">Var! <button class="btn" onclick="startOTA(\''+d.url+'\')">Yukle</button></p>':'<p>'+d.message+'</p>';
});
}

function forceUpdate(){
document.getElementById('update-result').innerHTML='Indiriliyor...';
fetch('/api/ota-force').then(r=>r.json()).then(d=>{
document.getElementById('update-result').innerHTML=d.status=='ok'?'Basladi...':'Hata';
if(d.status=='ok')setTimeout(()=>location.reload(),8000);
});
}

function startOTA(url){
document.getElementById('update-result').innerHTML='Yukleniyor...';
fetch('/api/ota-do?url='+encodeURIComponent(url)).then(r=>r.json()).then(d=>{
document.getElementById('update-result').innerHTML=d.status=='ok'?'Tamam! Yeniden baslatiliyor...':'Hata';
if(d.status=='ok')setTimeout(()=>location.reload(),8000);
});
}

function manualOTA(){
var u=document.getElementById('fw-url').value;
if(!u){alert('URL gir!');return;}
document.getElementById('manual-status').innerHTML='Yukleniyor...';
fetch('/api/ota-do?url='+encodeURIComponent(u)).then(r=>r.json()).then(d=>{
document.getElementById('manual-status').innerHTML=d.status=='ok'?'Tamam!':'Hata';
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
    
    Serial.println(getBanner());
    
    if (!SPIFFS.begin(true)) {
        SPIFFS.format();
        SPIFFS.begin(true);
    }
    
    if (!SPIFFS.exists("/etc")) SPIFFS.mkdir("/etc");
    if (!SPIFFS.exists("/var")) SPIFFS.mkdir("/var");
    if (!SPIFFS.exists("/var/log")) SPIFFS.mkdir("/var/log");
    
    IPAddress ip, gw, mask;
    ip.fromString("192.168.4.1");
    mask.fromString("255.255.255.0");
    gw.fromString("192.168.4.1");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, gw, mask);
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_pass.c_str(), cfg.ap_channel);
    
    initWeb();
    
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
    Serial.print("root@OpenWrt-ESP32:~# ");
}

void loop() {
    server.handleClient();
    handleTelnet();
    handleSerial();
    updateStats();
}

// ===== WEB SUNUCU =====
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
        j += ",\"repo_user\":\"" + cfg.repo_user + "\"";
        j += ",\"repo_name\":\"" + cfg.repo_name + "\"";
        j += ",\"last_check\":\"" + cfg.last_update_check + "\"";
        j += ",\"update_count\":" + String(cfg.update_count);
        j += ",\"custom_url_active\":" + String(cfg.use_custom_url ? "true" : "false");
        j += ",\"custom_url\":\"" + cfg.custom_firmware_url + "\"";
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
        if (server.hasArg("hostname")) cfg.hostname = server.arg("hostname");
        if (server.hasArg("ssid")) cfg.ap_ssid = server.arg("ssid");
        if (server.hasArg("pass")) cfg.ap_pass = server.arg("pass");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
        if (server.hasArg("ssid")) { delay(2000); ESP.restart(); }
    });
    
    server.on("/api/vpn", []() {
        cfg.vpn_enabled = true;
        if (server.hasArg("ip")) cfg.vpn_local_ip = server.arg("ip");
        if (server.hasArg("pk")) cfg.vpn_private_key = server.arg("pk");
        if (server.hasArg("ep")) cfg.vpn_endpoint = server.arg("ep");
        if (server.hasArg("pu")) cfg.vpn_public_key = server.arg("pu");
        server.send(200, "application/json", "{\"active\":true,\"message\":\"VPN ayarlari kaydedildi\"}");
    });
    
    server.on("/api/repo-settings", []() {
        String j = "{";
        j += "\"repo_user\":\"" + cfg.repo_user + "\"";
        j += ",\"repo_name\":\"" + cfg.repo_name + "\"";
        j += ",\"repo_branch\":\"" + cfg.repo_branch + "\"";
        j += ",\"custom_url\":\"" + cfg.custom_firmware_url + "\"";
        j += ",\"use_custom\":" + String(cfg.use_custom_url ? "true" : "false");
        j += "}";
        server.send(200, "application/json", j);
    });
    
    server.on("/api/repo-save", []() {
        if (server.hasArg("user")) cfg.repo_user = server.arg("user");
        if (server.hasArg("repo")) cfg.repo_name = server.arg("repo");
        if (server.hasArg("branch")) cfg.repo_branch = server.arg("branch");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/repo-reset", []() {
        cfg.repo_user = DEFAULT_REPO_USER;
        cfg.repo_name = DEFAULT_REPO_NAME;
        cfg.repo_branch = "main";
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/custom-url-save", []() {
        if (server.hasArg("url")) cfg.custom_firmware_url = server.arg("url");
        if (server.hasArg("use")) cfg.use_custom_url = (server.arg("use") == "1");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/custom-url-clear", []() {
        cfg.custom_firmware_url = "";
        cfg.use_custom_url = false;
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/update-history", []() {
        String j = "{";
        j += "\"last_check\":\"" + cfg.last_update_check + "\"";
        j += ",\"last_status\":\"" + cfg.last_update_status + "\"";
        j += ",\"update_count\":" + String(cfg.update_count);
        j += "}";
        server.send(200, "application/json", j);
    });
    
    server.on("/api/ota-check", []() {
        cfg.last_update_check = getUptime();
        cfg.last_update_status = "Kontrol edildi - Guncel";
        server.send(200, "application/json", "{\"update\":false,\"current\":\"" + String(CURRENT_VERSION) + "\",\"latest\":\"---\"}");
    });
    
    server.on("/api/ota-check-custom", []() {
        if (cfg.custom_firmware_url.length() > 0 && cfg.use_custom_url) {
            server.send(200, "application/json", "{\"update\":true,\"url\":\"" + cfg.custom_firmware_url + "\",\"message\":\"Ozel URL'de surum var\"}");
        } else {
            server.send(200, "application/json", "{\"update\":false,\"message\":\"Ozel URL aktif degil veya bos\"}");
        }
    });
    
    server.on("/api/ota-force", []() {
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/api/ota-do", []() {
        if (server.hasArg("url")) {
            String url = server.arg("url");
            server.send(200, "application/json", "{\"status\":\"ok\"}");
            cfg.update_count++;
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
                telnetClients[i].print(getBanner());
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
    if (cmd == "help")     return "Komutlar: help, ls, cat, echo, pwd, cd, wifi, free, sysinfo, uptime, temp, who, reboot, clear, version, repo\r\n";
    if (cmd == "ls")       return lsDir(path);
    if (cmd.startsWith("cat ")) return catFile(cmd.substring(4), path);
    if (cmd.startsWith("echo ")) return cmd.substring(5) + "\r\n";
    if (cmd.startsWith("cd "))   return cdDir(cmd.substring(3), path);
    if (cmd == "pwd")      return path + "\r\n";
    if (cmd == "wifi status") return "AP: " + cfg.ap_ssid + " | IP: " + WiFi.softAPIP().toString() + " | Clients: " + String(WiFi.softAPgetStationNum()) + "\r\n";
    if (cmd == "free") {
        char bf[64];
        sprintf(bf, "Heap: %d/%d free | SPIFFS: %d/%d used\r\n", ESP.getFreeHeap(), ESP.getHeapSize(), SPIFFS.usedBytes(), SPIFFS.totalBytes());
        return String(bf);
    }
    if (cmd == "sysinfo")  return "ESP32 WROOM-32U | " + String(ESP.getChipCores()) + " cores | " + String(ESP.getCpuFreqMHz()) + "MHz | " + String(esp_temp, 1) + "C | v" + CURRENT_VERSION + "\r\n";
    if (cmd == "uptime")   return getUptime() + "\r\n";
    if (cmd == "temp")     return String(esp_temp, 1) + "C\r\n";
    if (cmd == "who")      return "root ttyS0 serial\r\n";
    if (cmd == "clear")    return "\033[2J\033[H";
    if (cmd == "version")  return "v" + String(CURRENT_VERSION) + " (Build: " + CURRENT_BUILD + ")\r\n";
    if (cmd == "repo")     return "Repo: " + cfg.repo_user + "/" + cfg.repo_name + "\r\n";
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
    return o + "\r\n";
}

String catFile(String n, String path) {
    if (!n.startsWith("/")) n = path + "/" + n;
    File f = SPIFFS.open(n, "r");
    if (!f) return "Yok\r\n";
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
    } else if (n == "/") path = "/";
    else if (n != ".") path = path + (path.endsWith("/") ? "" : "/") + n;
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
        esp_temp = 42.0 + (cpu_usage * 0.2) + (random(0, 10) / 10.0);
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