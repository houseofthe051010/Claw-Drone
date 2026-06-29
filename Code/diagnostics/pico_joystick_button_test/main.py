from machine import ADC, Pin
import time

# Raspberry Pi Pico joystick/button diagnostic for Thonny.
# Run this directly in Thonny. It does not use the nRF24L01.

JOY1_X_PIN = 26  # left stick X
JOY1_Y_PIN = 27  # left stick Y
JOY2_X_PIN = 29  # right stick X, swapped for rotated mount
JOY2_Y_PIN = 28  # right stick Y, swapped for rotated mount

LEFT_BUTTON_PIN = 14   # active low, disarm button
RIGHT_BUTTON_PIN = 15  # active low, arm button

ADC_CENTER = 2048
DEADBAND_LOW = 1968
DEADBAND_HIGH = 2128
DIRECTION_THRESHOLD = 100

joy1_x = ADC(Pin(JOY1_X_PIN))
joy1_y = ADC(Pin(JOY1_Y_PIN))
joy2_x = ADC(Pin(JOY2_X_PIN))
joy2_y = ADC(Pin(JOY2_Y_PIN))

left_button = Pin(LEFT_BUTTON_PIN, Pin.IN, Pin.PULL_UP)
right_button = Pin(RIGHT_BUTTON_PIN, Pin.IN, Pin.PULL_UP)


def adc12(adc):
    return adc.read_u16() >> 4


def normalized(raw):
    if DEADBAND_LOW <= raw <= DEADBAND_HIGH:
        return 0
    return raw - ADC_CENTER


def x_direction(norm):
    if norm > DIRECTION_THRESHOLD:
        return "LEFT"
    if norm < -DIRECTION_THRESHOLD:
        return "RIGHT"
    return "CENTER"


def y_direction(norm):
    if norm < -DIRECTION_THRESHOLD:
        return "UP"
    if norm > DIRECTION_THRESHOLD:
        return "DOWN"
    return "CENTER"


def scale_axis(norm):
    if norm == 0:
        return 0
    if norm < 0:
        return max(-1000, (norm * 1000) // 2048)
    return min(1000, (norm * 1000) // 2047)


def read_axis(name, adc, is_x):
    raw = adc12(adc)
    norm = normalized(raw)
    direction = x_direction(norm) if is_x else y_direction(norm)
    scaled = scale_axis(norm)
    return "{} raw={:4d} norm={:5d} scaled={:5d} dir={:6s}".format(
        name, raw, norm, scaled, direction
    )


print()
print("Pico joystick/button diagnostic")
print("ADC range: 0..4095, center around 2048, deadband {}..{}".format(
    DEADBAND_LOW, DEADBAND_HIGH
))
print("Left button GPIO{}: PRESSED when value=0".format(LEFT_BUTTON_PIN))
print("Right button GPIO{}: PRESSED when value=0".format(RIGHT_BUTTON_PIN))
print("Press Ctrl+C in Thonny to stop.")
print()

last_left = left_button.value()
last_right = right_button.value()

while True:
    left = left_button.value()
    right = right_button.value()

    if left != last_left:
        print("LEFT BUTTON:", "PRESSED / DISARM" if left == 0 else "released")
        last_left = left

    if right != last_right:
        print("RIGHT BUTTON:", "PRESSED / ARM" if right == 0 else "released")
        last_right = right

    print(
        read_axis("J1_X yaw     ", joy1_x, True),
        "|",
        read_axis("J1_Y throttle", joy1_y, False),
    )
    print(
        read_axis("J2_X roll    ", joy2_x, True),
        "|",
        read_axis("J2_Y pitch   ", joy2_y, False),
    )
    print(
        "buttons: left_disarm={} right_arm={}".format(
            "PRESSED" if left == 0 else "idle",
            "PRESSED" if right == 0 else "idle",
        )
    )
    print("-" * 96)
    time.sleep_ms(250)
