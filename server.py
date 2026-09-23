"""Mouse 3D: a system-tray app that receives touch/IMU gestures from a
phone browser over WebSocket and replays them as mouse/keyboard input for
Fusion 360.

Run:  py -3.12 server.py  (or the packaged Mouse3D.exe)
It shows up as a tray icon near the clock; click it for a QR code that
opens https://<pc-ip>:8000 on the phone (same Wi-Fi / hotspot). The
self-signed TLS certificate is generated automatically on first run —
HTTPS is required for the DeviceOrientation/Generic Sensor (IMU) APIs to
work in modern mobile browsers.
"""
import asyncio
import sys
import time
from pathlib import Path

# Two different base directories, because a PyInstaller --onefile build
# extracts bundled read-only assets (static/) to a temp dir that's wiped
# between runs, but things we write ourselves (cert, QR code, log) need to
# live somewhere persistent next to the actual .exe.
if getattr(sys, "frozen", False):
    APP_DIR = Path(sys.executable).parent  # persistent: next to the .exe
    BUNDLE_DIR = Path(getattr(sys, "_MEIPASS", APP_DIR))  # read-only bundled assets
else:
    APP_DIR = Path(__file__).parent
    BUNDLE_DIR = APP_DIR

# When launched via pythonw.exe (no console, for a clean tray-only app),
# or as a --windowed PyInstaller build, sys.stdout/stderr are None — any
# print() would crash the whole app. Send that output to a log file
# instead, before anything else can print.
if sys.stdout is None or sys.stderr is None:
    _log = open(APP_DIR / "mouse3d.log", "a", buffering=1, encoding="utf-8")
    sys.stdout = _log
    sys.stderr = _log

import pyautogui
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

pyautogui.PAUSE = 0
pyautogui.FAILSAFE = False

STATIC_DIR = BUNDLE_DIR / "static"

app = FastAPI()
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

# Fusion 360 default navigation bindings. Adjust here if your Fusion 360
# mouse mapping differs (Preferences > General > Design > Mouse mapping).
NAV_MODES = {
    "orbit": {"button": "middle", "modifiers": ["shift"]},
    "pan": {"button": "middle", "modifiers": []},
}


@app.get("/")
def index():
    return FileResponse(
        STATIC_DIR / "index.html",
        headers={
            "Cache-Control": "no-store",
            # Explicitly allow the Generic Sensor API for this origin, in
            # case the phone's Chrome build requires it even though the
            # default allowlist ("self") should already cover a top-level
            # same-origin page.
            "Permissions-Policy": (
                "accelerometer=(self), gyroscope=(self), "
                "magnetometer=(self), ambient-light-sensor=(self)"
            ),
        },
    )


class DragState:
    def __init__(self):
        self.active_mode = None
        self.zoom_accum = 0.0

    def start(self, mode: str):
        if mode not in NAV_MODES or self.active_mode is not None:
            return
        cfg = NAV_MODES[mode]
        for key in cfg["modifiers"]:
            pyautogui.keyDown(key)
        if cfg["modifiers"]:
            # Give Fusion 360 time to register the modifier before the
            # button-down, otherwise the drag can start as a plain pan
            # (modifier not yet seen) instead of orbit.
            time.sleep(0.05)
        pyautogui.mouseDown(button=cfg["button"])
        self.active_mode = mode

    def move(self, dx: float, dy: float):
        if self.active_mode is None:
            return
        pyautogui.moveRel(dx, dy, duration=0)

    def end(self):
        if self.active_mode is None:
            return
        cfg = NAV_MODES[self.active_mode]
        pyautogui.mouseUp(button=cfg["button"])
        if cfg["modifiers"]:
            time.sleep(0.03)
        for key in cfg["modifiers"]:
            pyautogui.keyUp(key)
        self.active_mode = None

    def zoom(self, delta: float):
        # Accumulate fractional scroll so small pinch deltas (in either
        # direction) are never lost to int() truncation.
        self.zoom_accum += delta
        clicks = int(self.zoom_accum)
        if clicks:
            pyautogui.scroll(clicks)
            self.zoom_accum -= clicks


STALE_SESSION_TIMEOUT = 2.0  # seconds with no message while a drag is held


@app.websocket("/ws")
async def ws_endpoint(websocket: WebSocket):
    await websocket.accept()
    drag = DragState()
    try:
        while True:
            try:
                msg = await asyncio.wait_for(
                    websocket.receive_json(), timeout=STALE_SESSION_TIMEOUT
                )
            except asyncio.TimeoutError:
                # Safety net: if the page hangs, crashes, or the network
                # drops without a clean disconnect, never leave Shift or a
                # mouse button stuck held down on the PC.
                if drag.active_mode is not None:
                    print("[server] watchdog: releasing stale session", flush=True)
                    drag.end()
                continue
            mtype = msg.get("type")
            if mtype == "start":
                drag.start(msg.get("mode"))
            elif mtype == "move":
                drag.move(msg.get("dx", 0), msg.get("dy", 0))
            elif mtype == "end":
                drag.end()
            elif mtype == "zoom":
                delta = msg.get("delta", 0)
                if delta:
                    drag.zoom(delta)
            elif mtype == "debug":
                print(f"[phone] {msg.get('text', '')}", flush=True)
    except WebSocketDisconnect:
        drag.end()


HOTSPOT_IP = "192.168.137.1"  # fixed by Windows Mobile Hotspot, not configurable
PORT = 8000


def ensure_cert(data_dir: Path):
    """Generate a self-signed TLS cert/key next to the exe if missing, so
    the distributable doesn't need to ship a private key or depend on the
    openssl CLI being installed on the target machine."""
    cert_path = data_dir / "cert.pem"
    key_path = data_dir / "key.pem"
    if cert_path.exists() and key_path.exists():
        return cert_path, key_path

    import datetime
    import ipaddress

    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID

    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = issuer = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "mouse3d-local")])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=825))
        .add_extension(
            x509.SubjectAlternativeName(
                [
                    x509.IPAddress(ipaddress.ip_address(HOTSPOT_IP)),
                    x509.IPAddress(ipaddress.ip_address("127.0.0.1")),
                    x509.DNSName("localhost"),
                ]
            ),
            critical=False,
        )
        .sign(key, hashes.SHA256())
    )

    key_path.write_bytes(
        key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption(),
        )
    )
    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    print(f"[server] generated self-signed TLS certificate in {data_dir}", flush=True)
    return cert_path, key_path


def make_tray_image():
    """Small blue 'orbit' icon: a ring with a dot, for the system tray."""
    from PIL import Image, ImageDraw

    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((6, 6, 58, 58), fill=(53, 103, 224, 255))
    d.ellipse((16, 22, 48, 42), outline=(255, 255, 255, 255), width=3)
    d.ellipse((28, 28, 36, 36), fill=(255, 255, 255, 255))
    return img


def run_server():
    import uvicorn

    cert_path, key_path = ensure_cert(APP_DIR)
    # Pass the app object directly (not the "server:app" import string) so
    # this doesn't depend on the module being importable by name, which
    # isn't reliable inside a PyInstaller --onefile build.
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=PORT,
        ssl_certfile=str(cert_path),
        ssl_keyfile=str(key_path),
        log_level="info",
    )


def run_tray_app():
    """The tray icon is the whole app's controller: launching it starts the
    server (background thread) and generates the QR code; clicking the icon
    shows a small window with it; "Quit" stops everything."""
    import queue
    import threading
    import tkinter as tk

    import pystray
    from PIL import Image, ImageTk

    url = f"https://{HOTSPOT_IP}:{PORT}"
    qr_path = APP_DIR / "connect_qr.png"
    try:
        import qrcode

        qrcode.make(url).save(qr_path)
    except Exception as e:
        print(f"(failed to generate QR code: {e})", flush=True)

    threading.Thread(target=run_server, daemon=True).start()

    root = tk.Tk()
    root.withdraw()  # no main window — the tray icon is the whole UI
    root.title("Mouse 3D - Fusion 360")

    ui_queue: "queue.Queue[str]" = queue.Queue()
    qr_window = {"win": None}

    def show_qr_window():
        win = qr_window["win"]
        if win is not None and win.winfo_exists():
            win.deiconify()
            win.lift()
            win.focus_force()
            return
        win = tk.Toplevel(root)
        win.title("Connect Mouse 3D")
        win.resizable(False, False)
        win.attributes("-topmost", True)
        img = Image.open(qr_path)
        photo = ImageTk.PhotoImage(img)
        img_label = tk.Label(win, image=photo)
        img_label.image = photo  # keep a reference so it isn't garbage collected
        img_label.pack(padx=18, pady=(18, 8))
        tk.Label(win, text=url, font=("Consolas", 11)).pack(pady=(0, 6))
        tk.Label(
            win,
            text="Connect your phone to this PC's hotspot and scan.",
            font=("Segoe UI", 9),
            fg="#555",
        ).pack(pady=(0, 16))
        win.protocol("WM_DELETE_WINDOW", win.withdraw)
        qr_window["win"] = win

    def poll_queue():
        try:
            while True:
                cmd = ui_queue.get_nowait()
                if cmd == "show_qr":
                    show_qr_window()
                elif cmd == "quit":
                    icon.stop()
                    root.quit()
                    return
        except queue.Empty:
            pass
        root.after(150, poll_queue)

    def on_show(icon_, item):
        ui_queue.put("show_qr")

    def on_quit(icon_, item):
        ui_queue.put("quit")

    icon = pystray.Icon(
        "mouse3d",
        make_tray_image(),
        "Mouse 3D - Fusion 360",
        menu=pystray.Menu(
            pystray.MenuItem("Show QR Code", on_show, default=True),
            pystray.MenuItem("Quit", on_quit),
        ),
    )
    threading.Thread(target=icon.run, daemon=True).start()

    root.after(150, poll_queue)
    root.mainloop()


if __name__ == "__main__":
    run_tray_app()
