# Pico joystick UART transmitter

Install `main.py` on the Raspberry Pi Pico as `/main.py`. It reads the two
joysticks and active-low stick buttons, then sends a CRC-protected control frame
to the controller ESP32 every 20 ms.

It also sends a local dashboard frame every 200 ms containing the volatile
trims, held throttle, sensitivity, and arm command. The controller ESP32 uses
that frame locally and does not forward it as a flight command.

| Pico | Controller ESP32 |
| --- | --- |
| GP12 UART0 TX | GPIO16 UART2 RX |
| GP13 UART0 RX | GPIO17 UART2 TX |
| GND | GND |

Controls:

- Left X: yaw
- Left Y: spring-centered throttle rate control; center holds the current
  throttle, up increases it, and down decreases it. Full stick changes it by
  20% per second.
- Right X: roll
- Right Y: pitch
- GP0: arm, accepted only while the throttle stick is centered
- GP1: immediate disarm and throttle reset to zero
- GP9/GP8: increase/decrease throttle by 0.5% per press while armed
- GP11: command all four continuous servos CCW while held
- GP10: command all four continuous servos CW while held
- GP5/GP2: trim pitch forward/back
- GP3/GP4: trim roll right/left
- Right-stick button GP15: increase roll/pitch/yaw sensitivity by 5%
- Left-stick button GP14: decrease roll/pitch/yaw sensitivity by 5%
- Axis trims are 1.5% per press, bounded to +/-25%, and reset on restart
- Sensitivity ranges from 25% to 100% and resets to 50% on restart
- GP25 LED solid: recent confirmed ESP-NOW unicast delivery
- GP25 LED blinking: no confirmed delivery for 500 ms

The ESP32 reply is based on its ESP-NOW delivery callback, not merely receipt
of UART bytes.

Install with Thonny closed or disconnected from the Pico serial port:

```sh
cd Code/controller/pico
mpremote connect /dev/ttyACM0 fs cp main.py :main.py
mpremote connect /dev/ttyACM0 reset
```
