"""Builds LauncherImage/LangOn.bmp and LangOff.bmp: 4-frame strips (normal, pressed, hover, disabled) for the
"한국어 / Korean" language toggle, drawn on the MainFrame.BMP pixels under the button (left of the logo, mirroring
the DXVK toggle). On = Korean launcher, game text and voice files; off = English.
usage: python tools/make_language_checkbox.py   (position must match CWZLauncherDlg::OnInitDialog)"""
import os
from PIL import Image, ImageDraw, ImageFont

X, Y, W, H = 84, 33, 150, 16
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
img = os.path.join(here, 'bundle', 'LauncherImage')
bg = Image.open(os.path.join(img, 'MainFrame.BMP')).convert('RGB').crop((X, Y, X + W, Y + H))
font = ImageFont.truetype(r'C:\Windows\Fonts\malgun.ttf', 11)
for on in (True, False):
    strip = Image.new('RGB', (W * 4, H))
    for f, (text, box) in enumerate([((235, 240, 245), (150, 175, 200)), ((255, 255, 255), (200, 225, 255)),
                                     ((255, 255, 255), (200, 225, 255)), ((120, 125, 130), (90, 95, 100))]):
        fr = bg.copy()
        d = ImageDraw.Draw(fr)
        d.rectangle((2, 2, 13, 13), fill=(10, 18, 28), outline=box)
        if on:
            d.line([(4, 8), (7, 11), (12, 4)], fill=(120, 200, 255) if f != 3 else box, width=2)
        d.text((19, -1), '한국어 / Korean', font=font, fill=text)
        strip.paste(fr, (W * f, 0))
    strip.save(os.path.join(img, 'LangOn.bmp' if on else 'LangOff.bmp'))
print('ok')
