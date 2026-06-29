# Claw Drone

![Claw Drone](https://cdn.hackclub.com/019f14cf-6d57-7323-a0e8-c7fb3de276a7/Screenshot%202026-06-29%20151536.png)

Claw Drone is a custom payload quadcopter built to fly, carry, and manipulate
objects with four continuous-rotation claw servos. Its handheld controller
combines two joysticks, dedicated trim controls, a TFT dashboard, and direct
claw controls. Pilot commands travel over ESP-NOW Long Range to the aircraft,
where an ESP32 converts them to CRSF for the Betaflight flight controller and
generates all four servo PWM signals.

This repository contains the mechanical CAD and the complete custom firmware
for the controller and aircraft. The flight controller runs standard
Betaflight, so the upstream Betaflight source is not duplicated here.

The project began as an attempt to make an inexpensive drone around an ESP32
flight controller and low-cost ESCs. After several prototypes and failed
flight tests, the flight-critical controls moved to a conventional Betaflight
stack. The experimental parts—the four-leg claw, ESP-NOW radio link, CRSF
bridge, telemetry, and custom transmitter—remained custom-built.

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

The controller ESP32 drives the 320x240 TFT dashboard and hosts the Wi-Fi
servo-control page. The drone ESP32 sends CRSF battery telemetry back to the
controller over the same ESP-NOW link.

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

The first design was a normal X-frame carrying a five-degree-of-freedom arm.
That approach was rejected because the arm would be too heavy for the target
airframe.

The next concept moved the electronics toward the frame perimeter and left an
opening in the center. A servo-driven rope and spring system would tighten
around an object through that opening. A printed prototype exposed two major
problems:

- The gripper sat too close to the airframe and would only work when the drone
  was almost touching the object.
- Frame members and claw parts obstructed the propeller downwash, reducing
  thrust, efficiency, and flight time.

### 2. Lightweight claw redesign

The project returned to a conventional 5-inch X-frame and a lightweight claw
mounted beneath it. The final mechanism uses four symmetrically arranged
continuous-rotation servos and long claw legs with cup-shaped ends. This gives
the gripper a large capture area without the mass of a multi-axis robotic arm.

![Early claw prototype](https://cdn.hackclub.com/019ec355-8233-7346-a96f-261aa930281d/Screenshot%202026-06-13%20193231.png)

![Redesigned claw CAD](https://cdn.hackclub.com/019ec35d-74e1-70d2-9615-82af2e30d687/image.png)

### 3. ESP32 flight-controller experiment

The original flight-controller prototype used an ESP32-S3 SuperMini and a
BMI160 IMU over SPI. The sensor was mounted, soldered, and calibrated while
testing ESP-FC and Betaflight-compatible configuration tools.

A 4-in-1 ESC would not initialize reliably from the ESP32 using DSHOT,
ONESHOT, or PWM despite multiple wiring and pin-assignment changes. One ESP32
also failed during ESC testing, possibly because of an electrical fault on the
ESC signal connection.

Four inexpensive individual ESCs were tested next. They armed and spun the
motors, but their approximately 50 Hz update rate was too slow for stable
control of a responsive 5-inch quadcopter. Takeoff attempts repeatedly tipped
or crashed the aircraft and broke propellers while PID tuning was attempted.

![ESP32 and ESC development](https://cdn.hackclub.com/019ecc0f-3c6b-7371-a2ad-fd34ee2507ea/image.png)

### 4. Moving flight control to Betaflight

The experimental ESP32 flight controller and slow ESCs were replaced by the
Aero Selfie F405 and 45 A ESC stack documented below. This made the
flight-critical system substantially more robust while leaving the payload,
receiver bridge, telemetry, and transmitter open for experimentation.

![Flight-controller stack](https://cdn.hackclub.com/019ed383-7922-7b6d-8e36-17c1cc8e7390/image.png)

### 5. Temporary browser controller

Before the handheld transmitter was complete, an ESP32 receiver connected to
the flight controller over UART was tested using a temporary ESP32 web server.
Two virtual joysticks in a phone browser proved that the airframe and receiver
interface worked. Wi-Fi and video latency made this unsuitable as the final
control system, but the test validated the basic aircraft.

The temporary test interface drew on
[cifertech/ESP32-Drone](https://github.com/cifertech/ESP32-Drone).

### 6. Building the custom transmitter

NRF24L01 modules were considered first. A 100 µF decoupling capacitor improved
their power stability, but the link remained unreliable, so the final system
moved to ESP-NOW Long Range with external antennas.

The transmitter enclosure was designed around joystick spacing, reachable
controls, clean wiring, and expansion room. A Raspberry Pi Pico handles the
physical controls while the ESP32 handles the TFT, ESP-NOW link, telemetry,
web controls, and external UART.

![Transmitter wiring](https://cdn.hackclub.com/019f1057-e119-775f-ac5c-63eac06e99c7/image.png)

![Completed transmitter](https://cdn.hackclub.com/019f1058-59bb-790c-9c65-282e0644dde1/image.png)

### 7. Final assembly

The claw servos, XL4005 regulator, receiver, camera hardware, and printed
mounts were fitted to the 5-inch frame. The payload servos stayed on the drone
ESP32 instead of the flight-controller outputs, keeping the claw independent
from the timing-critical motor control path.

![Final electronics assembly](https://cdn.hackclub.com/019f148b-7dcf-7bab-8370-dddf9c211c3d/image.png)

![Drone with claw installed](https://cdn.hackclub.com/019f148d-098e-77a8-bafd-c0c3c1cc8c31/image.png)

## Controller pinout

### Raspberry Pi Pico

The joystick axes are analog inputs. Every button is active-low: connect one
side of the button to the listed GPIO and the other side to Pico GND. The
firmware enables the internal pull-up resistors.

| Pico pin | Connection | Function |
| --- | --- | --- |
| GP0 | Button to GND | Arm; accepted only with the throttle stick centered |
| GP1 | Button to GND | Immediate disarm and throttle reset |
| GP2 | Button to GND | Pitch-back trim |
| GP3 | Button to GND | Roll-right trim |
| GP4 | Button to GND | Roll-left trim |
| GP5 | Button to GND | Pitch-forward trim |
| GP6 | Not used | Reserved |
| GP7 | Not used | Reserved |
| GP8 | Button to GND | Decrease held throttle by 0.5% |
| GP9 | Button to GND | Increase held throttle by 0.5% |
| GP10 | Button to GND | Rotate all four claw servos clockwise while held |
| GP11 | Button to GND | Rotate all four claw servos counterclockwise while held |
| GP12 | ESP32 GPIO16 | UART0 TX to ESP32 RX2 |
| GP13 | ESP32 GPIO17 | UART0 RX from ESP32 TX2 |
| GP14 | Left stick button to GND | Decrease control sensitivity by 5% |
| GP15 | Right stick button to GND | Increase control sensitivity by 5% |
| GP25 | Onboard LED | Solid with recent radio delivery; blinking when disconnected |
| GP26 / ADC0 | Left joystick X | Yaw |
| GP27 / ADC1 | Left joystick Y | Spring-centered throttle increase/decrease |
| GP28 / ADC2 | Right joystick Y | Pitch |
| GP29 / ADC3 | Right joystick X | Roll |
| 3V3 | Joystick VCC | Use 3.3 V so ADC inputs never exceed 3.3 V |
| GND | All controller grounds | Common signal ground |

Trim values reset to zero whenever the Pico restarts. Roll and pitch trim use
15-unit steps with a limit of -250 to +250. Sensitivity starts at 50% and is
adjustable from 25% to 100%.

The spring-centered throttle does not directly represent an absolute throttle
position. Moving it up increases a held throttle value, returning it to center
holds that value, and moving it down decreases it. Disarming always resets the
held throttle to zero.

### Pico to controller ESP32 UART

| Raspberry Pi Pico | Controller ESP32-WROOM |
| --- | --- |
| GP12 UART0 TX | GPIO16 UART2 RX |
| GP13 UART0 RX | GPIO17 UART2 TX |
| GND | GND |

UART settings are 115200 baud, 8 data bits, no parity, and 1 stop bit.

### Controller TFT and touch

| Display signal | Controller ESP32 GPIO |
| --- | --- |
| TFT CS | GPIO15 |
| TFT DC | GPIO2 |
| TFT RST | GPIO4 |
| SPI SCLK | GPIO18 |
| SPI MOSI | GPIO23 |
| SPI MISO | GPIO19 |
| Touch CS | GPIO21 |
| Touch IRQ | GPIO22 |
| GND | GND |

The controller uses a 2.8-inch, 320x240 ILI9341 TFT with an XPT2046 touch
controller. The dashboard displays trim, held throttle, sensitivity, arm
command, radio-link state, and flight-battery telemetry. Touch hardware is
connected but is not used by the dashboard firmware. All ESP32 SPI signals use
3.3 V logic.

GPIO2 and GPIO15 are ESP32 boot-strapping pins. The display wiring leaves both
signals at valid boot levels during reset.

## Airframe and propulsion

| Component | Specification |
| --- | --- |
| Base frame | Standard 5-inch FPV quadcopter frame |
| Flight stack | Aero Selfie F405 with 45 A ESC |
| Motors 1-3 | 2306, 2450 KV |
| Motor 4 | 2306, 2500 KV |
| Propeller diameter | 5 inches |
| Propeller type | Low-to-moderate-pitch commercial or 3D-printed props |
| Claw servos | 4 x continuous-rotation SG90 |
| Servo regulator | XL4005 adjusted to 4.5 V |
| Main power connector | XT60 |

The standard 5-inch FPV frame carries the custom claw mechanism, servo mounts,
dual-battery system, and onboard receiver electronics represented in the CAD.

The aircraft deliberately uses three 2450 KV motors and one 2500 KV motor.
Betaflight compensates for the small thrust difference during closed-loop
flight, while motor temperature, output balance, and full-throttle stability
remain part of the preflight inspection.

Commercial 5-inch props are the flight baseline. The 3D-printed props are
experimental parts and require individual balance, layer inspection, and a
restrained low-throttle spin test before use.

## Four-leg claw system

The payload claw uses four SG90 servos arranged symmetrically beneath the four
propeller motors. Every servo faces inward toward the center of the airframe
and actuates one stick-like claw leg. Each leg ends in a cup-shaped contact
surface that helps hold the payload instead of relying on a single narrow
gripping point.

Together, the four legs form a compact spider-leg mechanism:

1. The legs open away from the center to create clearance around the payload.
2. All four legs extend inward from beneath the motor positions.
3. The cup-shaped ends converge around the payload from four directions.
4. The four contact points retain and stabilize the payload beneath the
   aircraft during lift.

The controller moves all four servos together while preserving the individual
servo direction inversions required by their mirrored installation. The web
interface also divides them into two opposing pairs for setup and testing.
Saved neutral PWM values define the resting position of each leg.

## Battery and power system

The aircraft uses two 4S LiPo batteries connected in parallel.

| Battery configuration | System value |
| --- | --- |
| Cell count | 4S |
| Nominal bus voltage | 14.8 V |
| Fully charged bus voltage | 16.8 V |
| Standard packs | 2 x 1500 mAh = 3000 mAh |
| Alternate packs | 2 x 1550 mAh = 3100 mAh |
| Connection | Parallel: voltage remains 4S and capacities add |
| Main connector | XT60 |

The parallel battery bus powers the propulsion system and the XL4005 UBEC. The
XL4005 is adjusted to 4.5 V and provides the separate regulated supply for all
four SG90 servos.

Both parallel packs must have the same cell count, chemistry, capacity, and
state of charge before connection. Never connect two packs in parallel at
different voltages. The parallel harness must be rated for the aircraft's full
motor current.

## Drone pinout

### Drone ESP32 to Betaflight flight controller

| Drone ESP32-WROOM | Flight controller |
| --- | --- |
| GPIO12 UART2 TX | RX3 |
| GPIO14 UART2 RX | TX3 |
| GND | GND |

The link runs CRSF at 420000 baud. GPIO12 is an ESP32 boot-strapping pin, so the
flight-controller RX3 input remains high-impedance during ESP32 reset.

### Drone ESP32 servo outputs

| Servo | PWM signal pin |
| --- | --- |
| Servo 1 | ESP32 GPIO16 |
| Servo 2 | ESP32 GPIO17 |
| Servo 3 | ESP32 GPIO5 |
| Servo 4 | ESP32 GPIO18 |

The receiver generates 50 Hz PWM. Pulse widths are constrained to 500-2500 us.
The paired direction logic accounts for the opposing servo installations:
Servos 1 and 3 use one polarity and Servos 2 and 4 use the inverse polarity.

The flight motors remain connected to the flight controller's normal
`M1`-`M4` motor/ESC outputs. The custom ESP32 firmware does not drive motor
outputs.

### Servo power

Power the servos from the external UBEC, not from an ESP32 GPIO or 3.3 V pin.

```text
UBEC +V  -> all servo positive power wires
UBEC GND -> all servo ground wires
UBEC GND -> drone ESP32 GND and flight-controller GND
ESP32 GPIO16/17/5/18 -> individual servo signal wires
```

The UBEC, servos, drone ESP32, and flight controller share a common ground so
the PWM signals have the same electrical reference. UBEC positive connects
only to the servo power wires, never to an ESP32 GPIO or 3.3 V pin. Servo
current does not pass through the ESP32 or flight controller.

## ESP-NOW radio configuration

Both ESP32s use unicast ESP-NOW on Wi-Fi channel 6 with Espressif Long Range
PHY at 250 Kbit/s.

The checked-in firmware is paired to these boards:

| Device | Wi-Fi STA MAC |
| --- | --- |
| Controller ESP32 | `68:09:47:5c:04:c4` |
| Drone ESP32 | `68:09:47:5c:2f:8c` |

When replacing either ESP32, update both hard-coded peer addresses:

- `drone_mac` in
  `Code/controller/esp32_tx/main/main.c`
- `controller_mac` in
  `Code/drone/esp32_rx/main/main.c`

The flight-control packet includes a sequence number and CRC. Loss of valid
control packets for 500 ms forces neutral axes, zero throttle, and arm low.

## Servo web controls

The controller ESP32 creates this access point:

| Setting | Value |
| --- | --- |
| SSID | `Drone-Servo-TX` |
| Password | `servo-control` |
| Address | `http://192.168.4.1/` |

The mobile page provides neutral values for all four continuous-rotation
servos, a 10-500 us rotation stretch, and paired CW/CCW hold buttons. Direction
commands take effect on press and return to neutral on release.

Changing a slider does not change the startup default. `Set Startup Default`
saves all four neutral values and the stretch to flash. A 1.5-second command
timeout returns every output to its saved neutral value.

## Betaflight setup

This project uses standard Betaflight on the Aero Selfie F405 flight
controller. The stack's 45 A ESC drives the four 2306 motors. No modified
Betaflight source is required.

The aircraft flight controller receives CRSF on UART3 with this configuration:

```text
serial 1 0 115200 57600 0 115200
serial 2 64 115200 57600 0 115200
set serialrx_provider = CRSF
set serialrx_inverted = OFF
set serialrx_halfduplex = OFF
save
```

On this STM32F405 Betaflight target, `serial 2` is UART3. The Ports tab has
`Serial RX` enabled on UART3. The receiver channel map is:

| CRSF channel | Function |
| --- | --- |
| CH1 | Roll |
| CH2 | Pitch |
| CH3 | Throttle |
| CH4 | Yaw |
| CH5 / AUX1 | Arm |
| CH6 / AUX2 | Link-active indication |

The ARM mode uses AUX1. Channel direction and endpoints are checked in the
Receiver tab before flight.

The flight controller sends CRSF battery frames through TX3. The drone ESP32
parses those frames and returns battery voltage and remaining capacity to the
controller TFT over ESP-NOW.

## Basic operation

1. Power the controller with the throttle stick centered.
2. Power the aircraft and wait for the Pico link LED to become solid.
3. Confirm the Receiver tab responds correctly with propellers removed.
4. Press GP0 to request arm. Betaflight will arm only if all of its arming
   checks pass.
5. Move the spring-centered throttle upward to increase and release it to hold.
6. Press GP1 at any time to request disarm and reset throttle to zero.

## Competition results

The drone flew at the competition, but the claw was difficult to position and
the custom transmitter made precise pickup control harder than expected. The
mechanism worked as a prototype, but it was not competitive in its first
event.

The build still validated several parts of the design:

- A custom ESP-NOW transmitter can control Betaflight through an
  ESP32-to-CRSF bridge.
- CRSF battery telemetry can make the return trip to a custom controller.
- The Pico/ESP32 handheld controller is reusable for other robotics projects.
- A lightweight four-leg claw can be integrated into a 5-inch airframe.
- Keeping experimental payload controls separate from flight-critical control
  improves reliability.
- Stable flight alone is not enough for precise aerial object pickup; pilot
  ergonomics, latency, visibility, and mechanism geometry must be tested
  together.

## Problems and lessons learned

### ESC protocol and update rate matter

The first individual ESCs accepted only approximately 50 Hz control updates.
That was not suitable for a responsive 5-inch quadcopter. The F405/45 A stack
with an appropriate motor protocol was a much better flight platform.

### Flight controllers are safety-critical

A basic controller can be made to read an IMU and calculate PID output, but a
reliable flight controller also needs filtering, vibration management,
deterministic timing, calibration, failsafes, and fault handling. Moving those
responsibilities to Betaflight allowed the custom work to focus on the radio,
telemetry, controller, and payload.

### Power integrity matters

The NRF24L01 tests were sensitive to supply noise, and an ESP32 failed during
ESC development. Future versions should use independently regulated rails,
local decoupling, deliberate grounding, and protection between high-power and
logic systems.

### Prototype the full interaction

The claw had a large capture area, but positioning the aircraft accurately
enough to use it was the harder problem. Mechanical pickup tests should be
performed early with realistic transmitter ergonomics, camera latency, and
pilot visibility.




