"""Stack crop images vertically into one sheet, in the order given."""
import sys

from PIL import Image

out = sys.argv[1]
images = [Image.open(path) for path in sys.argv[2:]]
width = max(image.width for image in images)
sheet = Image.new("RGB", (width, sum(image.height for image in images)), "white")
top = 0
for image in images:
    sheet.paste(image, (0, top))
    top += image.height
sheet.save(out)
