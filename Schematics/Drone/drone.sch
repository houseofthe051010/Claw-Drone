EESchema Schematic File Version 4
LIBS:power
LIBS:device
LIBS:Connector_Generic
LIBS:Switch
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Claw Drone Aircraft"
Date "2026-07-12"
Rev "1.0"
Comp "Aditya Verma"
Comment1 "Pin assignments verified against drone firmware and repository README"
Comment2 "Aero Selfie F405 + 45A ESC stack, ESP32 CRSF bridge, four claw servos"
Comment3 "Two matched 4S packs in parallel; XL4005 servo rail set to 4.5V"
Comment4 "F405 onboard 5V BEC powers ESP32 VIN"
$EndDescr
Text Notes 600 550 0 120 ~ 24
CLAW DRONE AIRCRAFT - WIRING SCHEMATIC
Text Notes 600 760 0 55 ~ 11
REMOVE PROPELLERS DURING BENCH TESTS. Matching net labels are electrically connected. ESP-NOW link to controller is wireless.
Text Notes 600 1050 0 75 ~ 15
4S POWER AND FLIGHT STACK
$Comp
L Connector_Generic:Conn_01x02 J1
U 1 1 30000001
P 1500 1500
F 0 "J1" H 1418 1717 50 0000 C CNN
F 1 "4S_LIPO_PACK_A_XT60" H 1418 1626 50 0000 C CNN
	1    1500 1500
	-1 0 0 -1
$EndComp
Text GLabel 1700 1500 2 45 Output ~ 0
VBAT_4S_PARALLEL
Text GLabel 1700 1600 2 45 Output ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 J2
U 1 1 30000002
P 1500 2050
F 0 "J2" H 1418 2267 50 0000 C CNN
F 1 "4S_LIPO_PACK_B_XT60" H 1418 2176 50 0000 C CNN
	1    1500 2050
	-1 0 0 -1
$EndComp
Text GLabel 1700 2050 2 45 Output ~ 0
VBAT_4S_PARALLEL
Text GLabel 1700 2150 2 45 Output ~ 0
GND
Text Notes 800 2450 0 45 ~ 9
Use only matched 4S packs at equal voltage with a current-rated parallel XT60 harness.
$Comp
L Connector_Generic:Conn_01x10 J3
U 1 1 30000003
P 3550 1850
F 0 "J3" H 3630 1842 50 0000 L CNN
F 1 "AERO_SELFIE_F405_FLIGHT_CONTROLLER" H 3630 1751 50 0000 L CNN
	1    3550 1850
	1 0 0 -1
$EndComp
Text GLabel 3350 1450 0 45 Input ~ 0
VBAT_4S_PARALLEL
Text GLabel 3350 1550 0 45 Input ~ 0
GND
Text GLabel 3350 1650 0 45 Output ~ 0
FC_5V_BEC
Text GLabel 3350 1750 0 45 Input ~ 0
FC_RX3_FROM_ESP_TX12
Text GLabel 3350 1850 0 45 Output ~ 0
FC_TX3_TO_ESP_RX14
Text GLabel 3350 1950 0 45 Output ~ 0
MOTOR1_SIGNAL
Text GLabel 3350 2050 0 45 Output ~ 0
MOTOR2_SIGNAL
Text GLabel 3350 2150 0 45 Output ~ 0
MOTOR3_SIGNAL
Text GLabel 3350 2250 0 45 Output ~ 0
MOTOR4_SIGNAL
Text GLabel 3350 2350 0 45 BiDi ~ 0
FPV_VIDEO
$Comp
L Connector_Generic:Conn_01x06 J4
U 1 1 30000004
P 5600 1850
F 0 "J4" H 5680 1842 50 0000 L CNN
F 1 "45A_4IN1_ESC" H 5680 1751 50 0000 L CNN
	1    5600 1850
	1 0 0 -1
$EndComp
Text GLabel 5400 1650 0 45 Input ~ 0
VBAT_4S_PARALLEL
Text GLabel 5400 1750 0 45 Input ~ 0
GND
Text GLabel 5400 1850 0 45 Input ~ 0
MOTOR1_SIGNAL
Text GLabel 5400 1950 0 45 Input ~ 0
MOTOR2_SIGNAL
Text GLabel 5400 2050 0 45 Input ~ 0
MOTOR3_SIGNAL
Text GLabel 5400 2150 0 45 Input ~ 0
MOTOR4_SIGNAL
$Comp
L Connector_Generic:Conn_01x03 J5
U 1 1 30000005
P 7350 1450
F 0 "J5" H 7430 1492 50 0000 L CNN
F 1 "MOTOR1_2306_2450KV" H 7430 1401 50 0000 L CNN
	1    7350 1450
	1 0 0 -1
$EndComp
Text GLabel 7150 1350 0 42 Output ~ 0
ESC_M1_U
Text GLabel 7150 1450 0 42 Output ~ 0
ESC_M1_V
Text GLabel 7150 1550 0 42 Output ~ 0
ESC_M1_W
$Comp
L Connector_Generic:Conn_01x03 J6
U 1 1 30000006
P 7350 1950
F 0 "J6" H 7430 1992 50 0000 L CNN
F 1 "MOTOR2_2306_2450KV" H 7430 1901 50 0000 L CNN
	1    7350 1950
	1 0 0 -1
$EndComp
Text GLabel 7150 1850 0 42 Output ~ 0
ESC_M2_U
Text GLabel 7150 1950 0 42 Output ~ 0
ESC_M2_V
Text GLabel 7150 2050 0 42 Output ~ 0
ESC_M2_W
$Comp
L Connector_Generic:Conn_01x03 J7
U 1 1 30000007
P 9300 1450
F 0 "J7" H 9380 1492 50 0000 L CNN
F 1 "MOTOR3_2306_2450KV" H 9380 1401 50 0000 L CNN
	1    9300 1450
	1 0 0 -1
$EndComp
Text GLabel 9100 1350 0 42 Output ~ 0
ESC_M3_U
Text GLabel 9100 1450 0 42 Output ~ 0
ESC_M3_V
Text GLabel 9100 1550 0 42 Output ~ 0
ESC_M3_W
$Comp
L Connector_Generic:Conn_01x03 J8
U 1 1 30000008
P 9300 1950
F 0 "J8" H 9380 1992 50 0000 L CNN
F 1 "MOTOR4_2306_2500KV" H 9380 1901 50 0000 L CNN
	1    9300 1950
	1 0 0 -1
$EndComp
Text GLabel 9100 1850 0 42 Output ~ 0
ESC_M4_U
Text GLabel 9100 1950 0 42 Output ~ 0
ESC_M4_V
Text GLabel 9100 2050 0 42 Output ~ 0
ESC_M4_W
Text Notes 6550 2450 0 45 ~ 9
The 4-in-1 ESC phase-wire order determines motor direction; swap any two motor wires if required.
Text Notes 600 2850 0 75 ~ 15
DRONE ESP32 CRSF BRIDGE AND CLAW OUTPUTS
$Comp
L Connector_Generic:Conn_02x19_Odd_Even J9
U 1 1 30000009
P 2900 4150
F 0 "J9" H 2950 5267 50 0000 C CNN
F 1 "DRONE_ESP32_DEVKIT_V1_38PIN" H 2950 5176 50 0000 C CNN
	1    2900 4150
	1 0 0 -1
$EndComp
Text GLabel 2700 3250 0 45 Output ~ 0
+3V3_ESP32
Text GLabel 3200 3250 2 45 Input ~ 0
GND
Text GLabel 2700 3350 0 45 BiDi ~ 0
ESP_EN
Text GLabel 3200 3350 2 45 BiDi ~ 0
GPIO23_UNUSED
Text GLabel 2700 3450 0 45 Input ~ 0
GPIO36_UNUSED
Text GLabel 3200 3450 2 45 BiDi ~ 0
GPIO22_UNUSED
Text GLabel 2700 3550 0 45 Input ~ 0
GPIO39_UNUSED
Text GLabel 3200 3550 2 45 BiDi ~ 0
UART_TX_GPIO1_UNUSED
Text GLabel 2700 3650 0 45 Input ~ 0
GPIO34_UNUSED
Text GLabel 3200 3650 2 45 BiDi ~ 0
UART_RX_GPIO3_UNUSED
Text GLabel 2700 3750 0 45 Input ~ 0
GPIO35_UNUSED
Text GLabel 3200 3750 2 45 BiDi ~ 0
GPIO21_UNUSED
Text GLabel 2700 3850 0 45 BiDi ~ 0
GPIO32_UNUSED
Text GLabel 3200 3850 2 45 Input ~ 0
GND
Text GLabel 2700 3950 0 45 BiDi ~ 0
GPIO33_UNUSED
Text GLabel 3200 3950 2 45 BiDi ~ 0
GPIO19_UNUSED
Text GLabel 2700 4050 0 45 BiDi ~ 0
GPIO25_UNUSED
Text GLabel 3200 4050 2 45 Output ~ 0
SERVO4_PWM_GPIO18
Text GLabel 2700 4150 0 45 BiDi ~ 0
GPIO26_UNUSED
Text GLabel 3200 4150 2 45 Output ~ 0
SERVO3_PWM_GPIO5
Text GLabel 2700 4250 0 45 BiDi ~ 0
GPIO27_UNUSED
Text GLabel 3200 4250 2 45 Output ~ 0
SERVO2_PWM_GPIO17
Text GLabel 2700 4350 0 45 Output ~ 0
CRSF_RX_GPIO14_FROM_FC_TX3
Text GLabel 3200 4350 2 45 Output ~ 0
SERVO1_PWM_GPIO16
Text GLabel 2700 4450 0 45 Output ~ 0
CRSF_TX_GPIO12_TO_FC_RX3
Text GLabel 3200 4450 2 45 BiDi ~ 0
GPIO4_UNUSED
Text GLabel 2700 4550 0 45 Input ~ 0
GND
Text GLabel 3200 4550 2 45 BiDi ~ 0
GPIO0_UNUSED
Text GLabel 2700 4650 0 45 BiDi ~ 0
GPIO13_UNUSED
Text GLabel 3200 4650 2 45 BiDi ~ 0
GPIO2_UNUSED
Text GLabel 2700 4750 0 45 BiDi ~ 0
FLASH_D2_UNUSED
Text GLabel 3200 4750 2 45 BiDi ~ 0
GPIO15_UNUSED
Text GLabel 2700 4850 0 45 BiDi ~ 0
FLASH_D3_UNUSED
Text GLabel 3200 4850 2 45 BiDi ~ 0
FLASH_D1_UNUSED
Text GLabel 2700 4950 0 45 BiDi ~ 0
FLASH_CMD_UNUSED
Text GLabel 3200 4950 2 45 BiDi ~ 0
FLASH_D0_UNUSED
Text GLabel 2700 5050 0 45 Input ~ 0
FC_5V_BEC
Text GLabel 3200 5050 2 45 BiDi ~ 0
FLASH_CLK_UNUSED
Text Notes 1900 5400 0 45 ~ 9
F405 onboard 5V BEC -> ESP32 VIN/5V. Never connect the 4S battery or 4.5V servo rail to ESP32 GPIO.
Text GLabel 4400 3300 0 45 Output ~ 0
CRSF_TX_GPIO12_TO_FC_RX3
Text GLabel 5550 3300 2 45 Input ~ 0
FC_RX3_FROM_ESP_TX12
Wire Wire Line
	4400 3300 5550 3300
Text GLabel 4400 3500 0 45 Input ~ 0
CRSF_RX_GPIO14_FROM_FC_TX3
Text GLabel 5550 3500 2 45 Output ~ 0
FC_TX3_TO_ESP_RX14
Wire Wire Line
	4400 3500 5550 3500
Text Notes 4400 3700 0 45 ~ 9
CRSF UART2/UART3: 420000 baud, 8-N-1. GPIO12 is an ESP32 boot strap; FC RX3 must remain high-impedance during reset.
$Comp
L Connector_Generic:Conn_01x03 J10
U 1 1 30000010
P 5900 4150
F 0 "J10" H 5980 4192 50 0000 L CNN
F 1 "CLAW_SERVO1_CR_SG90" H 5980 4101 50 0000 L CNN
	1    5900 4150
	1 0 0 -1
$EndComp
Text GLabel 5700 4050 0 42 Input ~ 0
+4V5_SERVO
Text GLabel 5700 4150 0 42 Input ~ 0
GND
Text GLabel 5700 4250 0 42 Input ~ 0
SERVO1_PWM_GPIO16
$Comp
L Connector_Generic:Conn_01x03 J11
U 1 1 30000011
P 5900 4700
F 0 "J11" H 5980 4742 50 0000 L CNN
F 1 "CLAW_SERVO2_CR_SG90" H 5980 4651 50 0000 L CNN
	1    5900 4700
	1 0 0 -1
$EndComp
Text GLabel 5700 4600 0 42 Input ~ 0
+4V5_SERVO
Text GLabel 5700 4700 0 42 Input ~ 0
GND
Text GLabel 5700 4800 0 42 Input ~ 0
SERVO2_PWM_GPIO17
$Comp
L Connector_Generic:Conn_01x03 J12
U 1 1 30000012
P 8200 4150
F 0 "J12" H 8280 4192 50 0000 L CNN
F 1 "CLAW_SERVO3_CR_SG90" H 8280 4101 50 0000 L CNN
	1    8200 4150
	1 0 0 -1
$EndComp
Text GLabel 8000 4050 0 42 Input ~ 0
+4V5_SERVO
Text GLabel 8000 4150 0 42 Input ~ 0
GND
Text GLabel 8000 4250 0 42 Input ~ 0
SERVO3_PWM_GPIO5
$Comp
L Connector_Generic:Conn_01x03 J13
U 1 1 30000013
P 8200 4700
F 0 "J13" H 8280 4742 50 0000 L CNN
F 1 "CLAW_SERVO4_CR_SG90" H 8280 4651 50 0000 L CNN
	1    8200 4700
	1 0 0 -1
$EndComp
Text GLabel 8000 4600 0 42 Input ~ 0
+4V5_SERVO
Text GLabel 8000 4700 0 42 Input ~ 0
GND
Text GLabel 8000 4800 0 42 Input ~ 0
SERVO4_PWM_GPIO18
Text Notes 5600 5100 0 45 ~ 9
Servo PWM: 50Hz, 500-2500us. Servo power comes from XL4005, never from ESP32 or FC 3.3V.
Text Notes 600 5750 0 75 ~ 15
PAYLOAD POWER AND FPV CAMERA
$Comp
L Connector_Generic:Conn_01x04 J14
U 1 1 30000014
P 2900 6200
F 0 "J14" H 2980 6192 50 0000 L CNN
F 1 "XL4005_BUCK_MODULE_SET_4V5" H 2980 6101 50 0000 L CNN
	1    2900 6200
	1 0 0 -1
$EndComp
Text GLabel 2700 6100 0 45 Input ~ 0
VBAT_4S_PARALLEL
Text GLabel 2700 6200 0 45 Input ~ 0
GND
Text GLabel 2700 6300 0 45 Output ~ 0
+4V5_SERVO
Text GLabel 2700 6400 0 45 Output ~ 0
GND
$Comp
L Device:C_Polarized C1
U 1 1 30000015
P 4650 6250
F 0 "C1" H 4768 6296 50 0000 L CNN
F 1 "2200uF_10V" H 4768 6205 50 0000 L CNN
	1    4650 6250
	1 0 0 -1
$EndComp
Text GLabel 4650 6100 1 45 Input ~ 0
+4V5_SERVO
Text GLabel 4650 6400 3 45 Input ~ 0
GND
$Comp
L Device:C C2
U 1 1 30000016
P 5750 6250
F 0 "C2" H 5865 6296 50 0000 L CNN
F 1 "100nF" H 5865 6205 50 0000 L CNN
	1    5750 6250
	1 0 0 -1
$EndComp
Text GLabel 5750 6100 1 45 Input ~ 0
+3V3_ESP32
Text GLabel 5750 6400 3 45 Input ~ 0
GND
$Comp
L Connector_Generic:Conn_01x03 J15
U 1 1 30000017
P 7900 6200
F 0 "J15" H 7980 6242 50 0000 L CNN
F 1 "FPV_CAMERA_SYSTEM" H 7980 6151 50 0000 L CNN
	1    7900 6200
	1 0 0 -1
$EndComp
Text GLabel 7700 6100 0 45 Input ~ 0
FC_5V_BEC
Text GLabel 7700 6200 0 45 Input ~ 0
GND
Text GLabel 7700 6300 0 45 Output ~ 0
FPV_VIDEO
Text Notes 7000 6550 0 45 ~ 9
The FPV camera uses the F405 5V BEC and sends its video signal to the flight controller video input.
Text Notes 600 7050 0 50 ~ 10
GPIO16/17/5/18 -> servos 1-4 | GPIO12 TX -> FC RX3 | GPIO14 RX <- FC TX3 | all grounds common.
Text Notes 600 7220 0 50 ~ 10
FC M1-M4 drive the 45A ESC stack; ESP32 firmware does not drive propulsion motors.
Text Notes 600 7390 0 50 ~ 10
Inspect polarity and regulator outputs with a multimeter before connecting electronics. Bench-test without propellers.
$EndSCHEMATC
