# Claw Drone

![Claw Drone](https://cdn.hackclub.com/019f14cf-6d57-7323-a0e8-c7fb3de276a7/Screenshot%202026-06-29%20151536.png)

Claw Drone is a custom payload quadcopter with four continuous-rotation claw
servos to fly, carry, and manipulate objects. It has a handheld controller with
two joysticks, trim buttons, a TFT dashboard, and direct claw controls.
Pilot commands are sent via ESP-NOW Long Range to the aircraft, where an ESP32
converts them to CRSF for the Betaflight flight controller and drives all
four servos.

This repository holds the mechanical CAD and complete custom firmware for the
controller and aircraft. Betaflight runs on standard firmware, so it is not
included here.

The project started as an attempt to make a cheap drone with an ESP32 flight
controller and low-cost ESCs. The fourth prototype crashed a few times and it
was decided to move flight-critical controls to a conventional Betaflight
stack. The experimental components – a four-leg claw, ESP-NOW radio link, CRSF
bridge, telemetry, and custom transmitter – are still custom.

## Demo

[Watch the competition flight demo on YouTube](https://youtube.com/shorts/Rw3jiggKQBY)

## System architecture

```text
Joysticks and buttons
        |
        v
Raspberry Pi Pico -- UART 115200 --> Controller ESP32-WROOM
                                           |
                                           | ESP-NOW LR, channel 6
                                           v
                                    Drone ESP32-WROOM
                                      |           |
                           CRSF UART3 |           | 4 x 50 Hz PWM
                                      v           v
                              Betaflight FC    Claw servos
                                      |
                                      v
                                ESC and motors
```

The controller ESP32 manages the 320x240 TFT dashboard and hosts the Wi-Fi
servo-control web page. The drone ESP32 sends CRSF battery telemetry to the
controller via the same ESP-NOW connection.

## Repository layout

```text
CAD/
  Drone+Frame.stp
Code/
  controller/
    pico/                 MicroPython joystick and button firmware
    esp32_tx/             ESP-IDF UART-to-ESP-NOW transmitter and dashboard
  drone/
    esp32_rx/             ESP-IDF ESP-NOW-to-CRSF receiver and servo PWM
  diagnostics/
    pico_joystick_button_test/
```

See [Code/README.md](Code/README.md) for build entry points.

## Design and build process

### 1. Early arm concepts

The first design was a conventional X-frame with a five-degree-of-freedom arm.
This design was abandoned because of the arm weight. The second idea was a
frame with electronics close to its perimeter and an opening in the center.
A servo-driven rope and spring system would tighten around the payload through
this opening. Printed prototype of this concept revealed two major problems:

* The gripper was too close to the airframe and could grip a payload only
  when the drone was nearly touching it.
* Obstructed propeller downwash due to the frame and claw parts reduced
  thrust, efficiency, and flight time.

### 2. Lightweight claw redesign

The project returned to a conventional 5-inch X-frame and a lightweight claw
underneath. The final payload system consists of four continuous-rotation
servos arranged symmetrically and long legs with cup-shaped ends. This gives
large payload capture area without the mass of a multi-axis robotic arm.

![Early claw prototype](https://cdn.hackclub.com/019ec355-8233-7346-a96f-261aa930281d/Screenshot%202026-06-13%20193231.png)

![Redesigned claw CAD](https://cdn.hackclub.com/019ec35d-74e1-70d2-9615-82af2e30d687/image.png)

### 3. ESP32 flight-controller experiment

The first flight controller prototype was based on ESP32-S3 SuperMini board
and a BMI160 IMU over SPI. The sensor was mounted, soldered, and calibrated
while testing ESP-FC and Betaflight-compatible configuration tool.

A 4-in-1 ESC refused to arm and spin the motors reliably with DSHOT, ONESHOT,
and PWM protocols despite many tries with different wiring and pin assignments.
One ESP32 also crashed during ESC testing, probably due to an electrical fault
on the ESC signal connection.

Next, four low-cost individual ESCs were tested. They armed and spun the
motors, but their approximate 50 Hz update rate was too low for the control of
a responsive 5-inch quadcopter. Multiple attempts to arm and fly resulted in
failed takeoffs and propellers breakage.

![ESP32 and ESC development](https://cdn.hackclub.com/019ecc0f-3c6b-7371-a2ad-fd34ee2507ea/image.png)

### 4. Moving flight control to Betaflight

The experimental ESP32 flight controller and low-frequency ESCs were replaced
by a conventional flight controller and 45 A ESC stack documented below.
It increased the reliability of the flight-critical system while leaving the
payload, receiver bridge, telemetry, and transmitter open for experimenting.

![Flight-controller stack](https://cdn.hackclub.com/019ed383-7922-7b6d-8e36-17c1cc8e7390/image.png)

### 5. Temporary browser controller

Until the custom transmitter was ready, there was a test with an ESP32
receiver which connected to the flight controller over UART. Two virtual
joysticks in a browser of a phone proved that the airframe and receiver
worked. Wi-Fi latency and lack of video made it unsuitable for a final
controller, but the test helped to validate the basic aircraft.

The test interface was based on
[cifertech/ESP32-Drone](https://github.com/cifertech/ESP32-Drone).

### 6. Building the custom transmitter

Firstly, NRF24L01 modules were considered. Adding a 100 µF decoupling
capacitor improved power stability, but the link was unstable. The final system
uses ESP-NOW Long Range and external antennas.

The transmitter box accommodates joystick spacing, reachable controls, wire
management, and expansion space. The Raspberry Pi Pico manages the physical
controls while the ESP32 manages the TFT, ESP-NOW link, telemetry, web
controls, and external UART.

![Transmitter wiring](https://cdn.hackclub.com/019f1057-e119-775f-ac5c-63eac06e99c7/image.png)

![Completed transmitter](https://cdn.hackclub.com/019f1058-59bb-790c-9c65-282e0644dde1/image.png)

### 7. Final assembly

The claws, XL4005 power regulator, receiver, camera hardware, and printed
mounts were attached to the 5-inch frame. Payload servos stay on the drone
ESP32 and not on the flight-controller motor outputs to keep the claw
independent from the timing-critical motor control system.

![Final electronics assembly](https://cdn.hackclub.com/019f148b-7dcf-7bab-8370-dddf9c211c3d/image.png)

![Drone with claw installed](https://cdn.hackclub.com/019f148d-098e-77a8-bafd-c0c3c1cc8c31/image.png)

## Controller pinout

### Raspberry Pi Pico

Joystick axes are analog inputs. All buttons are active-low: one side of the
button goes to listed GPIO while another side connects to Pico GND. Firmware
configures the internal pull-up resistors for all buttons.

| Pico pin    | Connection             | Function                                                     |
| ----------- | ---------------------- | ------------------------------------------------------------ |
| GP0         | Button to GND          | Arm; accepted only with the throttle stick centered          |
| GP1         | Button to GND          | Immediate disarm and throttle reset                          |
| GP2         | Button to GND          | Pitch-back trim                                              |
| GP3         | Button to GND          | Roll-right trim                                              |
| GP4         | Button to GND          | Roll-left trim                                               |
| GP5         | Button to GND          | Pitch-forward trim                                           |
| GP6         | Not used               | Reserved                                                     |
| GP7         | Not used               | Reserved                                                     |
| GP8         | Button to GND          | Decrease held throttle by 0.5%                               |
| GP9         | Button to GND          | Increase held throttle by 0.5%                               |
| GP10        | Button to GND          | Rotate all four claw servos clockwise while held             |
| GP11        | Button to GND          | Rotate all four claw servos counterclockwise while held      |
| GP12        | ESP32 GPIO16           | UART0 TX to ESP32 RX2                                        |
| GP13        | ESP32 GPIO17           | UART0 RX from ESP32 TX2                                      |
| GP14        | Left stick button      | Decrease control sensitivity by 5%                           |
| GP15        | Right stick button     | Increase control sensitivity by 5%                           |
| GP25        | Onboard LED            | Solid with recent radio delivery; blinking when disconnected |
| GP26 / ADC0 | Left joystick X        | Yaw                                                          |
| GP27 / ADC1 | Left joystick Y        | Spring-centered throttle increase/decrease                   |
| GP28 / ADC2 | Right joystick Y       | Pitch                                                        |
| GP29 / ADC3 | Right joystick X       | Roll                                                         |
| 3V3         | Joystick VCC           | Use 3.3 V to never exceed 3.3 V on ADC inputs                |
| GND         | All controller grounds | Common signal ground                                         |

Trim values are reset to zero on Pico reboot. Roll and pitch trim values have
step of 15 units with limits of -250 to +250. Initial sensitivity is set to
50% and adjustable from 25% to 100%.

Spring-centered throttle value does not correspond to an absolute throttle
position. Moving it upwards increases a held throttle value, holding keeps it,
moving downwards decreases. Disarming sets a held throttle value to zero.

### Pico to controller ESP32 UART

| Raspberry Pi Pico | Controller ESP32-WROOM |
| ----------------- | ---------------------- |
| GP12 UART0 TX     | GPIO16 UART2 RX        |
| GP13 UART0 RX     | GPIO17 UART2 TX        |
| GND               | GND                    |

UART parameters are 115200 baud, 8 data bits, no parity, and 1 stop bit.

### Controller TFT and touch

| Display signal | Controller ESP32 GPIO |
| -------------- | --------------------- |
| TFT CS         | GPIO15                |
| TFT DC         | GPIO2                 |
| TFT RST        | GPIO4                 |
| SPI SCLK       | GPIO18                |
| SPI MOSI       | GPIO23                |
| SPI MISO       | GPIO19                |
| Touch CS       | GPIO21                |
| Touch IRQ      | GPIO22                |
| GND            | GND                   |

Controller ESP32 has a 2.8-inch, 320x240 ILI9341 TFT with XPT2046 touch
controller. Dashboard shows trim, held throttle, sensitivity, arm command,
radio-link status, and flight battery telemetry. Touch is wired to ESP32,
but not used by the dashboard firmware. All ESP32 SPI signals use 3.3 V logic.

GPIO2 and GPIO15 are ESP32 boot-strapping pins. Display wiring ensures proper
booting level of both pins.

## Airframe and propulsion

| Component            | Specification                                        |
| -------------------- | ---------------------------------------------------- |
| Base frame           | Standard 5-inch FPV quadcopter frame                 |
| Flight stack         | Aero Selfie F405 with 45 A ESC                       |
| Motors 1-3           | 2306, 2450 KV                                        |
| Motor 4              | 2306, 2500 KV                                        |
| Propeller diameter   | 5 inches                                             |
| Propeller type       | Low-to-moderate-pitch commercial or 3D-printed props |
| Claw servos          | 4 x continuous-rotation SG90                         |
| Servo regulator      | XL4005 adjusted to 4.5 V                             |
| Main power connector | XT60                                                 |

The standard 5-inch FPV frame holds a custom claw payload, servos mounts, dual
battery system, and onboard receiver electronics in the CAD.

Aircraft has three 2450 KV motors and one 2500 KV motor. Betaflight adjusts
thrust differences in closed-loop flight, but motor temperature, balance, and
full-throttle stability check are a part of the pre-flight procedure.

Commercial 5-inch props are flight baseline. The 3D-printed props are an
experimental component and require balancing, layer inspection, and a gentle
low-throttle spin-test.

## Four-leg claw system

The payload system of the drone uses four SG90 servos arranged symmetrically
underneath the propellers. All servos are facing inwards towards the center
of the airframe and controlling one stick-like claw leg. Each leg has a
cup-shaped end which holds the payload.

Thus, all four legs make a compact spider-leg mechanism:

1. Legs spread from the center creating clearance around the payload.
2. All four legs move out from under the motors.
3. Cup-shaped ends of legs converge from four sides to the payload.
4. Four contact points retain and stabilize the payload under the airframe.

Controller moves all four servos together while maintaining the inversion
needed by opposite polarity of their installation. Web interface splits them
into two opposite pairs for easier setup and testing. Neutral PWM values are
stored for each leg.

## Battery and power system

The aircraft uses two 4S LiPo batteries connected in parallel.

| Battery configuration     | System value                                    |
| ------------------------- | ----------------------------------------------- |
| Cell count                | 4S                                              |
| Nominal bus voltage       | 14.8 V                                          |
| Fully charged bus voltage | 16.8 V                                          |
| Standard packs            | 2 x 1500 mAh = 3000 mAh                         |
| Alternate packs           | 2 x 1550 mAh = 3100 mAh                         |
| Connection                | Parallel: voltage remains 4S and capacities add |
| Main connector            | XT60                                            |

The parallel battery bus supplies the propulsion system and the XL4005 UBEC.
The XL4005 is adjusted to 4.5 V and powers all four SG90 servos.

Both parallel batteries should have equal cell count, chemistry, capacity, and
charge level. Never connect batteries in parallel with different voltages.
Parallel harness is rated to handle the total aircraft current.

## Drone pinout

### Drone ESP32 to Betaflight flight controller

| Drone ESP32-WROOM | Flight controller |
| ----------------- | ----------------- |
| GPIO12 UART2 TX   | RX3               |
| GPIO14 UART2 RX   | TX3               |
| GND               | GND               |

Link transmits CRSF protocol with 420000 baud speed. GPIO12 is boot-strapping
pin on the ESP32 and flight-controller RX3 remains high-impedance during
ESP32 reset.

### Drone ESP32 servo outputs

| Servo   | PWM signal pin |
| ------- | -------------- |
| Servo 1 | ESP32 GPIO16   |
| Servo 2 | ESP32 GPIO17   |
| Servo 3 | ESP32 GPIO5    |
| Servo 4 | ESP32 GPIO18   |

Receiver generates 50 Hz PWM signal with pulse width constrained to 500-2500 us.
Inversion logic is taken into account for opposite polarity of the two pairs of
servos: Servo 1 and Servo 3 share one polarity while Servo 2 and Servo 4 use
the other one.

Motor outputs are connected to the Betaflight FC `M1`-`M4` outputs and the
custom ESP32 firmware does not control motor outputs.

### Servo power

Servos are powered by the UBEC, not by the ESP32 GPIO or 3.3 V pin.

```text
UBEC +V -> all servo positive power wires
UBEC GND -> all servo ground wires
UBEC GND -> drone ESP32 GND and flight-controller GND
ESP32 GPIO16/17/5/18 -> individual servo signal wires
```

UBEC, servos, drone ESP32 and flight controller share a common ground to have
PWM pulses referenced to the same voltage level. UBEC +V connects only to the
servo power wires, not to the ESP32 GPIO and 3.3 V pin. Servo current should
not flow through the ESP32 or flight controller.

## ESP-NOW radio configuration

Both ESP32s use ESP-NOW unicast with Espressif Long Range PHY at 250 Kbit/s
and Wi-Fi channel 6.

The checked-in firmware is paired to these boards:

| Device           | Wi-Fi STA MAC       |
| ---------------- | ------------------- |
| Controller ESP32 | `68:09:47:5c:04:c4` |
| Drone ESP32      | `68:09:47:5c:2f:8c` |

To replace either ESP32, update both hard-coded peer addresses:

* `drone_mac` in
  `Code/controller/esp32_tx/main/main.c`
* `controller_mac` in
  `Code/drone/esp32_rx/main/main.c`

Control packets have a sequence number and CRC. Loss of valid control packets
for 500 ms makes all axes neutral, throttle zero, and arm low.

## Servo web controls

Controller ESP32 hosts this access point:

| Setting  | Value                 |
| -------- | --------------------- |
| SSID     | `Drone-Servo-TX`      |
| Password | `servo-control`       |
| Address  | `http://192.168.4.1/` |

Mobile page allows to set neutral values for all four continuous-rotation
servos, 10-500 us rotation stretch, and CW/CCW hold buttons for each of them.
Direction command starts when pressing and returns to neutral when releasing.

Slider value does not change startup defaults. `Set Startup Default` saves all
four neutral values and a stretch to the flash memory. There is a 1.5 seconds
timeout of commands.

## Betaflight setup

The project uses standard Betaflight firmware on the Aero Selfie F405 flight
controller. 45 A ESC from the stack drives the four 2306 motors. No special
Betaflight firmware modification is needed.

Aircraft flight controller gets CRSF on UART3 with this configuration:

```text
serial 1 0 115200 57600 0 115200
serial 2 64 115200 57600 0 115200
set serialrx_provider = CRSF
set serialrx_inverted = OFF
set serialrx_halfduplex = OFF
save
```

On this STM32F405 target, `serial 2` corresponds to UART3. Ports tab has
`Serial RX` enabled on UART3. The receiver channel map is:

| CRSF channel | Function               |
| ------------ | ---------------------- |
| CH1          | Roll                   |
| CH2          | Pitch                  |
| CH3          | Throttle               |
| CH4          | Yaw                    |
| CH5 / AUX1   | Arm                    |
| CH6 / AUX2   | Link-active indication |

ARM mode uses AUX1. Directions and endpoints of channels are verified in the
Receiver tab.

The flight controller sends CRSF battery frames via TX3 port and drone ESP32
reads them and sends to the controller TFT via ESP-NOW link.

## Basic operation

1. Power the controller with the throttle stick centered.
2. Power the aircraft and wait until Pico link LED becomes solid.
3. Verify that the Receiver tab works properly without propellers installed.
4. Press GP0 to request arm. Betaflight arms only if all its arming checks pass.
5. Move the spring-centered throttle upwards to increase throttle value and hold
   it.
6. Press GP1 at any time to request disarm and set throttle value to zero.

## Competition results

Drone flew at the competition, but picking objects with the claw was hard and
the custom transmitter was not convenient. Mechanism itself worked as a
prototype, but it was not competitive in the first competition.

Nevertheless, it validated many parts of the design:

* Custom ESP-NOW transmitter can control Betaflight via ESP32-to-CRSF bridge.
* CRSF battery telemetry can travel in the opposite direction.
* Custom handheld controller is reusable for other robotics projects.
* A four-leg claw is suitable for integration into 5-inch airframe.
* Separate control of experimental payload improves reliability.
* Stable flight alone is not enough for precise object pickup.

## Problems and lessons learned

### ESC protocol and update rate matters

First individual ESCs accepted only about 50 Hz updates which was not
suitable for a responsive 5-inch quadcopter. F405/45 A stack and a right
protocol was a much better flight platform.

### Flight controllers are safety-critical

While it is possible to make a simple controller to read IMU and calculate PID
output, a flight controller is a complex device with filtering, vibration
management, deterministic timing, calibration, failsafes, and fault handling.
Moving responsibility for these to Betaflight allowed to focus on the rest of
the system.

### Power integrity matters

NRF24L01 modules were sensitive to supply noises and one ESP32 crash occurred
during ESC development. Independent regulation, local decoupling, proper
grounding, and power protection should be added in future versions.

### Prototype the full interaction

Claw had a large payload capture area, but positioning the aircraft
correctly for it was a much harder task. A mechanical pickup test should
include realistic transmitter ergonomics, latency, and pilot visibility.
