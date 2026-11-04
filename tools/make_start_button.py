"""Make an English start button (LauncherImage\\StartButton_en.BMP) from the Korean one.

StartButton.BMP is a strip of four 154x86 states (normal, pressed, hover, disabled) with Korean
"게임 시작" (Game Start) baked in. The text sits on a face whose stripes run horizontally, so each
text row is rebuilt from the plain face on either side, then the English label is drawn in each
state's original text colour with the same dark shadow.

    python make_start_button.py <StartButton.BMP> <out.BMP> [label]
"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, dst = sys.argv[1], sys.argv[2]
label = sys.argv[3] if len(sys.argv) > 3 else 'START'
img = Image.open(src).convert('RGB')
W = img.width // 4
px = img.load()
X0, X1, Y0, Y1 = 30, 126, 27, 60          # text box inside each state (relative to the state)
font = ImageFont.truetype(r'C:\Windows\Fonts\arialbd.ttf', 21)
draw = ImageDraw.Draw(img)
for s in range(4):
    ox = s * W
    # The text colour: the brightest pixel in the text box.
    text = max((px[ox + x, y] for x in range(X0, X1) for y in range(Y0, Y1)), key=sum)
    for y in range(Y0, Y1):
        left, right = px[ox + X0 - 3, y], px[ox + X1 + 2, y]
        for x in range(X0, X1):
            t = (x - X0) / (X1 - X0)
            px[ox + x, y] = tuple(round(a * (1 - t) + b * t) for a, b in zip(left, right))
    box = draw.textbbox((0, 0), label, font=font)
    tx = ox + (W - (box[2] - box[0])) // 2 - box[0]
    ty = (86 - (box[3] - box[1])) // 2 - box[1] - 1
    draw.text((tx + 1, ty + 1), label, font=font, fill=(10, 12, 16))
    draw.text((tx, ty), label, font=font, fill=text)
img.save(dst)
print(f'{dst}: "{label}", 4 states of {W}x{img.height}')
