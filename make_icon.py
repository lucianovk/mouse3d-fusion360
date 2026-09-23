"""One-off script to generate icon.ico from the same design as the tray icon."""
from PIL import Image, ImageDraw

img = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.ellipse((24, 24, 232, 232), fill=(53, 103, 224, 255))
d.ellipse((64, 88, 192, 168), outline=(255, 255, 255, 255), width=12)
d.ellipse((112, 112, 144, 144), fill=(255, 255, 255, 255))
img.save("icon.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("icon.ico gerado")
