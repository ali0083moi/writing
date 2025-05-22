@echo off
echo This script will help you upload the SPIFFS files to your ESP32.
echo Make sure your ESP32 is connected to your computer.
echo.
echo Follow these steps:
echo 1. In Arduino IDE, go to Tools > ESP32 Sketch Data Upload
echo    (If you don't see this option, install ESP32 filesystem uploader from:
echo     https://github.com/me-no-dev/arduino-esp32fs-plugin)
echo 2. Wait for the upload to complete
echo 3. Then upload the main sketch as usual
echo.
echo Press any key to continue...
pause > nul
echo.
echo Opening Arduino IDE... (You may need to open it manually if it doesn't start)
start "" "%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\3.0.0/esptool.exe" --chip esp32 --port [YOUR_PORT] --baud 921600 write_flash -z 0x290000 ./data
echo.
echo Don't forget to install these libraries if you haven't already:
echo - ArduinoJson
echo - AsyncTCP
echo - ESPAsyncWebServer
echo.
echo Press any key to exit...
pause > nul 