"""Print the shape tests paper_sift applies to three lines: sentence, segmentation, glosses."""
import re
import sys
import os

from workdir import PRIVATE  # noqa: E402

from paper_sift import same_letters


def gloss_shaped(text, share=3):
    pieces = [piece for piece in re.split(r"[\s=.\-\[\]]+", text) if piece]
    capitals = [piece for piece in pieces if re.fullmatch(r"[0-9]*[A-Z][A-Z0-9]*", piece)]
    return bool(pieces) and len(capitals) * share >= len(pieces) and len(text) < 120 and \
        any(len(piece) >= 2 and not piece.isdigit() for piece in capitals)


def main():
    with open(os.path.join(PRIVATE, "pagetext", sys.argv[1] + ".txt"), encoding="utf-8") as handle:
        text = handle.read().split("\n")
    at = int(sys.argv[2]) - 1
    body, seg, gloss = text[at], text[at + 1], text[at + 2]
    print(repr(body), repr(seg), repr(gloss))
    print("same 0.8", same_letters(body, seg), "same 0.6", same_letters(body, seg, 0.6))
    print("gloss 3", gloss_shaped(gloss), "gloss 4", gloss_shaped(gloss, 4), "body gloss", gloss_shaped(body))


if __name__ == "__main__":
    main()
