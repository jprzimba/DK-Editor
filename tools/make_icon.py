"""Build the Windows .ico and a preview PNG from the source app icon.

Usage: python tools/make_icon.py [source.png]
"""

import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "src", "resources", "DKEditor.png")
ICO = os.path.join(ROOT, "src", "resources", "DKEditor.ico")
PREVIEW = os.path.join(ROOT, "docs", "images", "icon.png")

SIZES = [16, 24, 32, 48, 64, 128, 256]

image = Image.open(SRC).convert("RGBA")
# Square crop, then resize with a high-quality filter.
side = min(image.width, image.height)
left = (image.width - side) // 2
top = (image.height - side) // 2
image = image.crop((left, top, left + side, top + side))
image = image.resize((256, 256), Image.LANCZOS)

os.makedirs(os.path.dirname(ICO), exist_ok=True)
os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
image.save(ICO, format="ICO", sizes=[(s, s) for s in SIZES])
image.save(PREVIEW, format="PNG")
print(f"wrote {ICO}")
print(f"wrote {PREVIEW}")
