"""Crop a band of a rendered page, for reading by eye what the text layer and pdfium both lost.

  python page_crop.py PDF PAGE TOP BOTTOM OUT.png [DPI]

PAGE counts from 1; TOP and BOTTOM are fractions of the page height from the top.
"""
import sys

import pypdfium2 as pdfium


def main():
    pdf, page, top, bottom, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
    dpi = float(sys.argv[6]) if len(sys.argv) > 6 else 200.0
    image = pdfium.PdfDocument(pdf)[page - 1].render(scale=dpi / 72.0).to_pil()
    width, height = image.size
    image.crop((0, int(top * height), width, int(bottom * height))).save(out)
    print(out)


if __name__ == "__main__":
    main()
