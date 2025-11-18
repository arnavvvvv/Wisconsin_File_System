#!/usr/bin/env python3
import os
import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <file> <color>")
    print("Colors: none, red, green, blue, yellow, magenta, cyan, white, black, orange, purple, gray")
    sys.exit(1)

filepath = sys.argv[1]
color = sys.argv[2]

os.setxattr(filepath, "user.color", color.encode())
print(f"Set {filepath} to {color}")