import json
import os

# Define relative paths
json_path = os.path.join("..", "config", "constants.json")
header_path = os.path.join("..", "sterownik_indi", "constants.h")

def get_most_restrictive_cpp_type(val):
    """Evaluates the integer size to return the smallest viable C++ data type."""
    if isinstance(val, int):
        # 8-bit types
        if 0 <= val <= 255:
            return "const uint8_t"  # Fits in 1 unsigned byte
        if -128 <= val <= 127:
            return "const int8_t"  # Fits in 1 signed byte

        # 16-bit types
        if 0 <= val <= 65535:
            return "const uint16_t"
        if -32768 <= val <= 32767:
            return "const int16_t"  # e.g., -1000 fits perfectly here

        # 32-bit types
        if 0 <= val <= 4294967295:
            return "const uint32_t"
        return "const int32_t"

    elif isinstance(val, str):
        return "const char* const"

    raise TypeError(f"Unsupported data type in JSON: {type(val)}")


def build_header():
    if not os.path.exists(json_path):
        print(f"Error: Could not find {json_path}")
        return

    with open(json_path, "r") as f:
        data = json.load(f)

    constants = data.get("CONSTANTS", {})

    with open(header_path, "w") as h:
        h.write("// Auto-generated constants file. Do not edit directly.\n")
        h.write("#ifndef CONSTANTS_H\n#define CONSTANTS_H\n\n")
        h.write("#include <Arduino.h> // Includes core integer types like uint8_t\n\n")

        for key, value in constants.items():
            cpp_type = get_most_restrictive_cpp_type(value)

            # Formatting strings vs formatting numbers
            if isinstance(value, str):
                h.write(f'{cpp_type} {key} = "{value}";\n')
            else:
                h.write(f'{cpp_type} {key} = {value};\n')

        h.write("\n#endif\n")
    print(f"✓ {header_path} generated successfully with dynamic C++ typing.")


if __name__ == "__main__":
    build_header()