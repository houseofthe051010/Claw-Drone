# Claw Drone

![Claw Drone](https://cdn.hackclub.com/019f14cf-6d57-7323-a0e8-c7fb3de276a7/Screenshot%202026-06-29%20151536.png)

This is a payload quadcopter with four continuous-rotation claw
servos to grab objects. I build a custom controller with
two joysticks, trim buttons, a TFT dashboard, and direct claw controls.
The transmitter uses ESPNOW long range to send CRSF commands at 200hz, where another RX es32 onboard converts them into CRSF for the FC to interpret.

The CAD, BOM, and FIRMWARE for both the controller and the drone are in this repo

This project started with an esp32 as the FC, but later changed to using a conventional ESC+FC stack.

## Demo

[Watch the competition flight demo on YouTube](https://youtube.com/shorts/Rw3jiggKQBY)

## System architecture

```text
Joysticks and buttons
        |
        v
Black Pill Pico-compatible board -- UART 115200 --> Controller ESP32-WROOM
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


## Wiring schematics



### Handheld controller schematic

![Handheld controller wiring schematic](Pictures/Controller_Schematic.png)


### Aircraft schematic

![Aircraft wiring schematic](Pictures/Claw_Drone_Schematic.png)



## Repository layout

```text
BOM.csv                      Bill of materials for the aircraft and handheld controller
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
management, and expansion space. The Black Pill Pico-compatible board manages the physical
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

## Bill of materials

This BOM documents the components used in the current build. Prices are the
amount used by the BOM, not necessarily the full multipack purchase price, and
exclude shipping and tax. Marketplace prices were checked on July 23, 2026 and
may change.

### Aircraft

| Quantity | Part | Link | Price |
| -------: | ---- | ---- | ----: |
| 1 | FPV quadcopter frame | [AliExpress](https://www.aliexpress.us/item/3256811569379527.html) | $17.06 |
| 1 | Flight-controller and ESC stack | [AliExpress](https://www.aliexpress.us/item/3256808946065124.html) | $49.63 |
| 3 | Brushless motor, 2306 2450 KV | [AliExpress](https://www.aliexpress.us/item/3256812458735650.html) | $46.02 used (3 of 4-pack at $61.36) |
| 1 | Brushless motor, 2306 2500 KV | [AliExpress](https://www.aliexpress.us/item/3256804750595457.html) | $12.87 |
| 1 set | Propellers | [AliExpress](https://www.aliexpress.us/item/2255801043927518.html) | $15.25 (12-pair pack; includes spares) |
| 2 | Flight battery | [AliExpress](https://www.aliexpress.us/item/3256812540292605.html) | $30.34 used (2 at $15.17 each) |
| 1 | Parallel battery harness | [AliExpress](https://www.aliexpress.us/item/3256807605726245.html) | $1.77 |
| 1 | Drone ESP32 board | [AliExpress](https://www.aliexpress.us/item/3256808984038785.html) | $4.84 |
| 4 | Claw servo | [AliExpress](https://www.aliexpress.us/item/3256809795714797.html) | $3.63 used (4 of 5-pack at $4.54) |
| 1 | Servo regulator | [AliExpress](https://www.aliexpress.us/item/3256806558389509.html) | $1.46 |
| 1 | Servo-rail capacitor | [AliExpress](https://www.aliexpress.us/item/3256806095708994.html) | $0.05 used (1 of 20-pack at $0.99) |
| 1 | Camera module | [AliExpress](https://www.aliexpress.us/item/3256810060131517.html) | $25.90 |
| 1 set | Claw parts and mounts | [PLA filament](https://www.aliexpress.us/item/3256806989098121.html) | $2.54 estimated (250 g of $10.14/kg PLA) |
| As needed | Power and signal wiring | [AliExpress](https://www.aliexpress.us/item/3256809610137308.html) | $5.49 allowance |
| As needed | Connectors and hardware | [AliExpress](https://www.aliexpress.us/item/3256804089626824.html) | $3.81 assortment kit |

### Handheld controller

| Quantity | Part | Link | Price |
| -------: | ---- | ---- | ----: |
| 1 | Black Pill Pico-compatible board | [AliExpress](https://www.aliexpress.us/item/3256805910466139.html) | $4.35 |
| 1 | Controller ESP32 board | [AliExpress](https://www.aliexpress.us/item/3256808984038785.html) | $4.84 |
| 1 | TFT/touch module | [AliExpress](https://www.aliexpress.us/item/2255800128937536.html) | $10.87 |
| 2 | Joystick module | [AliExpress](https://www.aliexpress.us/item/3256809884616435.html) | $1.15 used (2 of 10-pack at $5.76) |
| 10 | Momentary pushbutton | [AliExpress](https://www.aliexpress.us/item/3256801710165519.html) | $3.32 (10-pack) |
| 1 | Controller battery | [AliExpress](https://www.aliexpress.us/item/3256809090690409.html) | $4.24 |
| 1 | Boost converter | [AliExpress](https://www.aliexpress.us/item/3256810344563800.html) | $0.99 |
| 1 | Battery charger and protection board | [AliExpress](https://www.aliexpress.us/item/3256808777213556.html) | $0.20 used (1 of 5-pack at $0.99) |
| 1 | Main power switch | [AliExpress](https://www.aliexpress.us/item/3256807619399290.html) | $0.16 used (1 of 10-pack at $1.64) |
| 1 | Bulk capacitor | [AliExpress](https://www.aliexpress.us/item/2251832671851361.html) | $0.10 used (1 of 10-pack at $0.99) |
| 1 | Decoupling capacitor | [AliExpress](https://www.aliexpress.us/item/3256811560978404.html) | $0.01 used (1 of 100-pack at $0.99) |
| 1 | Controller enclosure | [PLA filament](https://www.aliexpress.us/item/3256806989098121.html) | $3.04 estimated (300 g of $10.14/kg PLA) |
| As needed | Wiring and hardware | [AliExpress](https://www.aliexpress.us/item/3256809610137308.html) | $5.49 allowance |

The controller uses a Black Pill Pico-compatible board that exposes GP29/ADC3. The right joystick X axis connects directly to GP29/ADC3 exactly as defined in the firmware.

## Assembly process

Build the physical aircraft and handheld controller through the process below. Follow [Building Your First 5-Inch FPV Drone: A Complete Step-by-Step Guide](https://blog.uavmodel.com/building-your-first-5-inch-fpv-drone-a-complete-step-by-step-guide/) for the standard frame, motor, flight-stack, soldering, Betaflight, and preflight stages.

Complete all soldering, configuration, continuity checks, and powered bench tests with the propellers removed. Use a smoke stopper for the first power-up.

### Aircraft build

1. Five-inch frame and propulsion
Install a standard 5-inch FPV racer frame with the F405 flight controller, 45 A ESC, four motors, ESP32 receiver, XIAO ESP32 camera, XL4005 regulator, claw servos, batteries, and printed servo holders.
Construct the arms, bottom plates, standoffs, and top plate. Mount the FC/ESC stack on its vibration dampers with the flight controller arrow directed forward.
Install three 2306 2450 KV motors and one 2306 2500 KV motor. Betaflight compensates for the tiny difference with closed-loop control.
Use motor screws that only reach the motor bases but not the windings. Route the three motor wires along their respective arms, leaving strain relief and soldering them to the ESC pads.

2. Parallel battery connection
Use two matching 4S batteries with identical chemistry, capacity, and voltage in a parallel combination.
Solder two XT60 pigtails to the ESC battery pads with heavy-gauge wire that reaches the two battery positions.
Both positive wires connected to the ESC BAT+ pad, and both negatives to BAT-. Each joint is insulated and strain-relieved to prevent the battery movement from straining the ESC pads.
Solder the input capacitor of the flight stack between the ESC battery pads with their marked polarity. Finish the continuity check prior to battery attachment.

3. Standard FPV connections
Plug the ESC-to-flight controller cable in its keyed position.
Plug the XIAO ESP32 camera into the F405 5 V BEC and common ground.
The custom drone ESP32 acts as an ESP-NOW receiver and CRSF bridge instead of a conventional radio receiver.
Perform the power, receiver, motor direction, failsafe, and servos tests before attaching the propellers.

4. Four-servos claw
3D print four SG90 mount holders and install one beneath each motor holder.
Install 10 mm M3 screws through each printed holder and frame arm into the motor base. The screw installation stops short of the motor windings.
Secure each continuous rotation SG90 to its printed holder with the provided self-tapping screws.
Route the servo cables along the arms to the inside. Secure them outside the motor and propeller area with zip ties.
Connect all four servo positive wires to XL4005 OUT+, and all four grounds to OUT-. Connect the PWM control lines as servo 1 – GPIO16, servo 2 – GPIO17, servo 3 – GPIO5, and servo 4 – GPIO18.

5. XL4005 claw power supply
Connect XL4005 IN+ directly to the ESC BAT+ pad and IN- to BAT-.
Set the voltage on the regulator to 4.5 V using a smoke stopper before connecting the servos.
Connect XL4005 OUT-, the F405, drone ESP32, and all four servos to common ground. Servo current goes directly from the XL4005 bypassing the ESP32 and FC.

6. Drone ESP32 receiver
Provide the drone ESP32 5V/VIN input from the F405 5 V BEC. Connect its ground to the F405 ground and XL4005 OUT-.
Use the CRSF UART interface with ESP32 GPIO12 TX to F405 RX3 and ESP32 GPIO14 RX to F405 TX3.
Install the ESP32 board on top of the frame with its long edge horizontally and partially projecting.
Zip ties hold the board over an insulating pad allowing some movement upon impact rather than installing it rigidly to the carbon frame.
Mount the U.FL antenna and route it through the antenna hole, away from the carbon frame, heavy-current wiring, and the propeller area.

7. XIAO ESP32 camera
Supply 5 V input of the XIAO ESP32 camera with the F405 5 V BEC. Ground the board to the F405 ground.
Install the camera on the front with an unobstructed view and secure its board and cable outside the propeller area.

8. Aircraft commissioning
Inspect the solder joints, wire routing, connector polarity, mounting screws, motor clearances, and propeller clearances.
Check the continuity and voltages on the regulator with the multimeter.
Connect the first battery through a smoke stopper. The F405, ESC, both ESP32 boards, camera, and XL4005 power up normally.
With the propellers detached, Betaflight verifies the FC orientation, receiver channels, arm and failsafe operation, motor order, and motor direction.
Verify the four claw servos and the correct rotation direction on opposite pairs in the firmware.
Install balanced 5-inch propellers after the bench test and make the first hover and radio range test outside.

### Handheld controller build

1. Enclosure preparation
Print the enclosure and its top lid and clean button openings, joystick openings, TFT opening, USB access, antenna opening, and M3 mounting holes.
Position the 450 mAh cell, TP4056, 5V boost converter, Black Pill Pico-compatible board, controller ESP32, two joysticks, buttons, TFT, switch, and antenna cable inside the enclosure.
Wire lengths provided sufficient clearance to open the lid without tensioning the display and antenna cable.

2. Controller power supply
Secure the 450 mAh salvage vape cell in its cell compartment with hot glue away from cell terminals.
Mount the TP4056 charger and protection board with its USB charger connector accessible through the enclosure.
Connect the battery to TP4056 B+ and B-. Then, connect TP4056 OUT+ to the main toggle switch, while OUT- connects to controller ground.
Provide the switched positive line and TP4056 ground as the input to the 5V boost converter.
Adjust the converter to 5.0V using the multimeter. Its output is powering the Black Pill board, controller ESP32, and TFT.
Adhesive on the flat surface of both PCBs securely fixed them in the enclosure without blocking their connectors and solder pads.

3. Controller boards and antenna
Position the Black Pill board right behind the USB opening of the enclosure, so that the connector is accessible even when the case is closed.
Apply a small amount of superglue to the flat underside of the board to hold it in place without covering the USB connector and solder pads.
Mount the controller ESP32 right to the left of the Black Pill board using adhesive on its flat side.
Both boards connected to the regulated 5V output and common controller ground.
The UART connection runs from Black Pill GP12 TX to ESP32 GPIO16 RX and from Black Pill GP13 RX to ESP32 GPIO17 TX.
Install the U.FL antenna, route its cable through the antenna opening, and insert the Plex antenna connector into its external position.

4. Buttons
Insert the pushbutton into its enclosure hole and secure it with superglue.
One terminal from each pushbutton forms the daisy-chained common ground bus.
Other terminals are connected to GP0 through GP5 for arm, disarm, and trim; GP8 and GP9 for throttle adjustment; and GP10 and GP11 for claw direction.
Left joystick pushbutton is connected to GP14 for sensitivity down, while the right one connects to GP15 for sensitivity up.

5. Joysticks
Secure each joystick in its four mounting holes in the enclosure with 10 mm M3 screws and nuts.
Both joysticks' VCC pins connected to the Black Pill 3.3V output, while grounds are connected to common ground.
Analog axes are connected as left X to GP26/ADC0, left Y to GP27/ADC1, right Y to GP28/ADC2, and right X to the exposed GP29/ADC3 pin.
Joystick push switches are connected to GP14 and GP15.

6. TFT display
Solder the controller ESP32 to the TFT as CS GPIO15, DC GPIO2, RST GPIO4, SCLK GPIO18, MOSI GPIO23, MISO GPIO19, touch CS GPIO21, and touch IRQ GPIO22.
TFT power connected to the regulated 5V rail, while the TFT ground connects to the common controller ground.
Insert the TFT in the front opening, secure it with M3 x 25 mm screws, and route its wires around the joystick mechanism.

7. Controller assembly
Check the wiring, common ground bus, and 5.0V boost converter output before the first startup.
First startup verified Black Pill link LED, TFT dashboard, joysticks, buttons, UART bridge, and ESP-NOW communication.
Test arm and throttle control when the aircraft propellers are removed.
Route the rest of the wires in the clearance from joystick mechanisms and enclosure edges.
Four M3 x 25 mm screws secured the top lid and finished the controller assembly.

## Controller pinout

### Black Pill Picoboard

Note that I am using a black pill pico with 4 ADC inputs instead of 3 like the normal pi pico.

| Pico pin    | Connection             |
| ----------- | ---------------------- |
| GP0         | Button to GND          |
| GP1         | Button to GND          |
| GP2         | Button to GND          |
| GP3         | Button to GND          |
| GP4         | Button to GND          |
| GP5         | Button to GND          |
| GP6         | Not used               |
| GP7         | Not used               |
| GP8         | Button to GND          |
| GP9         | Button to GND          |
| GP10        | Button to GND          |
| GP11        | Button to GND          |
| GP12        | ESP32 GPIO16           |
| GP13        | ESP32 GPIO17           |
| GP14        | Left stick button      |
| GP15        | Right stick button     |
| GP25        | Onboard LED            |
| GP26 / ADC0 | Left joystick X        |
| GP27 / ADC1 | Left joystick Y        |
| GP28 / ADC2 | Right joystick Y       |
| GP29 / ADC3 | Right joystick X       |
| 3V3         | Joystick VCC           |
| GND         | All controller grounds |


### Pico to controller ESP32 UART

| Black Pill Pico-compatible board | Controller ESP32-WROOM |
| ----------------- | ---------------------- |
| GP12 UART0 TX     | GPIO16 UART2 RX        |
| GP13 UART0 RX     | GPIO17 UART2 TX        |
| GND               | GND                    |


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


## Drone Frame 

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


## Four-leg claw system

Each leg has a cup-shaped end which holds the payload.
All four legs make a compact spider-leg mechanism:

1. Legs spread from the center creating area around the payload.
2. All four legs move out from under the motors.
3. Cup-shaped ends of legs converge from four sides to the payload.
4. Four contact points grip the payload under the drone.


## Battery and power system

I used a parallel 4s setup but it really isn't needed and you can use one for more flight time by swapping them.

| Battery configuration     | System value                                    |
| ------------------------- | ----------------------------------------------- |
| Cell count                | 4S                                              |
| Nominal bus voltage       | 14.8 V                                          |
| Fully charged bus voltage | 16.8 V                                          |
| Standard packs            | 3000 mAh                         |
| Alternate packs           | 3100 mAh                         |
| Connection                | Parallel |
| Main connector            | XT60                                            |



## Drone pinout

### Drone ESP32 to Betaflight flight controller

| Drone ESP32-WROOM | Flight controller |
| ----------------- | ----------------- |
| GPIO12 UART2 TX   | RX3               |
| GPIO14 UART2 RX   | TX3               |
| GND               | GND               |

UART setup for CRSF protocol.

### Drone ESP32 servo outputs

| Servo   | PWM signal pin |
| ------- | -------------- |
| Servo 1 | ESP32 GPIO16   |
| Servo 2 | ESP32 GPIO17   |
| Servo 3 | ESP32 GPIO5    |
| Servo 4 | ESP32 GPIO18   |

The FC did not have enough motor outputs to drive servos so I used the onboard ESP32.

### Servo power

Servos are powered by the XL4005 buck converter at 4.5V

## ESP-NOW radio configuration


Every ESP32 has different values for this: 

| Device           | Wi-Fi STA MAC       |
| ---------------- | ------------------- |
| Controller ESP32 | `68:09:47:5c:04:c4` |
| Drone ESP32      | `68:09:47:5c:2f:8c` |

Update both hard-coded peer addresses as per your devices.

* `drone_mac` in
  `Code/controller/esp32_tx/main/main.c`
* `controller_mac` in
  `Code/drone/esp32_rx/main/main.c`


## Betaflight setup


Give CRSF on UART3 with this configuration:

```text
serial 1 0 115200 57600 0 115200
serial 2 64 115200 57600 0 115200
set serialrx_provider = CRSF
set serialrx_inverted = OFF
set serialrx_halfduplex = OFF
save
```



| CRSF channel | Function               |
| ------------ | ---------------------- |
| CH1          | Roll                   |
| CH2          | Pitch                  |
| CH3          | Throttle               |
| CH4          | Yaw                    |
| CH5 / AUX1   | Arm                    |
| CH6 / AUX2   | Link-active indication |

ARM mode uses AUX1.
