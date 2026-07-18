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

The controller ESP32 manages the 320x240 TFT dashboard and hosts the Wi-Fi
servo-control web page. The drone ESP32 sends CRSF battery telemetry to the
controller via the same ESP-NOW connection.

## Wiring schematics

The editable KiCad projects and electrical-rule-check reports are available in the [`Schematics`](Schematics) folder. The diagrams use matching net labels to show connections without long crossing wires.

### Handheld controller schematic

![Handheld controller wiring schematic](Pictures/Controller_Schematic.png)


### Aircraft schematic

![Aircraft wiring schematic](Pictures/Claw_Drone_Schematic.png)

The aircraft schematic covers the two parallel 4S packs, F405/45 A flight stack, four motors, F405 BEC-powered ESP32, CRSF UART, XL4005 claw-servo rail, four continuous-rotation SG90 servos, and FPV camera system. Open the editable [`drone.kicad_sch`](Schematics/Drone/drone.kicad_sch) file in KiCad.

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

This BOM documents the components used in the current build based on the checked-in firmware, CAD, wiring, and completed prototype.

### Aircraft

| Quantity | Part | Specification or purpose |
| -------: | ---- | ------------------------ |
| 1 | FPV quadcopter frame | Standard 5-inch X-frame |
| 1 | Flight-controller/ESC stack | Aero Selfie F405 flight controller with 45 A 4-in-1 ESC |
| 3 | Brushless motors | 2306, 2450 KV |
| 1 | Brushless motor | 2306, 2500 KV |
| 1 set | Propellers | Two clockwise and two counterclockwise 5-inch propellers, plus spares |
| 2 | Flight batteries | Matched 4S 1500 mAh packs, or matched 4S 1550 mAh packs |
| 1 | Parallel battery harness | Current-rated 4S XT60 parallel harness |
| 1 | Drone ESP32 board | ESP32-WROOM development board used for ESP-NOW, CRSF, telemetry, and claw PWM |
| 4 | Claw servos | Continuous-rotation SG90 or equivalent |
| 1 | Servo regulator | XL4005 buck converter adjusted to 4.5 V |
| 1 | Servo-rail capacitor | Approximately 2,200 uF, rated for at least 10 V |
| 1 | XIAO ESP32 camera module | Provides the aircraft video feed and is powered from the F405 5 V BEC |
| 1 set | Claw parts and mounts | Printed claw legs, servo mounts, cups, and frame attachments from the CAD |
| As needed | Power and signal wiring | High-current battery wire, servo wire, UART wire, heat-shrink, solder, and zip ties |
| As needed | Connectors and hardware | XT60 connectors, headers, M2/M3 fasteners, standoffs, and vibration mounting hardware |

### Handheld controller

| Quantity | Part | Specification or purpose |
| -------: | ---- | ------------------------ |
| 1 | Black Pill Pico-compatible board | Exposes GP29/ADC3 and reads joysticks and buttons before sending control packets over UART |
| 1 | Controller ESP32 board | ESP32-WROOM development board for ESP-NOW, TFT dashboard, telemetry, and web controls |
| 1 | TFT/touch module | 2.8-inch 320x240 ILI9341 display with XPT2046 touch controller |
| 2 | Joystick modules | Two-axis analog joysticks with push switches |
| 10 | Momentary pushbuttons | Arm, disarm, four trim buttons, throttle up/down, and claw clockwise/counterclockwise |
| 1 | Controller battery | Salvaged 450 mAh single-cell lithium-ion vape battery |
| 1 | Boost converter | Regulated 5 V output with sufficient current for Pico, ESP32, and TFT |
| 1 | 1S battery protection and charging board | Protects and charges the 450 mAh controller cell |
| 1 | Main power switch | Rated for the controller input current |
| 1 | Bulk capacitor | Approximately 470 uF, rated for at least 10 V, on the 5 V controller rail |
| 1 | Decoupling capacitor | 100 nF ceramic near the ESP32/display power connection |
| 1 | Controller enclosure | Printed or fabricated handheld transmitter enclosure |
| As needed | Wiring and hardware | 26 AWG signal wire, power wire, headers, solder, heat-shrink, screws, and standoffs |

The controller uses a Black Pill Pico-compatible board that exposes GP29/ADC3. The right joystick X axis connects directly to GP29/ADC3 exactly as defined in the firmware.

## Assembly process

Build the physical aircraft and handheld controller through the process below. Follow [Building Your First 5-Inch FPV Drone: A Complete Step-by-Step Guide](https://blog.uavmodel.com/building-your-first-5-inch-fpv-drone-a-complete-step-by-step-guide/) for the standard frame, motor, flight-stack, soldering, Betaflight, and preflight stages.

Complete all soldering, configuration, continuity checks, and powered bench tests with the propellers removed. Use a smoke stopper for the first power-up.

### Aircraft build

#### 1. Five-inch frame and propulsion

1. Start with a conventional 5-inch FPV racing frame and lay out the F405 flight controller, 45 A ESC, four motors, ESP32 receiver, XIAO ESP32 camera, XL4005 regulator, claw servos, batteries, and printed servo mounts.
2. Assemble the arms, bottom plates, standoffs, and top plate. Mount the FC/ESC stack centrally on its vibration-isolating gummies with the flight-controller arrow facing forward.
3. Mount three 2306 2450 KV motors and one 2306 2500 KV motor. Betaflight handles the small KV difference through closed-loop control.
4. Use motor screws that engage the motor bases without reaching the windings. Route the three wires from each motor along its arm, leave strain-relief slack, and solder them to the corresponding ESC pads.

#### 2. Parallel battery connection

1. Use two matched 4S batteries with the same chemistry, capacity, and voltage in a parallel configuration.
2. Solder two XT60 pigtails to the ESC battery pads with high-current wire cut to reach the two battery positions.
3. Both positive wires joined the ESC `BAT+` pad, and both negative wires joined `BAT-`. Every joint was insulated and strain-relieved so battery movement did not load the ESC pads.
4. Solder the flight-stack input capacitor across the ESC battery pads with the marked polarity. Complete a continuity test before the first battery connection.

#### 3. Standard FPV wiring

1. Connect the ESC-to-flight-controller harness in its keyed orientation.
2. Connect the XIAO ESP32 camera to the F405 5 V BEC and common ground.
3. The custom drone ESP32 replaced a conventional radio receiver and served as the ESP-NOW receiver and CRSF bridge.
4. Complete the power, receiver, motor-direction, failsafe, and servo tests before fitting the propellers.

#### 4. Four-servo claw

1. 3D print four SG90 mounts and place one beneath each motor mount.
2. Use 10 mm M3 screws through each printed mount and frame arm into the motor base. Their installed depth stops below the motor windings.
3. Fasten each continuous-rotation SG90 to its printed mount with the supplied self-tapping screws.
4. Route the servo cables inward along the arms and secure them with zip ties outside the motor and propeller paths, leaving slack for frame flex.
5. All four servo positive wires joined XL4005 `OUT+`, and all four grounds joined `OUT-`. The PWM signals connected as servo 1 to GPIO16, servo 2 to GPIO17, servo 3 to GPIO5, and servo 4 to GPIO18.

#### 5. XL4005 claw power supply

1. Connect XL4005 `IN+` directly to the ESC `BAT+` pad and `IN-` to `BAT-`.
2. Set the regulator output to 4.5 V through a smoke stopper before attaching the servos.
3. XL4005 `OUT-`, the F405, drone ESP32, and all four servos shared a common ground. Servo current flowed directly from the XL4005 rather than through the ESP32 or flight controller.

#### 6. Drone ESP32 receiver

1. Power the drone ESP32 `5V`/`VIN` pin from the F405 5 V BEC. Join its ground to F405 ground and XL4005 `OUT-`.
2. The CRSF UART used ESP32 GPIO12 TX to F405 RX3 and ESP32 GPIO14 RX to F405 TX3.
3. Mount the ESP32 across the front of the frame with its long edge running sideways and protruding slightly.
4. Zip ties held the board over an insulating pad, giving it limited movement during impacts instead of fixing it rigidly to the carbon frame.
5. Connect the U.FL antenna and route it through the antenna opening, away from the carbon frame, high-current wiring, and propeller area.

#### 7. XIAO ESP32 camera

1. Connect the XIAO ESP32 camera's 5 V input to the F405 5 V BEC and join its ground to F405 ground.
2. Mount the camera at the front with an unobstructed view and secure its board and cable outside the propeller area.

#### 8. Aircraft commissioning

1. Inspect the solder joints, wire routes, connector polarity, mounting screws, motor clearance, and propeller clearance.
2. A multimeter confirmed continuity and the regulator voltages.
3. The first battery connection ran through a smoke stopper. The F405, ESC, both ESP32 boards, camera, and XL4005 powered normally.
4. With the propellers removed, Betaflight confirmed the FC orientation, receiver channels, arm and failsafe behavior, motor order, and motor direction.
5. Test the four claw servos and confirm the firmware's opposite-pair rotation.
6. Fit balanced 5-inch propellers after the bench tests, then complete the first hover and radio range test outdoors.

### Handheld controller build

#### 1. Enclosure preparation

1. Print the controller enclosure and top lid, then clean the button openings, joystick holes, TFT opening, USB access, antenna opening, and M3 mounting holes.
2. Arrange the 450 mAh cell, TP4056, 5 V boost converter, Black Pill Pico-compatible board, controller ESP32, two joysticks, buttons, TFT, switch, and antenna cable inside the enclosure.
3. The wire lengths left enough slack to open the lid without loading the display or antenna cable.

#### 2. Controller power system

1. Fix the 450 mAh salvaged vape cell in its compartment with hot glue applied away from the cell terminals.
2. Mount the TP4056 charging and protection board with its USB charging connector accessible from the enclosure.
3. The battery connected to TP4056 `B+` and `B-`. TP4056 `OUT+` passed through the main toggle switch, while `OUT-` joined controller ground.
4. The switched positive line and TP4056 ground fed the 5 V boost converter.
5. Set the converter to 5.0 V with a multimeter. Its output powers the Black Pill board, controller ESP32, and TFT.
6. Adhesive on the insulated flat sides held the TP4056 and boost converter in place without covering their connectors or solder pads.

#### 3. Controller boards and antenna

1. Place the Black Pill board directly behind the enclosure's USB opening, leaving its connector accessible with the case closed.
2. A thin layer of superglue on the flat underside held the board without covering the USB connector or solder pads.
3. Mount the controller ESP32 immediately to the left of the Black Pill board with adhesive on its flat side.
4. Both boards connected to the regulated 5 V output and common controller ground.
5. The UART connection ran from Black Pill GP12 TX to ESP32 GPIO16 RX and from Black Pill GP13 RX to ESP32 GPIO17 TX.
6. Attach the U.FL antenna, route its cable through the antenna opening, and seat the Plex antenna connector in its external position.

#### 4. Buttons

1. Insert each pushbutton into its enclosure opening and fix it with superglue.
2. One terminal from every pushbutton formed a daisy-chained common-ground bus.
3. The remaining terminals connected to GP0 through GP5 for arm, disarm, and trim; GP8 and GP9 for throttle adjustment; and GP10 and GP11 for claw direction.
4. The left joystick pushbutton connected to GP14 for sensitivity down, and the right joystick pushbutton connected to GP15 for sensitivity up.

#### 5. Joysticks

1. Mount each joystick through its four enclosure holes with 10 mm M3 screws and nuts.
2. Both joystick VCC pins connected to the Black Pill's 3.3 V output, and both grounds joined common ground.
3. The analog axes connected as left X to GP26/ADC0, left Y to GP27/ADC1, right Y to GP28/ADC2, and right X to the exposed GP29/ADC3 pin.
4. The two joystick push switches connected to GP14 and GP15.

#### 6. TFT display

1. Solder the controller ESP32 to the TFT as CS GPIO15, DC GPIO2, RST GPIO4, SCLK GPIO18, MOSI GPIO23, MISO GPIO19, touch CS GPIO21, and touch IRQ GPIO22.
2. TFT power connected to the regulated 5 V rail, and TFT ground joined common controller ground.
3. Place the TFT in the front opening, secure it with M3 x 25 mm screws, and route its wiring around the joystick mechanisms.

#### 7. Controller completion

1. Inspect the wiring, common-ground bus, and 5.0 V boost-converter output before the first startup.
2. The first startup confirmed the Black Pill link LED, TFT dashboard, joysticks, buttons, UART bridge, and ESP-NOW connection.
3. Test arm and throttle control while the aircraft propellers are removed.
4. Arrange the remaining wires clear of the joystick mechanisms and enclosure edges.
5. Four M3 x 25 mm screws secured the top lid and completed the controller.

## Controller pinout

### Black Pill Pico-compatible board

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

| Black Pill Pico-compatible board | Controller ESP32-WROOM |
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
