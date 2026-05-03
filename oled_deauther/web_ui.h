#ifndef WEB_UI_H
#define WEB_UI_H
#include <Arduino.h>
#include "WiFiClient.h"

// HTML page split into PROGMEM chunks to avoid large RAM buffers.
// Each chunk is streamed sequentially via sendProgmemPage().

static const char PROGMEM HTML_P1[] =
"<!DOCTYPE html><html lang='en'><head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>5GHZ DEAUTHER</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{background:#000;color:#00ff41;font-family:'Courier New',monospace;font-size:13px;padding:10px}"
"h1{color:#00ff41;text-align:center;font-size:20px;letter-spacing:4px;"
"border-bottom:1px solid #00ff41;padding-bottom:8px;margin-bottom:14px}"
"h2{color:#00cc33;font-size:12px;letter-spacing:2px;margin-bottom:8px;text-transform:uppercase}"
".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
"@media(max-width:600px){.grid{grid-template-columns:1fr}}"
".panel{border:1px solid #00ff41;padding:10px;margin-bottom:10px}"
"table{width:100%;border-collapse:collapse}"
"th{background:#00ff41;color:#000;padding:4px 6px;font-size:11px;text-align:left}"
"td{padding:3px 6px;border-bottom:1px solid #002800;font-size:11px;vertical-align:middle}"
"tr:hover td{background:#001400}"
"tr.selected td{background:#002200;color:#00ff88}"
"button{background:#000;color:#00ff41;border:1px solid #00ff41;padding:5px 10px;"
"font-family:'Courier New',monospace;font-size:12px;cursor:pointer;margin:2px}"
"button:hover{background:#00ff41;color:#000}"
"button.stop{border-color:#ff4400;color:#ff4400}"
"button.stop:hover{background:#ff4400;color:#000}"
"button.tgt{border-color:#009922;color:#009922;font-size:10px;padding:2px 5px}"
"button.tgt:hover{background:#009922;color:#000}"
".sl{display:flex;justify-content:space-between;align-items:center;"
"padding:4px 0;border-bottom:1px solid #001800}"
".sl:last-child{border-bottom:none}"
".lbl{color:#009922}"
".val{color:#00ff41}"
"#ind{text-align:center;font-size:16px;letter-spacing:3px;padding:6px;"
"border:1px solid #ff4400;color:#ff4400;display:none;margin-bottom:8px}"
"#ind.on{display:block;animation:blink 0.8s infinite}"
"@keyframes blink{0%,100%{opacity:1}50%{opacity:0.2}}"
"select,input[type=range]{background:#000;color:#00ff41;border:1px solid #00ff41;"
"padding:3px;font-family:'Courier New',monospace;font-size:12px}"
".bars{display:inline-flex;gap:2px;align-items:flex-end;height:12px;vertical-align:middle}"
".b{width:4px;background:#00ff41;display:inline-block}"
".b.off{background:#002800}"
".atk-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}"
"</style></head><body>"
"<h1>[ 5GHZ DEAUTHER ]</h1>"
"<div id='ind'>!! ATTACK RUNNING !!</div>";

static const char PROGMEM HTML_P2[] =
"<div class='grid'>"
"<div>"
"<div class='panel'>"
"<h2>Networks <span id='nc' style='color:#00ff41'></span></h2>"
"<div style='overflow-x:auto'>"
"<table><thead><tr><th>#</th><th>SSID</th><th>BSSID</th>"
"<th>CH</th><th>RSSI</th><th>Band</th><th></th></tr></thead>"
"<tbody id='ntbl'><tr><td colspan='7' style='text-align:center;padding:10px'>"
"Loading...</td></tr></tbody></table></div>"
"<div style='margin-top:8px'>"
"<button onclick='doScan()'>[ RESCAN ]</button>"
"<span id='scan-status' style='margin-left:8px;color:#009922'></span>"
"</div></div>"
"<div class='panel'>"
"<h2>Settings</h2>"
"<div class='sl'><span class='lbl'>Deauth Count</span>"
"<span><input type='range' id='dc' min='1' max='10' value='3' "
"oninput='document.getElementById(\"dcv\").textContent=this.value' "
"onchange='saveSettings()'>"
"<span id='dcv' class='val'>3</span></span></div>"
"<div class='sl'><span class='lbl'>Scan Time</span>"
"<span><select id='st' onchange='saveSettings()'>"
"<option value='2'>2s</option>"
"<option value='5' selected>5s</option>"
"<option value='10'>10s</option>"
"</select></span></div>"
"<div class='sl'><span class='lbl'>Band Filter</span>"
"<span><select id='bf' onchange='saveSettings()'>"
"<option value='0'>All</option>"
"<option value='1'>2.4G only</option>"
"<option value='2'>5G only</option>"
"</select></span></div>"
"</div></div>";

static const char PROGMEM HTML_P3[] =
"<div>"
"<div class='panel'>"
"<h2>Attack</h2>"
"<div class='atk-grid'>"
"<button onclick='atk(0)'>Single Deauth</button>"
"<button onclick='atk(1)'>All Deauth</button>"
"<button onclick='atk(2)'>Beacon Clone</button>"
"<button onclick='atk(3)'>Random Beacon</button>"
"<button onclick='atk(4)'>Beacon+Deauth</button>"
"<button onclick='atk(5)'>Probe Flood</button>"
"</div>"
"<div style='margin-top:8px'>"
"<button class='stop' style='width:100%' onclick='doStop()'>[ STOP ATTACK ]</button>"
"</div></div>"
"<div class='panel'>"
"<h2>Status</h2>"
"<div class='sl'><span class='lbl'>Device State</span>"
"<span class='val' id='s-st'>-</span></div>"
"<div class='sl'><span class='lbl'>Attacking</span>"
"<span class='val' id='s-atk'>-</span></div>"
"<div class='sl'><span class='lbl'>Attack Type</span>"
"<span class='val' id='s-typ'>-</span></div>"
"<div class='sl'><span class='lbl'>Networks</span>"
"<span class='val' id='s-net'>-</span></div>"
"<div class='sl'><span class='lbl'>Scanning</span>"
"<span class='val' id='s-scn'>-</span></div>"
"<div class='sl'><span class='lbl'>Target</span>"
"<span class='val' id='s-tgt'>-</span></div>"
"</div></div></div>";

static const char PROGMEM HTML_P4[] =
"<script>"
"var ATKS=['Single Deauth','All Deauth','Beacon Clone','Random Beacon','Beacon+Deauth','Probe Flood'];"
"var STATES=['Title','Main Menu','Scanning','Networks','Attack Menu','Attacking','Settings','Info'];"
"var selIdx=-1;"
"function bars(r){"
"var l=r>=-50?4:r>=-65?3:r>=-75?2:r>=-85?1:0;"
"var h='';"
"for(var i=1;i<=4;i++)h+='<div class=\"b'+(i>l?' off':'')+'\" style=\"height:'+(i*3+2)+'px\"></div>';"
"return '<div class=\"bars\">'+h+'</div>';}"
"function fetchNets(){"
"fetch('/networks.json').then(function(r){return r.json();}).then(function(d){"
"document.getElementById('nc').textContent='['+d.length+']';"
"if(!d.length){document.getElementById('ntbl').innerHTML='<tr><td colspan=\"7\" style=\"text-align:center\">No networks found</td></tr>';return;}"
"var h='';"
"d.forEach(function(n){"
"h+='<tr id=\"r'+n.idx+'\"'+(n.idx==selIdx?' class=\"selected\"':'')+'>';"
"h+='<td>'+n.idx+'</td>';"
"h+='<td>'+n.ssid.substring(0,22)+'</td>';"
"h+='<td style=\"font-size:10px\">'+n.bssid+'</td>';"
"h+='<td>'+n.ch+'</td>';"
"h+='<td>'+bars(n.rssi)+'<span style=\"margin-left:3px\">'+n.rssi+'</span></td>';"
"h+='<td>'+n.band+'</td>';"
"h+='<td><button class=\"tgt\" onclick=\"selNet('+n.idx+')\">Target</button></td>';"
"h+='</tr>';});"
"document.getElementById('ntbl').innerHTML=h;"
"}).catch(function(){});}"
"function selNet(i){"
"fetch('/select?idx='+i).then(function(){"
"selIdx=i;"
"document.querySelectorAll('#ntbl tr').forEach(function(r){r.classList.remove('selected');});"
"var row=document.getElementById('r'+i);"
"if(row)row.classList.add('selected');"
"});}"
"function atk(t){fetch('/attack?type='+t);}"
"function doStop(){fetch('/stop');}"
"function doScan(){"
"document.getElementById('scan-status').textContent='Scanning...';"
"fetch('/scan').then(function(){setTimeout(function(){fetchNets();"
"document.getElementById('scan-status').textContent='';},7000);});}"
"function saveSettings(){"
"var dc=document.getElementById('dc').value;"
"var st=document.getElementById('st').value;"
"var bf=document.getElementById('bf').value;"
"fetch('/settings?deauth_count='+dc+'&scan_time='+st+'&band='+bf);}"
"function fetchStatus(){"
"fetch('/status.json').then(function(r){return r.json();}).then(function(d){"
"document.getElementById('s-st').textContent=STATES[d.state]||d.state;"
"document.getElementById('s-atk').textContent=d.attacking?'YES':'NO';"
"document.getElementById('s-typ').textContent=d.attacking?ATKS[d.attack_type]||'-':'-';"
"document.getElementById('s-net').textContent=d.net_count;"
"document.getElementById('s-scn').textContent=d.scanning?'YES':'NO';"
"document.getElementById('s-tgt').textContent=d.target_idx>=0?'#'+d.target_idx:'-';"
"var ind=document.getElementById('ind');"
"ind.className=d.attacking?'on':'';"
"}).catch(function(){});}"
"fetchNets();"
"fetchStatus();"
"setInterval(fetchStatus,3000);"
"</script></body></html>";

// Stream the full page to client in chunks to avoid large RAM buffers.
// Falls back to 64-byte memcpy_P reads if write_P is unavailable.
inline void sendChunk(WiFiClient &client, const char *pgm_str) {
  char buf[64];
  size_t len = strlen_P(pgm_str);
  for (size_t i = 0; i < len; i += 64) {
    size_t chunk = len - i;
    if (chunk > 64) chunk = 64;
    memcpy_P(buf, pgm_str + i, chunk);
    client.write((uint8_t *)buf, chunk);
  }
}

inline void sendProgmemPage(WiFiClient &client) {
  sendChunk(client, HTML_P1);
  sendChunk(client, HTML_P2);
  sendChunk(client, HTML_P3);
  sendChunk(client, HTML_P4);
}

#endif // WEB_UI_H
