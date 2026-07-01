# Firmware


```text
controller/
  pico/
    main.py              Reads controls and sends framed UART packets
  esp32_tx/
    main/main.c          UART bridge, ESP-NOW, servo web server
    main/dashboard.c     TFT dashboard and telemetry display
drone/
  esp32_rx/
    main/main.c          ESP-NOW receiver, CRSF output, servo PWM
diagnostics/
  pico_joystick_button_test/
    main.py              Standalone Pico input test
```

Both ESP32 projects use ESP-IDF through PlatformIO and include their own
`platformio.ini`, CMake files, and `sdkconfig.defaults`. The Pico programs use
MicroPython.


