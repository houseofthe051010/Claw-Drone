from machine import ADC, Pin, UART
import select
import struct
import sys
import time

# Raspberry Pi Pico -> ESP32 WROOM UART bridge wiring:
#   Pico GP12 / UART0 TX -> ESP32 GPIO16 / RX2
#   Pico GP13 / UART0 RX <- ESP32 GPIO17 / TX2
#   Pico GND             <-> ESP32 GND
UART_ID = 0
UART_BAUD = 115200
UART_TX_PIN = 12
UART_RX_PIN = 13

# Joystick/buttons from your controller wiring.
JOY_LEFT_X_ADC = 26    # ADC0 yaw
JOY_LEFT_Y_ADC = 27    # ADC1 throttle
JOY_RIGHT_X_ADC = 29   # ADC3 roll, swapped for rotated right stick
JOY_RIGHT_Y_ADC = 28   # ADC2 pitch, swapped for rotated right stick
LINK_LED_PIN = 25

# Active-low buttons wired between GPIO and GND.
BUTTON_PINS = {
    "arm": 0,
    "disarm": 1,
    "pitch_back": 2,
    "roll_right": 3,
    "roll_left": 4,
    "pitch_forward": 5,
    "throttle_down": 8,
    "throttle_up": 9,
    "yaw_right": 10,
    "yaw_left": 11,
    "sensitivity_down": 14,
    "sensitivity_up": 15,
}

ADC_CENTER = 2048
ADC_DEADBAND_LOW = 1968
ADC_DEADBAND_HIGH = 2128
BUTTON_DEBOUNCE_MS = 80
SEND_INTERVAL_MS = 20
STATUS_INTERVAL_MS = 200
SERVO_BUTTON_INTERVAL_MS = 100
LINK_TIMEOUT_MS = 500

TRIM_STEP = 15
TRIM_LIMIT = 250
THROTTLE_BUTTON_STEP = 5
SENSITIVITY_DEFAULT_PERCENT = 50
SENSITIVITY_STEP_PERCENT = 5
SENSITIVITY_MIN_PERCENT = 25
SENSITIVITY_MAX_PERCENT = 100
ROLL_PITCH_COMMAND_LIMIT = 600
YAW_COMMAND_LIMIT = 350

# Spring-centered throttle rate control. Center holds the current throttle;
# full stick changes throttle by 200 units (20 percent) per second.
THROTTLE_DEADBAND = 200
THROTTLE_RATE_PER_SECOND = 200
THROTTLE_RATE_SCALE = 1_000_000

UART_SYNC_0 = 0xA5
UART_SYNC_1 = 0x5A
PAYLOAD_SIZE = 16

RADIO_PACKET_MAGIC = 0x52
RADIO_PACKET_VERSION = 1
CONTROLLER_STATUS_MAGIC = 0x54
CONTROLLER_SERVO_MAGIC = 0x56

CONTROL_FLAG_ARM = 1 << 0
CONTROL_FLAG_ESTOP = 1 << 1
CONTROL_FLAG_CLEAR_ESTOP = 1 << 2

uart = UART(
    UART_ID,
    baudrate=UART_BAUD,
    tx=Pin(UART_TX_PIN),
    rx=Pin(UART_RX_PIN),
    bits=8,
    parity=None,
    stop=1,
    timeout=0,
)

link_led = Pin(LINK_LED_PIN, Pin.OUT, value=0)
buttons = {
    name: Pin(gpio, Pin.IN, Pin.PULL_UP)
    for name, gpio in BUTTON_PINS.items()
}
button_raw_states = {name: button.value() for name, button in buttons.items()}
button_stable_states = dict(button_raw_states)
button_change_ms = {name: time.ticks_ms() for name in buttons}

adc_left_x = ADC(Pin(JOY_LEFT_X_ADC))
adc_left_y = ADC(Pin(JOY_LEFT_Y_ADC))
adc_right_x = ADC(Pin(JOY_RIGHT_X_ADC))
adc_right_y = ADC(Pin(JOY_RIGHT_Y_ADC))

sequence = 0
armed = False
estop = False
override_until = 0
override_control = None
throttle_setpoint = 0
throttle_last_update_ms = time.ticks_ms()
throttle_rate_accumulator = 0
roll_trim = 0
pitch_trim = 0
yaw_trim = 0
control_sensitivity_percent = SENSITIVITY_DEFAULT_PERCENT
sent = 0
last_ack_ms = 0
last_bridge_text = b""
stdin_poll = select.poll()
stdin_poll.register(sys.stdin, select.POLLIN)
usb_line = ""


def ticks_ms():
    return time.ticks_ms()


def ticks_diff(a, b):
    return time.ticks_diff(a, b)


def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def clamp(value, minimum, maximum):
    if value < minimum:
        return minimum
    if value > maximum:
        return maximum
    return value


def adc12(adc):
    total = 0
    for _ in range(4):
        total += adc.read_u16() >> 4
        time.sleep_us(150)
    return total // 4


def centered(raw):
    if ADC_DEADBAND_LOW <= raw <= ADC_DEADBAND_HIGH:
        return 0
    return raw - ADC_CENTER


def scale_axis(raw):
    c = centered(raw)
    if c < 0:
        return clamp((c * 1000) // 2048, -1000, 1000)
    return clamp((c * 1000) // 2047, -1000, 1000)


def scale_for_sensitivity(value, command_limit):
    return (
        int(value) * int(command_limit) * control_sensitivity_percent
    ) // 100000


def pressed_buttons(now):
    pressed = []
    for name, button in buttons.items():
        raw = button.value()
        if raw != button_raw_states[name]:
            button_raw_states[name] = raw
            button_change_ms[name] = now
        if (
            raw != button_stable_states[name]
            and ticks_diff(now, button_change_ms[name]) >= BUTTON_DEBOUNCE_MS
        ):
            button_stable_states[name] = raw
            if raw == 0:
                pressed.append(name)
    return pressed


def apply_button_presses(pressed, now):
    global armed, estop, override_control
    global roll_trim, pitch_trim, yaw_trim, control_sensitivity_percent
    global throttle_setpoint

    if "disarm" in pressed:
        armed = False
        estop = False
        override_control = None
        reset_throttle(now)
        print("button: DISARM")
    elif "arm" in pressed:
        if throttle_rate_command(adc12(adc_left_y)) == 0:
            reset_throttle(now)
            armed = True
            estop = False
            print("button: ARM")
        else:
            armed = False
            reset_throttle(now)
            print("button: ARM REFUSED - center throttle stick first")

    if "roll_right" in pressed:
        roll_trim = clamp(roll_trim + TRIM_STEP, -TRIM_LIMIT, TRIM_LIMIT)
    if "roll_left" in pressed:
        roll_trim = clamp(roll_trim - TRIM_STEP, -TRIM_LIMIT, TRIM_LIMIT)
    if "pitch_forward" in pressed:
        pitch_trim = clamp(pitch_trim - TRIM_STEP, -TRIM_LIMIT, TRIM_LIMIT)
    if "pitch_back" in pressed:
        pitch_trim = clamp(pitch_trim + TRIM_STEP, -TRIM_LIMIT, TRIM_LIMIT)
    if armed and "arm" not in pressed:
        if "throttle_up" in pressed:
            throttle_setpoint = clamp(
                throttle_setpoint + THROTTLE_BUTTON_STEP, 0, 1000
            )
        if "throttle_down" in pressed:
            throttle_setpoint = clamp(
                throttle_setpoint - THROTTLE_BUTTON_STEP, 0, 1000
            )

    if "sensitivity_up" in pressed:
        control_sensitivity_percent = clamp(
            control_sensitivity_percent + SENSITIVITY_STEP_PERCENT,
            SENSITIVITY_MIN_PERCENT,
            SENSITIVITY_MAX_PERCENT,
        )
    if "sensitivity_down" in pressed:
        control_sensitivity_percent = clamp(
            control_sensitivity_percent - SENSITIVITY_STEP_PERCENT,
            SENSITIVITY_MIN_PERCENT,
            SENSITIVITY_MAX_PERCENT,
        )

    if any(name not in ("arm", "disarm") for name in pressed):
        print(
            "adjust: roll={} pitch={} yaw={} throttle={} sensitivity={}%".format(
                roll_trim,
                pitch_trim,
                yaw_trim,
                throttle_setpoint,
                control_sensitivity_percent,
            )
        )


def throttle_rate_command(raw):
    offset = int(raw) - ADC_CENTER
    if -THROTTLE_DEADBAND <= offset <= THROTTLE_DEADBAND:
        return 0
    if offset < 0:
        return clamp(
            ((-offset - THROTTLE_DEADBAND) * 1000)
            // (ADC_CENTER - THROTTLE_DEADBAND),
            0,
            1000,
        )
    return -clamp(
        ((offset - THROTTLE_DEADBAND) * 1000)
        // (2047 - THROTTLE_DEADBAND),
        0,
        1000,
    )


def reset_throttle(now=None):
    global throttle_setpoint, throttle_last_update_ms, throttle_rate_accumulator
    throttle_setpoint = 0
    throttle_rate_accumulator = 0
    throttle_last_update_ms = ticks_ms() if now is None else now


def update_throttle(raw, now):
    global throttle_setpoint, throttle_last_update_ms, throttle_rate_accumulator

    elapsed_ms = clamp(ticks_diff(now, throttle_last_update_ms), 0, 100)
    throttle_last_update_ms = now
    command = throttle_rate_command(raw)
    if command == 0:
        throttle_rate_accumulator = 0
        return throttle_setpoint
    throttle_rate_accumulator += command * THROTTLE_RATE_PER_SECOND * elapsed_ms

    if throttle_rate_accumulator >= THROTTLE_RATE_SCALE:
        change = throttle_rate_accumulator // THROTTLE_RATE_SCALE
    elif throttle_rate_accumulator <= -THROTTLE_RATE_SCALE:
        change = -((-throttle_rate_accumulator) // THROTTLE_RATE_SCALE)
    else:
        change = 0

    if change:
        throttle_rate_accumulator -= change * THROTTLE_RATE_SCALE
        throttle_setpoint = clamp(throttle_setpoint + change, 0, 1000)
        if throttle_setpoint in (0, 1000):
            throttle_rate_accumulator = 0

    return throttle_setpoint


def read_controls():
    now = ticks_ms()
    apply_button_presses(pressed_buttons(now), now)

    if override_control is not None and ticks_diff(override_until, now) > 0:
        return override_control

    if not armed:
        reset_throttle(now)
        return 0, 0, 0, 0, False, False

    # X directions are inverted to match your spec:
    # raw high means LEFT, but RC positive roll/yaw should be RIGHT by convention.
    yaw = scale_for_sensitivity(
        -scale_axis(adc12(adc_left_x)), YAW_COMMAND_LIMIT
    )
    roll = scale_for_sensitivity(
        -scale_axis(adc12(adc_right_x)), ROLL_PITCH_COMMAND_LIMIT
    )
    pitch = scale_for_sensitivity(
        scale_axis(adc12(adc_right_y)), ROLL_PITCH_COMMAND_LIMIT
    )

    roll = clamp(roll + roll_trim, -ROLL_PITCH_COMMAND_LIMIT, ROLL_PITCH_COMMAND_LIMIT)
    pitch = clamp(
        pitch + pitch_trim, -ROLL_PITCH_COMMAND_LIMIT, ROLL_PITCH_COMMAND_LIMIT
    )
    yaw = clamp(yaw + yaw_trim, -YAW_COMMAND_LIMIT, YAW_COMMAND_LIMIT)

    # Spring-centered rate throttle: up increases, center holds, down decreases.
    throttle = update_throttle(adc12(adc_left_y), now)

    return roll, pitch, throttle, yaw, armed, estop


def build_payload(roll, pitch, throttle, yaw, arm, estop_flag):
    global sequence
    sequence = (sequence + 1) & 0xFFFF

    flags = 0
    if estop_flag:
        flags |= CONTROL_FLAG_ESTOP
    elif arm:
        flags |= CONTROL_FLAG_ARM
    else:
        flags |= CONTROL_FLAG_CLEAR_ESTOP

    payload = bytearray(PAYLOAD_SIZE)
    payload[0] = RADIO_PACKET_MAGIC
    payload[1] = flags
    struct.pack_into("<Hhhhh", payload, 2, sequence, roll, pitch, throttle, yaw)
    crc = crc16_ccitt(payload[:12])
    payload[12] = crc & 0xFF
    payload[13] = crc >> 8
    payload[14] = 0
    payload[15] = RADIO_PACKET_VERSION
    return payload


def build_status_payload():
    payload = bytearray(PAYLOAD_SIZE)
    payload[0] = CONTROLLER_STATUS_MAGIC
    payload[1] = RADIO_PACKET_VERSION
    struct.pack_into(
        "<hhhHBB",
        payload,
        2,
        roll_trim,
        pitch_trim,
        yaw_trim,
        throttle_setpoint,
        control_sensitivity_percent,
        1 if armed else 0,
    )
    crc = crc16_ccitt(payload[:12])
    payload[12] = crc & 0xFF
    payload[13] = crc >> 8
    payload[14] = 0
    payload[15] = RADIO_PACKET_VERSION
    return payload


def servo_button_direction():
    clockwise = button_stable_states["yaw_right"] == 0
    counterclockwise = button_stable_states["yaw_left"] == 0
    return int(clockwise) - int(counterclockwise)


def build_servo_button_payload(direction):
    payload = bytearray(PAYLOAD_SIZE)
    payload[0] = CONTROLLER_SERVO_MAGIC
    payload[1] = RADIO_PACKET_VERSION
    struct.pack_into("<b", payload, 2, clamp(int(direction), -1, 1))
    crc = crc16_ccitt(payload[:12])
    payload[12] = crc & 0xFF
    payload[13] = crc >> 8
    payload[15] = RADIO_PACKET_VERSION
    return payload


def send_payload(payload):
    uart.write(bytes((UART_SYNC_0, UART_SYNC_1, PAYLOAD_SIZE)))
    uart.write(payload)


def read_bridge_replies():
    global last_ack_ms, last_bridge_text

    while uart.any():
        chunk = uart.read()
        if not chunk:
            return
        last_bridge_text += chunk
        if len(last_bridge_text) > 80:
            last_bridge_text = last_bridge_text[-80:]
        if b"OK" in last_bridge_text:
            last_ack_ms = ticks_ms()
            last_bridge_text = b""


def update_link_led():
    if ticks_diff(ticks_ms(), last_ack_ms) <= LINK_TIMEOUT_MS:
        link_led.value(1)
    else:
        link_led.value((ticks_ms() // 250) & 1)


def override(roll, pitch, throttle, yaw, arm, estop_flag=False, duration_ms=2000):
    global override_control, override_until, armed, estop
    armed = bool(arm)
    estop = bool(estop_flag)
    override_control = (
        clamp(int(roll), -1000, 1000),
        clamp(int(pitch), -1000, 1000),
        clamp(int(throttle), 0, 1000),
        clamp(int(yaw), -1000, 1000),
        armed,
        estop,
    )
    override_until = time.ticks_add(ticks_ms(), duration_ms)
    print("override:", override_control)


def live():
    global override_control
    override_control = None
    print("live joystick mode")


def neutral():
    override(0, 0, 0, 0, False, False)


def arm():
    override(0, 0, 0, 0, True, False)


def test():
    override(0, 0, 200, 0, True, False, duration_ms=4000)


def estop_cmd():
    override(0, 0, 0, 0, False, True)


def set_cmd(roll=0, pitch=0, throttle=0, yaw=0, arm_value=1):
    override(roll, pitch, throttle, yaw, bool(arm_value), False, duration_ms=4000)


def help():
    print("Commands:")
    print("  neutral() / disarm")
    print("  arm()")
    print("  test()")
    print("  set_cmd(roll, pitch, throttle, yaw, arm_value)")
    print("  estop_cmd()")
    print("  live()")
    print("  stats()")


def stats():
    age = ticks_diff(ticks_ms(), last_ack_ms) if last_ack_ms else -1
    print("sent={} seq={} bridge_ack_age_ms={}".format(sent, sequence, age))


def handle_usb_command(line):
    line = line.strip()
    if not line:
        return

    if line == "help":
        help()
    elif line in ("neutral", "disarm"):
        neutral()
    elif line == "arm":
        arm()
    elif line == "test":
        test()
    elif line == "estop":
        estop_cmd()
    elif line == "live":
        live()
    elif line == "stats":
        stats()
    elif line.startswith("set "):
        parts = line.split()
        if len(parts) != 6:
            print("usage: set ROLL PITCH THROTTLE YAW ARM")
            return
        try:
            roll = int(parts[1])
            pitch = int(parts[2])
            throttle = int(parts[3])
            yaw = int(parts[4])
            arm_value = int(parts[5])
        except ValueError:
            print("set values must be integers")
            return
        override(roll, pitch, throttle, yaw, arm_value != 0, False, duration_ms=4000)
    else:
        print("unknown command:", line)
        print("type: help")


def poll_usb_commands():
    global usb_line

    while stdin_poll.poll(0):
        char = sys.stdin.read(1)
        if char in ("\r", "\n"):
            handle_usb_command(usb_line)
            usb_line = ""
        elif char == "\x03":
            raise KeyboardInterrupt
        elif len(usb_line) < 80:
            usb_line += char
        else:
            usb_line = ""
            print("USB command too long")


print()
print("========================================")
print("PICO UART -> ESP32 ESP-NOW CONTROLLER")
print("========================================")
print("UART{} TX GP{} -> ESP32 RX2/GPIO16, RX GP{} <- ESP32 TX2/GPIO17".format(
    UART_ID, UART_TX_PIN, UART_RX_PIN
))
print("USB commands: help, arm, disarm, test, set R P T Y ARM, live, stats")
print("Throttle: center=hold, up=increase, down=decrease, max rate=20%/s")
print("GP10/GP11: all-servos CW/CCW while held")
print("Trims reset on restart; control sensitivity starts at {}%".format(
    SENSITIVITY_DEFAULT_PERCENT
))
print("Streaming starts now.")

last_log = ticks_ms()
last_send = ticks_ms()
last_status_send = ticks_ms()
last_servo_button_send = ticks_ms()
last_servo_button_direction = 0

while True:
    poll_usb_commands()
    read_bridge_replies()

    now = ticks_ms()
    if ticks_diff(now, last_send) >= SEND_INTERVAL_MS:
        roll, pitch, throttle, yaw, arm_state, estop_state = read_controls()
        payload = build_payload(roll, pitch, throttle, yaw, arm_state, estop_state)
        send_payload(payload)
        sent += 1
        last_send = now

        if ticks_diff(now, last_status_send) >= STATUS_INTERVAL_MS:
            send_payload(build_status_payload())
            last_status_send = now

        servo_direction = servo_button_direction()
        if (
            servo_direction != last_servo_button_direction
            or (
                servo_direction != 0
                and ticks_diff(now, last_servo_button_send)
                >= SERVO_BUTTON_INTERVAL_MS
            )
        ):
            send_payload(build_servo_button_payload(servo_direction))
            last_servo_button_direction = servo_direction
            last_servo_button_send = now

        if ticks_diff(now, last_log) >= 1000:
            age = ticks_diff(now, last_ack_ms) if last_ack_ms else -1
            print(
                "seq={} arm={} estop={} r={} p={} t={} y={} sent={} bridge_ack_age_ms={}".format(
                    sequence,
                    int(arm_state),
                    int(estop_state),
                    roll,
                    pitch,
                    throttle,
                    yaw,
                    sent,
                    age,
                )
            )
            last_log = now

    update_link_led()
    time.sleep_ms(2)
