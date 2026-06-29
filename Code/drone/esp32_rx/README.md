# Drone ESP32: ESP-NOW to CRSF receiver

This is the drone-side receiver. It receives control frames over
ESP-NOW and outputs CRSF RC frames to the flight controller.

Wiring:

| ESP32 WROOM | Flight controller |
| -------- | ----------------- |
| GPIO12 TX | FC RX3 |
| GPIO14 RX | FC TX3 |
| GND | GND |

Both ESP32s use unicast ESP-NOW on Wi-Fi channel 6 with Espressif Long Range
PHY at 250 Kbit/s. The transmitter is configured for this receiver's STA MAC:
`68:09:47:5c:2f:8c`.

The FC TX3 return wire is parsed for CRSF battery sensor frames. Valid battery
voltage and remaining-capacity percentage are returned to controller STA MAC
`68:09:47:5c:04:c4` over ESP-NOW for the TFT dashboard. No estimate is shown
when the FC does not provide a valid battery telemetry frame.

This firmware drives four direct ESP32 servo outputs at 50 Hz. They start
centered at 1500 us:

| Servo | ESP32 GPIO |
| --- | --- |
| Servo 1 | GPIO16 |
| Servo 2 | GPIO17 |
| Servo 3 | GPIO5 |
| Servo 4 | GPIO18 |

The matching transmitter web interface sends separate CRC-protected continuous
servo packets. Each output accepts 500-2500 us and applies a new pulse width
immediately. Saved neutral values and the selected 10-500 us rotation stretch
are stored in NVS and restored before PWM starts after a reboot. If servo
commands stop for 1.5 seconds, every output returns to its saved neutral value.

Betaflight UART3 configuration:

```text
serial 1 0 115200 57600 0 115200
serial 2 64 115200 57600 0 115200
set serialrx_provider = CRSF
set serialrx_inverted = OFF
set serialrx_halfduplex = OFF
save
```

`GPIO12` is an ESP32 boot-strapping pin. The FC RX input should be
high-impedance, but an external pull-up on this signal can prevent the ESP32
from booting. If that occurs, disconnect GPIO12 during reset or move CRSF TX
to a non-strapping GPIO and update `CRSF_TX_PIN`.

Build/flash:

```sh
cd Code/drone/esp32_rx
pio run -t upload
pio device monitor
```
