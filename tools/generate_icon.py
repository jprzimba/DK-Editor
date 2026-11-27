"""Generate the source app icon (1024x1024 PNG) with Pillow.

The design is intentionally simple so it stays readable at 16x16:
dark rounded square, a subtle level grid, a 3x2 terrain block cluster with a
selection outline, and a green pencil.
"""

import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "resources", "DKEditor.png")

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# Background rounded square.
bg_box = (40, 40, S - 40, S - 40)
radius = 180
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(shadow)
sd.rounded_rectangle((bg_box[0] + 8, bg_box[1] + 18, bg_box[2] + 8, bg_box[3] + 18),
                     radius, fill=(0, 0, 0, 120))
shadow = shadow.filter(ImageFilter.GaussianBlur(18))
img.alpha_composite(shadow)

d.rounded_rectangle(bg_box, radius, fill=(13, 21, 37, 255),
                    outline=(44, 76, 110, 255), width=10)
d.rounded_rectangle((70, 70, S - 70, S - 70), radius - 40,
                    outline=(28, 52, 82, 255), width=4)

# Subtle grid, clipped to the rounded square.
grid = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gd = ImageDraw.Draw(grid)
step = 92
for x in range(70, S - 60, step):
    gd.line((x, 70, x, S - 70), fill=(33, 60, 92, 90), width=3)
for y in range(70, S - 60, step):
    gd.line((70, y, S - 70, y), fill=(33, 60, 92, 90), width=3)
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle((70, 70, S - 70, S - 70), radius - 40, fill=255)
img.alpha_composite(Image.composite(grid, Image.new("RGBA", (S, S), (0, 0, 0, 0)), mask))

# Terrain block cluster.
tile = 190
gap = 12
cols, rows = 3, 2
cluster_w = cols * tile + (cols - 1) * gap
cluster_h = rows * tile + (rows - 1) * gap
x0 = (S - cluster_w) // 2
y0 = (S - cluster_h) // 2 + 10

block_shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
bs = ImageDraw.Draw(block_shadow)
bs.rounded_rectangle((x0 - 8, y0 + 16, x0 + cluster_w + 8, y0 + cluster_h + 24),
                     28, fill=(0, 0, 0, 100))
block_shadow = block_shadow.filter(ImageFilter.GaussianBlur(16))
img.alpha_composite(block_shadow)

for r in range(rows):
    for c in range(cols):
        tx = x0 + c * (tile + gap)
        ty = y0 + r * (tile + gap)
        grass_h = 54 if r == 0 else 26
        d.rounded_rectangle((tx, ty, tx + tile, ty + tile), 18,
                            fill=(198, 137, 74, 255), outline=(92, 58, 30, 255), width=6)
        d.rounded_rectangle((tx + 4, ty + 4, tx + tile - 4, ty + grass_h), 14,
                            fill=(74, 155, 56, 255))
        d.rectangle((tx + 8, ty + grass_h - 8, tx + tile - 8, ty + grass_h + 4),
                    fill=(58, 128, 44, 255))
        # Dirt pixels.
        for (px, py, pw, ph, col) in [
            (tx + 30, ty + 80, 34, 22, (176, 113, 58, 255)),
            (tx + 110, ty + 74, 46, 26, (176, 113, 58, 255)),
            (tx + 58, ty + 132, 58, 20, (176, 113, 58, 255)),
            (tx + 130, ty + 138, 26, 26, (156, 96, 48, 255)),
            (tx + 24, ty + 112, 18, 18, (156, 96, 48, 255)),
        ]:
            d.rectangle((px, py, px + pw, py + ph), fill=col)
        # Grass highlights.
        for gx in range(tx + 18, tx + tile - 18, 34):
            d.rectangle((gx, ty + 18, gx + 12, ty + 34), fill=(105, 196, 80, 255))

# Selection outline on the center-top block.
sx = x0 + (tile + gap)
sy = y0
d.rounded_rectangle((sx - 8, sy - 8, sx + tile + 8, sy + tile + 8), 22,
                    outline=(86, 220, 255, 255), width=9)
handle = 15
for hx, hy in [(sx - 8, sy - 8), (sx + tile + 8 - handle, sy - 8),
               (sx - 8, sy + tile + 8 - handle), (sx + tile + 8 - handle, sy + tile + 8 - handle)]:
    d.rectangle((hx, hy, hx + handle, hy + handle), fill=(230, 252, 255, 255),
                outline=(20, 90, 120, 255), width=3)

# Pencil (drawn horizontally, then rotated).
pw, ph = 420, 116
pencil = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
pd = ImageDraw.Draw(pencil)
# Body.
pd.rectangle((80, 30, 330, 86), fill=(38, 196, 106, 255), outline=(9, 52, 32, 255), width=7)
pd.rectangle((96, 40, 318, 54), fill=(110, 240, 163, 255))
# Wood and graphite tip.
pd.polygon([(330, 26), (400, 58), (330, 90)], fill=(232, 196, 132, 255),
           outline=(9, 52, 32, 255))
pd.polygon([(382, 51), (400, 58), (382, 65)], fill=(38, 42, 54, 255),
           outline=(9, 52, 32, 255))
# Eraser and metal band.
pd.rectangle((34, 30, 80, 86), fill=(240, 166, 186, 255), outline=(9, 52, 32, 255), width=7)
pd.rectangle((66, 30, 82, 86), fill=(201, 208, 219, 255), outline=(9, 52, 32, 255), width=4)
pencil = pencil.rotate(135, expand=True, resample=Image.BICUBIC)
# Place the pencil so it points toward the blocks at the upper left.
px = 560
py = 650
img.alpha_composite(pencil, (px, py))

img.save(OUT)
print(f"wrote {OUT}")
