"""Crop each occurrence of the wanted strings out of a 600 dpi render, with the text around it, for reading by eye."""
import os
import re
import sys

import pypdfium2 as pdfium

pdf, out = sys.argv[1], sys.argv[2]
wanted = sys.argv[3:]
os.makedirs(out, exist_ok=True)
scale = float(os.environ.get("DPI", "600")) / 72.0
document = pdfium.PdfDocument(pdf)
for number in range(len(document)):
    page = document[number]
    height = page.get_height()
    textpage = page.get_textpage()
    text = textpage.get_text_range()
    image = None
    for one in wanted:
        for hit in re.finditer(re.escape(one), text):
            if image is None:
                image = page.render(scale=scale).to_pil()
            boxes = [textpage.get_charbox(index) for index in range(hit.start(), hit.end())]
            left = min(box[0] for box in boxes)
            bottom = min(box[1] for box in boxes)
            right = max(box[2] for box in boxes)
            top = max(box[3] for box in boxes)
            pad = float(os.environ.get("PAD", "70"))
            crop = image.crop((int(max(0, left - pad) * scale), int((height - top - 8) * scale),
                               int((right + 70) * scale), int((height - bottom + 8) * scale)))
            label = re.sub(r"\W", "", one.encode("ascii", "ignore").decode()) or "w%d" % wanted.index(one)
            target = os.path.join(out, "p%d_%s_%d.png" % (number + 1, label, int(top)))
            crop.save(target)
            print(number + 1, one, target)
