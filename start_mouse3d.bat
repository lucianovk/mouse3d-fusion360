@echo off
cd /d "%~dp0"

rem Launches silently (no console window) via pythonw.exe — the app lives
rem entirely as a tray icon near the clock from here on. Output that would
rem normally print to console goes to mouse3d.log instead (see server.py).
rem Uses whichever "pythonw" is on PATH (e.g. an activated venv). If it's
rem not found, falls back to a normal console window via the py launcher.
where pythonw >nul 2>nul
if %errorlevel%==0 (
    start "" pythonw server.py
) else (
    py -3 server.py
)
