@echo off
powershell -Command "Stop-Process -Name node -ErrorAction SilentlyContinue; Start-Process node -ArgumentList '\"%~dp0server.js\"' -WindowStyle Hidden; Start-Sleep -Milliseconds 500; Start-Process 'C:\Program Files\Google\Chrome\Application\chrome.exe' -ArgumentList '--app=http://127.0.0.1:54321'"
