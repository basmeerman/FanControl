#pragma once

// Embedded single-page UI (HTML + inline CSS + inline JS) served from
// GET / by webserver.cpp. Stored in PROGMEM to save RAM.
//
// Hard rules (CLAUDE.md / plan §F3):
//   - No external CDN references.
//   - Dark theme, mobile-first (>=380 px), accordion sections.
//   - Vanilla JS only.
//   - All visible labels English (v1.2 decision).
//
// The raw-string delimiter is HTMLPG to avoid clashing with any quoted
// parens in the content.

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLPG(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FanControl</title>
<style>
:root{
  --bg:#1a1a1a;--panel:#242424;--panel2:#2c2c2c;--border:#3a3a3a;
  --text:#e0e0e0;--muted:#9aa0a6;--accent:#3aa3ff;
  --danger:#e74c3c;--success:#27ae60;--warn:#f1c40f;
}
*{box-sizing:border-box}
html,body{margin:0;padding:0;background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  font-size:15px;line-height:1.4;min-width:380px}
header{padding:14px 16px;background:#111;border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;gap:12px}
header h1{margin:0;font-size:18px;font-weight:600;color:var(--accent)}
header .wsstate{font-size:12px;color:var(--muted)}
header .wsstate.ok{color:var(--success)}
header .wsstate.bad{color:var(--danger)}
main{padding:12px;max-width:760px;margin:0 auto}
details{background:var(--panel);border:1px solid var(--border);
  border-radius:8px;margin-bottom:10px;overflow:hidden}
details>summary{padding:12px 14px;cursor:pointer;font-weight:600;
  list-style:none;user-select:none;display:flex;justify-content:space-between;
  align-items:center}
details>summary::-webkit-details-marker{display:none}
details>summary::after{content:"+";color:var(--muted);font-size:20px;line-height:1}
details[open]>summary::after{content:"-"}
.section-body{padding:12px 14px;border-top:1px solid var(--border)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
@media (max-width:520px){.grid{grid-template-columns:1fr}}
.tile{background:var(--panel2);border:1px solid var(--border);
  border-radius:6px;padding:10px}
.tile .k{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.tile .v{font-size:18px;font-weight:600;margin-top:4px;word-break:break-all}
.bar{height:10px;background:#1a1a1a;border-radius:5px;overflow:hidden;margin-top:6px;border:1px solid var(--border)}
.bar>div{height:100%;background:var(--accent);width:0%;transition:width .4s}
.pill{display:inline-block;padding:3px 10px;border-radius:999px;
  font-size:12px;font-weight:600;margin-right:6px;margin-top:4px;
  background:#333;color:var(--muted)}
.pill.ok{background:rgba(39,174,96,.18);color:var(--success)}
.pill.bad{background:rgba(231,76,60,.22);color:var(--danger)}
label{display:block;font-size:12px;color:var(--muted);margin-top:10px}
input[type=text],input[type=password],input[type=number],select{
  width:100%;padding:8px 10px;margin-top:4px;background:#1a1a1a;
  color:var(--text);border:1px solid var(--border);border-radius:5px;font-size:14px}
input:focus,select:focus{outline:none;border-color:var(--accent)}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.curve-row{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:6px}
button{background:var(--accent);color:#000;border:none;padding:10px 16px;
  border-radius:5px;font-weight:600;font-size:14px;cursor:pointer;margin-top:14px}
button.secondary{background:#444;color:var(--text)}
button.danger{background:var(--danger);color:#fff}
button:disabled{opacity:.5;cursor:not-allowed}
.hint{font-size:12px;color:var(--muted);margin-top:6px}
.banner{background:var(--danger);color:#fff;padding:10px 14px;border-radius:6px;
  margin-bottom:10px;font-weight:600;display:none}
.banner.show{display:block}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);
  background:#333;color:var(--text);padding:10px 16px;border-radius:6px;
  border:1px solid var(--border);opacity:0;transition:opacity .25s;z-index:99;
  font-size:13px;max-width:90vw}
.toast.show{opacity:1}
.toast.ok{border-color:var(--success)}
.toast.bad{border-color:var(--danger)}
.small{font-size:12px;color:var(--muted)}
</style>
</head>
<body>
<header>
  <h1>FanControl</h1>
  <span class="wsstate" id="wsstate">connecting...</span>
</header>
<main>
  <div class="banner" id="pwdbanner">You must set a new OTA password before OTA updates will be accepted.</div>

  <details open>
    <summary>Status</summary>
    <div class="section-body">
      <div class="grid">
        <div class="tile"><div class="k">Temperature</div><div class="v" id="s_temp">-</div></div>
        <div class="tile"><div class="k">Humidity</div><div class="v" id="s_hum">-</div></div>
        <div class="tile">
          <div class="k">Fan speed</div>
          <div class="v" id="s_fan">-</div>
          <div class="bar"><div id="s_fanbar"></div></div>
        </div>
        <div class="tile"><div class="k">Uptime</div><div class="v" id="s_up">-</div></div>
        <div class="tile"><div class="k">MQTT</div><div class="v" id="s_mqtt">-</div></div>
        <div class="tile"><div class="k">WiFi</div><div class="v" id="s_wifi">-</div></div>
      </div>
      <div style="margin-top:10px">
        <span class="k small">Alarms:</span>
        <span class="pill" id="a_temp">Temp</span>
        <span class="pill" id="a_sensor">Sensor</span>
        <span class="pill" id="a_wdt">Watchdog</span>
      </div>
      <div class="hint" id="s_restarts">Restarts: -</div>
    </div>
  </details>

  <details>
    <summary>Ventilation settings</summary>
    <div class="section-body">
      <div class="small">Fan curve (5 points). Below T1 uses P1, above T5 forces 100%.</div>
      <div id="curve"></div>

      <div class="row">
        <div><label>Alarm temperature (C)<input type="number" id="c_alarm" step="0.5"></label></div>
        <div><label>Min fan speed (%)<input type="number" id="c_min" min="0" max="100"></label></div>
      </div>
      <div class="row">
        <div><label>Sensor interval (ms)<input type="number" id="c_interval" min="1000" max="60000" step="500"></label></div>
        <div><label>PWM frequency (Hz)<input type="number" id="c_freq" min="1000" max="5000" step="100"></label></div>
      </div>
      <button id="btn_fan">Save ventilation</button>
    </div>
  </details>

  <details>
    <summary>Network and MQTT settings</summary>
    <div class="section-body">
      <div class="small">Saving these restarts the device.</div>
      <label>WiFi SSID<input type="text" id="n_ssid"></label>
      <label>WiFi password<input type="password" id="n_wpwd" placeholder="leave empty to keep"></label>
      <div class="row">
        <div><label>MQTT host<input type="text" id="n_mhost"></label></div>
        <div><label>MQTT port<input type="number" id="n_mport" min="1" max="65535"></label></div>
      </div>
      <div class="hint">Port 8883 enables TLS automatically (no certificate validation).</div>
      <div class="row">
        <div><label>MQTT user<input type="text" id="n_muser"></label></div>
        <div><label>MQTT password<input type="password" id="n_mpwd" placeholder="leave empty to keep"></label></div>
      </div>
      <div class="row">
        <div><label>MQTT prefix<input type="text" id="n_mprefix"></label></div>
        <div><label>Device name<input type="text" id="n_dev"></label></div>
      </div>
      <button id="btn_net">Save network</button>
    </div>
  </details>

  <details>
    <summary>System</summary>
    <div class="section-body">
      <div class="grid">
        <div class="tile"><div class="k">Firmware</div><div class="v" id="s_ver">-</div></div>
        <div class="tile"><div class="k">Build</div><div class="v" id="s_build">-</div></div>
        <div class="tile"><div class="k">Free heap</div><div class="v" id="s_heap">-</div></div>
        <div class="tile"><div class="k">Total restarts</div><div class="v" id="s_rst">-</div></div>
      </div>
      <label>New OTA password (min 8 chars)<input type="password" id="o_pwd"></label>
      <div class="hint" id="o_note">OTA disabled until you change the password from the default.</div>
      <button id="btn_ota">Save OTA password</button>
      <div style="margin-top:14px">
        <a href="/update" target="_blank" id="otalink">Open OTA updater</a>
      </div>
      <div style="margin-top:14px;display:flex;gap:8px;flex-wrap:wrap">
        <button class="secondary" id="btn_restart">Restart</button>
        <button class="danger"    id="btn_factory">Factory reset</button>
      </div>
    </div>
  </details>
</main>
<div class="toast" id="toast"></div>

<script>
(function(){
var N=5;
var ws=null, wsTimer=null, firstStatus=true;
var $=function(id){return document.getElementById(id);};

function setWsState(s, ok){
  var el=$("wsstate"); el.textContent=s;
  el.className="wsstate "+(ok===true?"ok":ok===false?"bad":"");
}
function toast(msg, ok){
  var t=$("toast"); t.textContent=msg;
  t.className="toast show "+(ok?"ok":"bad");
  setTimeout(function(){t.className="toast";},2500);
}
function fmtUptime(s){
  var d=Math.floor(s/86400), h=Math.floor(s%86400/3600),
      m=Math.floor(s%3600/60), sec=s%60;
  function p(n){return(n<10?"0":"")+n;}
  return p(d)+":"+p(h)+":"+p(m)+":"+p(sec);
}
function fmtNum(v, unit, digits){
  if(v===null||v===undefined||isNaN(v)) return "-";
  return (digits===undefined?v:Number(v).toFixed(digits))+(unit||"");
}
function pill(id, active){
  var el=$(id); el.className="pill "+(active?"bad":"ok");
}

function buildCurveInputs(){
  var box=$("curve"); box.innerHTML="";
  for(var i=0;i<N;i++){
    var row=document.createElement("div");
    row.className="curve-row";
    row.innerHTML=
      '<label>T'+(i+1)+' (C)<input type="number" id="ct'+i+'" step="0.5"></label>'+
      '<label>P'+(i+1)+' (%)<input type="number" id="cp'+i+'" min="0" max="100"></label>';
    box.appendChild(row);
  }
}

function applyStatus(d){
  $("s_temp").textContent = fmtNum(d.temperature, " C", 1);
  $("s_hum").textContent  = fmtNum(d.humidity, " %", 1);
  $("s_fan").textContent  = fmtNum(d.fan_speed, " %", 0);
  $("s_fanbar").style.width = (Math.max(0,Math.min(100,d.fan_speed||0)))+"%";
  $("s_up").textContent   = fmtUptime(d.system.uptime||0);

  var m=d.mqtt||{};
  $("s_mqtt").textContent = (m.connected?"connected":"disconnected")+
                            (m.broker?(" @ "+m.broker):"");
  var w=d.wifi||{};
  $("s_wifi").textContent = (w.portal?"AP ":"")+
                            (w.ssid||"-")+
                            (w.rssi?(" "+w.rssi+" dBm"):"")+
                            (w.ip?(" "+w.ip):"");

  var a=d.alarm||{};
  pill("a_temp", a.temperature);
  pill("a_sensor", a.sensor);
  pill("a_wdt", a.watchdog);

  var sys=d.system||{};
  $("s_restarts").textContent = "Restarts: "+sys.restarts;
  $("s_ver").textContent   = sys.version||"-";
  $("s_build").textContent = sys.build_date||"-";
  $("s_heap").textContent  = sys.heap_free?(sys.heap_free+" B"):"-";
  $("s_rst").textContent   = sys.restarts!==undefined?sys.restarts:"-";

  // First-boot password banner: shown until they save a new one.
  var banner=$("pwdbanner");
  if(sys.first_boot_change_password){
    banner.classList.add("show");
    $("otalink").style.pointerEvents="none";
    $("otalink").style.opacity=".5";
    $("o_note").textContent="OTA disabled until you change the password from the default.";
  } else {
    banner.classList.remove("show");
    $("otalink").style.pointerEvents="";
    $("otalink").style.opacity="";
    $("o_note").textContent="OTA enabled. Use the link below with user 'admin' and the password you set.";
  }
  firstStatus=false;
}

function connect(){
  setWsState("connecting...", null);
  try{
    ws = new WebSocket((location.protocol==="https:"?"wss://":"ws://")+location.host+"/ws");
  } catch(e){
    setWsState("error", false);
    scheduleReconnect();
    return;
  }
  ws.onopen=function(){ setWsState("connected", true); };
  ws.onclose=function(){ setWsState("disconnected", false); scheduleReconnect(); };
  ws.onerror=function(){ setWsState("error", false); };
  ws.onmessage=function(ev){
    try{
      var d=JSON.parse(ev.data);
      if(d.type==="status") applyStatus(d);
      else if(d.type==="saved"){
        if(d.ok){
          toast("Saved: "+d.section+(d.reboot_required?" (restarting...)":""), true);
        } else {
          toast("Save failed: "+(d.error||"unknown"), false);
        }
      } else if(d.type==="log"){
        // no-op UI for now; reserved for debug log section
      }
    } catch(e){ console.error(e); }
  };
}
function scheduleReconnect(){
  if(wsTimer) return;
  wsTimer=setTimeout(function(){ wsTimer=null; connect(); }, 2000);
}

function send(obj){
  if(!ws||ws.readyState!==1){ toast("Not connected", false); return false; }
  ws.send(JSON.stringify(obj));
  return true;
}

function saveFan(){
  var thresholds=[], levels=[];
  for(var i=0;i<N;i++){
    var t=parseFloat($("ct"+i).value);
    var p=parseInt($("cp"+i).value, 10);
    if(isNaN(t)||isNaN(p)){ toast("Fill in all curve points", false); return; }
    thresholds.push(t); levels.push(p);
  }
  send({
    type:"set_fan",
    thresholds:thresholds,
    pwm_levels:levels,
    alarm_temp:parseFloat($("c_alarm").value),
    min_fan:parseInt($("c_min").value,10),
    sensor_interval_ms:parseInt($("c_interval").value,10),
    pwm_freq_hz:parseInt($("c_freq").value,10)
  });
}
function saveNet(){
  send({
    type:"set_network",
    wifi:{ssid:$("n_ssid").value, password:$("n_wpwd").value},
    mqtt:{
      host:$("n_mhost").value,
      port:parseInt($("n_mport").value,10)||1883,
      user:$("n_muser").value,
      password:$("n_mpwd").value,
      prefix:$("n_mprefix").value||"fancontrol"
    },
    device_name:$("n_dev").value||"fancontrol"
  });
}
function saveOta(){
  var p=$("o_pwd").value||"";
  if(p.length<8){ toast("Password must be at least 8 chars", false); return; }
  send({type:"set_ota_password", password:p});
  $("o_pwd").value="";
}
function doRestart(){
  if(!confirm("Restart the controller now?")) return;
  send({type:"restart"});
}
function doFactory(){
  if(!confirm("Factory reset will wipe all settings and restart. Continue?")) return;
  send({type:"factory_reset", confirm:true});
}

window.addEventListener("DOMContentLoaded", function(){
  buildCurveInputs();
  // Seed defaults so the form isn't empty before the first save cycle.
  var defT=[15,20,25,30,35], defP=[10,25,50,75,100];
  for(var i=0;i<N;i++){ $("ct"+i).value=defT[i]; $("cp"+i).value=defP[i]; }
  $("c_alarm").value=35; $("c_min").value=10;
  $("c_interval").value=5000; $("c_freq").value=1000;
  $("n_mport").value=1883; $("n_mprefix").value="fancontrol";
  $("n_dev").value="fancontrol";

  $("btn_fan").addEventListener("click", saveFan);
  $("btn_net").addEventListener("click", saveNet);
  $("btn_ota").addEventListener("click", saveOta);
  $("btn_restart").addEventListener("click", doRestart);
  $("btn_factory").addEventListener("click", doFactory);
  connect();
});
})();
</script>
</body>
</html>
)HTMLPG";
