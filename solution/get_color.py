
#!/usr/bin/env python3
import os
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <file>")
    sys.exit(1)

filepath = sys.argv[1]
try:
    color = os.getxattr(filepath, "user.color").decode()
    print(color)
except:
    print("No color set")
