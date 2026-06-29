# Controller ESP32: Pico UART to ESP-NOW

This is the controller-side ESP32-WROOM bridge.

Wiring:

| Pico | ESP32 WROOM |
| ---- | ----------- |
| GP12 UART0 TX | GPIO16 RX2 |
| GP13 UART0 RX | GPIO17 TX2 |
| GND | GND |

The Pico script sends the same CRC-protected 16-byte control payload used by
the nRF path. This ESP32 validates the payload and sends it by unicast ESP-NOW
to drone STA MAC `68:09:47:5c:2f:8c` on channel 6. Both ESP32s use Espressif
Long Range PHY at 250 Kbit/s.

The bridge returns `OK` to the Pico only after the ESP-NOW unicast delivery
callback succeeds. The Pico therefore lights GP25 solid for a recently
confirmed radio link and blinks it when delivery has not been confirmed for
500 ms.

The transmitter also creates a WPA2 servo-control access point:

- SSID: `Drone-Servo-TX`
- Password: `servo-control`
- URL: `http://192.168.4.1/`

The page controls four continuous-rotation servos. The four neutral pulse
widths are adjustable from 500-2500 us and are only persisted by `Set Startup
Default`. The rotation stretch is adjustable from 10-500 us. While a pair's
CW or CCW button is held, one servo receives neutral plus the stretch and its
inverted partner receives neutral minus the stretch. Releasing the button
immediately restores that pair's neutral values. Both pairs support simultaneous
mobile touch input. Servo commands use a separate CRC-protected ESP-NOW packet,
so the Pico flight packet format remains unchanged.
The drone must run the matching `esp32_espnow_crsf_rx` firmware.

The controller dashboard uses an ILI9341-compatible 320x240 TFT in landscape
mode. The XPT2046 touch controller is initialized, but the dashboard is
currently display-only.

| Display signal | ESP32 GPIO |
| --- | --- |
| TFT CS | GPIO15 |
| TFT DC | GPIO2 |
| TFT RST | GPIO4 |
| SPI SCLK | GPIO18 |
| SPI MOSI | GPIO23 |
| SPI MISO | GPIO19 |
| Touch CS | GPIO21 |
| Touch IRQ | GPIO22 |

The display shows the Pico's volatile roll, pitch, and yaw trims, held
throttle, sensitivity, arm command, ESP-NOW delivery status, and flight battery
voltage. `ARM COMMAND` is the command sent to Betaflight, not confirmation that
the flight controller actually armed. Battery voltage remains `NO DATA` until
the receiver obtains a valid CRSF battery sensor frame from the FC.

This bridge intentionally uses ESP-IDF rather than MicroPython. Standard
MicroPython ESP-NOW is fast enough for 50 Hz control packets, but does not
provide the same reliable access to per-peer LR PHY configuration and delivery
callbacks used here.

Build/flash:

```sh
cd Code/controller/esp32_tx
pio run -t upload
pio device monitor
```
