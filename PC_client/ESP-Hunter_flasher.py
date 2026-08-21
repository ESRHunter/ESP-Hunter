import subprocess as sp
import os, sys
import requests
from serial.tools import list_ports

from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QComboBox, QTextEdit
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal, QPoint

# Настройки репозитория
GITHUB_REPO = "ESRHunter/ESP-Hunter"

def resourcePath(relative_path):
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("."), relative_path)

def scanComPorts():
    return [p.device for p in list_ports.comports()]

class UploadThread(QThread):
    log = pyqtSignal(str)
    finished = pyqtSignal(int)

    def __init__(self, comPort):
        super().__init__()
        self.comPort = comPort

    def get_latest_release_info(self):
        try:
            api_url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
            response = requests.get(api_url, timeout=10)
            if response.status_code != 200:
                return None, None
            
            data = response.json()
            for asset in data.get('assets', []):
                if asset['name'].endswith(".bin"):
                    return asset['browser_download_url'], asset['name']
            return None, None
        except Exception:
            return None, None

    def run(self):
        fw_dir = resourcePath("firmware")
        if not os.path.exists(fw_dir):
            os.makedirs(fw_dir)

        esptool = os.path.join(fw_dir, "esptool.exe")
        
        # 1. Проверка и скачивание прошивки с GitHub
        self.log.emit("[SYS] Connecting to GitHub API...\n")
        download_url, filename = self.get_latest_release_info()
        
        target_bin = os.path.join(fw_dir, filename if filename else "latest_firmware.bin")

        if download_url:
            self.log.emit(f"[SYS] Found latest release: {filename}\n")
            self.log.emit(f"[SYS] Downloading firmware... ")
            try:
                r = requests.get(download_url, stream=True)
                if r.status_code == 200:
                    with open(target_bin, 'wb') as f:
                        for chunk in r.iter_content(chunk_size=8192):
                            f.write(chunk)
                    self.log.emit("DONE.\n")
                else:
                    self.log.emit("FAILED (Server Error).\n")
            except Exception as e:
                self.log.emit(f"\n[!] Download error: {e}\n")
        else:
            self.log.emit("[!] Could not find release on GitHub. Looking for local file...\n")

        if not os.path.exists(target_bin):
            self.log.emit(f"[!] No firmware file found at {target_bin}\n")
            self.finished.emit(-1)
            return

        if not os.path.exists(esptool):
            self.log.emit("[!] esptool.exe not found in firmware/ folder.\n")
            self.finished.emit(-1)
            return

        # 2. Запуск прошивки
        cmd = [
            esptool,
            "--chip", "esp32s3",
            "--port", self.comPort,
            "--baud", "1500000",
            "--before", "default-reset",
            "--after", "hard-reset",
            "write-flash",
            "--flash-mode", "dio",
            "--flash-freq", "80m",
            "--flash-size", "keep",
            "0x0",
            target_bin
        ]

        try:
            self.log.emit(f"[SYS] Starting flash process: {filename}\n")
            proc = sp.Popen(
                cmd,
                stdout=sp.PIPE,
                stderr=sp.STDOUT,
                universal_newlines=True,
                creationflags=sp.CREATE_NO_WINDOW
            )

            for line in proc.stdout:
                self.log.emit(line)

            self.finished.emit(proc.wait())

        except Exception as e:
            self.log.emit(f"[!] Exception: {e}\n")
            self.finished.emit(-1)

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.Window)
        self.setFixedSize(580, 420)
        self.oldPos = QPoint()
        self.initUI()
        self.refreshPorts()

    def initUI(self):
        mainLayout = QVBoxLayout()
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.setSpacing(0)

        self.titleBar = QWidget()
        self.titleBar.setObjectName("titleBar")
        self.titleBar.setFixedHeight(40)
        titleLayout = QHBoxLayout()
        titleLayout.setContentsMargins(14, 0, 10, 0)
        self.titleLabel = QLabel(">_ ESP-HUNTER // CLOUD FLASHER")
        self.titleLabel.setObjectName("titleLabel")
        closeBtn = QPushButton("✕")
        closeBtn.setObjectName("closeBtn")
        closeBtn.setFixedSize(30, 26)
        closeBtn.clicked.connect(self.close)
        titleLayout.addWidget(self.titleLabel)
        titleLayout.addStretch()
        titleLayout.addWidget(closeBtn)
        self.titleBar.setLayout(titleLayout)

        content = QWidget()
        content.setObjectName("contentWidget")
        contentLayout = QVBoxLayout()
        contentLayout.setContentsMargins(18, 16, 18, 18)
        contentLayout.setSpacing(12)

        portLayout = QHBoxLayout()
        portLabel = QLabel("COM PORT:")
        portLabel.setObjectName("subLabel")
        self.portCombo = QComboBox()
        self.portCombo.setFixedHeight(34)
        refreshBtn = QPushButton("REFRESH")
        refreshBtn.setObjectName("secBtn")
        refreshBtn.setFixedHeight(34)
        refreshBtn.clicked.connect(self.refreshPorts)
        portLayout.addWidget(portLabel)
        portLayout.addWidget(self.portCombo, 1)
        portLayout.addWidget(refreshBtn)
        contentLayout.addLayout(portLayout)

        self.uploadBtn = QPushButton("⚡ DOWNLOAD & FLASH LATEST")
        self.uploadBtn.setObjectName("uploadBtn")
        self.uploadBtn.setFixedHeight(44)
        self.uploadBtn.clicked.connect(self.startUpload)
        contentLayout.addWidget(self.uploadBtn)

        logLabel = QLabel("CLOUD SYNC & EXECUTION LOG:")
        logLabel.setObjectName("subLabel")
        contentLayout.addWidget(logLabel)

        self.logBox = QTextEdit()
        self.logBox.setObjectName("logBox")
        self.logBox.setReadOnly(True)
        contentLayout.addWidget(self.logBox)

        content.setLayout(contentLayout)
        mainLayout.addWidget(self.titleBar)
        mainLayout.addWidget(content)
        self.setLayout(mainLayout)

        self.setStyleSheet("""
            QWidget { background-color: #050806; color: #e0f2e6; font-family: 'Consolas', monospace; }
            #titleBar { background-color: #0a140d; border-bottom: 1px solid #183822; }
            #titleLabel { color: #00ff66; font-weight: bold; font-size: 13px; }
            #closeBtn { background-color: transparent; color: #7aa387; border: 1px solid #183822; border-radius: 4px; }
            #closeBtn:hover { background-color: #ff4d4d; color: #fff; }
            #subLabel { color: #00ff66; font-size: 11px; font-weight: bold; }
            QComboBox { background-color: #000; color: #00ff66; border: 1px solid #183822; border-radius: 4px; padding-left: 10px; }
            QPushButton#uploadBtn { background-color: rgba(0, 255, 102, 0.1); color: #00ff66; border: 1.5px solid #00ff66; border-radius: 4px; font-weight: bold; }
            QPushButton#uploadBtn:hover { background-color: #00ff66; color: #000; }
            QPushButton#secBtn { background-color: #000; color: #7aa387; border: 1px solid #183822; border-radius: 4px; padding: 0 14px; }
            QTextEdit#logBox { background-color: #000; color: #00ff66; border: 1px solid #183822; border-radius: 4px; font-size: 11px; }
        """)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton and event.pos().y() < 40: self.oldPos = event.globalPos()

    def mouseMoveEvent(self, event):
        if event.buttons() == Qt.LeftButton and event.pos().y() < 40:
            delta = event.globalPos() - self.oldPos
            self.move(self.x() + delta.x(), self.y() + delta.y())
            self.oldPos = event.globalPos()

    def log(self, msg):
        self.logBox.moveCursor(self.logBox.textCursor().End)
        self.logBox.insertPlainText(msg)
        self.logBox.ensureCursorVisible()

    def refreshPorts(self):
        self.portCombo.clear()
        for p in scanComPorts(): self.portCombo.addItem(p)

    def startUpload(self):
        comPort = self.portCombo.currentText()
        if not comPort: return
        self.uploadBtn.setEnabled(False)
        self.thread = UploadThread(comPort)
        self.thread.log.connect(self.log)
        self.thread.finished.connect(self.uploadFinished)
        self.thread.start()

    def uploadFinished(self, code):
        self.uploadBtn.setEnabled(True)
        if code == 0: self.log("\n[+] SUCCESS: Device is ready.\n")
        else: self.log(f"\n[!] ERROR: Process failed (Code {code})\n")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())
