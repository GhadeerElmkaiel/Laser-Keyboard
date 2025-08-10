import serial
import time
import keyboard

# Define the keyboard layout matrix
key_matrix = [
    ['esc', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'backspace'],
    ['tab', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '\\'],
    ['capslock', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 'enter'],
    ['shift', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', 'up', 'enter'],
    ['ctrl', 'fn', 'win', 'alt', ' ', ' ', ' ', "'", '/', 'left', 'down', 'right']
]

# === Settings ===
PORT = 'COM3' # Use your port to which the Bluetooth module is connected, for example 'COM3'
BAUDRATE = 9600
MISSED_THRESHOLD = 3  # messages before releasing key
debug_mode = False # True

held_keys = {}  # (row, col) -> missed_count
prev_rows = []
prev_cols = []

# Connect to Bluetooth serial
ser = serial.Serial(PORT, BAUDRATE, timeout=0.01)
time.sleep(2)

# === Helpers ===
def get_key(row, col):
    max_r = len(key_matrix)
    max_c = len(key_matrix[0])
    if 0 <= row < max_r and 0 <= col < max_c:
        return key_matrix[max_r - row - 1][max_c - col - 1]
    return None

key_aliases = {
    ' ': 'space',
    '\\': 'backslash',
    'win': 'windows',
    'capslock': 'caps lock',
}

def normalize_key(key):
    return key_aliases.get(key, key)

def smart_key_detection_simple(prev_rows, prev_cols, curr_rows, curr_cols):
    new_keys = set()

    new_rows = set(curr_rows) - set(prev_rows)
    new_cols = set(curr_cols) - set(prev_cols)
    
    if len(new_rows) > 0 or len(new_cols) > 0:
        print(f"New rows: {new_rows}, New cols: {new_cols}")

    if len(new_rows) > 0 and len(new_cols) > 0:
        for r in new_rows:
            for c in new_cols:
                new_keys.add((r, c))
    elif len(new_rows) > 0:
        for r in new_rows:
            for c in prev_cols:
                new_keys.add((r, c))
    elif len(new_cols) > 0:
        for c in new_cols:
            for r in prev_rows:
                new_keys.add((r, c))

    return new_keys

def parse_message(message):
    try:
        if "R:" not in message or "C:" not in message:
            return [], [], False

        row_part, col_part = message.strip().split(', ')
        rows = row_part.replace('R: ', '').strip()
        cols = col_part.replace('C: ', '').strip()

        row_list = list(map(int, rows.split())) if rows else []
        col_list = list(map(int, cols.split())) if cols else []

        return row_list, col_list, True
    except ValueError:
        return [], [], True

def press_key(key):
    try:
        key = normalize_key(key)
        keyboard.press(key)
        print(f"Holding: {key}")
    except Exception as e:
        print(f"Error pressing {key}: {e}")

def release_key(key):
    try:
        key = normalize_key(key)
        keyboard.release(key)
        print(f"Released: {key}")
    except Exception as e:
        print(f"Error releasing {key}: {e}")

# === Main Loop ===
print("Listening for Bluetooth messages...")
while True:
    line = ser.readline().decode('utf-8').strip()

    curr_rows, curr_cols, is_valid = parse_message(line)
    if not is_valid:
        continue
    
    now = time.time()

    if debug_mode:
        if "R" not in line or "C" not in line:
            continue
        print("Received:", line)

    # --- Handle full keyboard disable
    if len(curr_rows) == 5 or len(curr_cols) == 12:
        print("Skipping message with full row/column data. Keyboard is off.")
        continue
    if len(curr_rows) > 2 or len(curr_cols) > 2:
        print("Skipping message with too many row/column data.")
        continue

    # --- Always update held_keys regardless of whether message changed
    detected_keys = {(r, c) for r in curr_rows for c in curr_cols}
    for key_pos in list(held_keys.keys()):
        if key_pos in detected_keys:
            held_keys[key_pos]['missed'] = 0
        else:
            held_keys[key_pos]['missed'] += 1
        # Release key if held for too long
        held_duration = now - held_keys[key_pos]['press_time']
        if held_duration > 2.0 or held_keys[key_pos]['missed'] >= MISSED_THRESHOLD:
            key = get_key(*key_pos)
            release_key(key)
            del held_keys[key_pos]

    # --- Detect and press new keys
    new_keys = smart_key_detection_simple(prev_rows, prev_cols, curr_rows, curr_cols)
    for (r, c) in new_keys:
        key = get_key(r, c)
        if key and (r, c) not in held_keys:
            press_key(key)
            held_keys[(r, c)] = {'missed': 0, 'press_time': now}

    # --- Update previous state
    prev_rows = curr_rows
    prev_cols = curr_cols
