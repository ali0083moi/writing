#!/bin/bash

echo "This script will help you upload the SPIFFS files to your ESP32."
echo "Make sure your ESP32 is connected to your computer."
echo ""
echo "Follow these steps:"
echo "1. In Arduino IDE, go to Tools > ESP32 Sketch Data Upload"
echo "   (If you don't see this option, install ESP32 filesystem uploader from:"
echo "    https://github.com/me-no-dev/arduino-esp32fs-plugin)"
echo "2. Wait for the upload to complete"
echo "3. Then upload the main sketch as usual"
echo ""
echo "Press Enter to continue..."
read

echo ""
echo "Don't forget to install these libraries if you haven't already:"
echo "- ArduinoJson (search in Library Manager)"
echo "- AsyncTCP (https://github.com/me-no-dev/AsyncTCP)"
echo "- ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)"
echo ""
echo "You can install libraries in Arduino IDE by going to:"
echo "Sketch > Include Library > Manage Libraries... (for ArduinoJson)"
echo "or by downloading ZIP files and installing them manually for:"
echo "AsyncTCP and ESPAsyncWebServer"
echo ""
echo "Press Enter to exit..."
read 