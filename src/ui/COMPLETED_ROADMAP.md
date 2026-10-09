# The orior app: completed roadmap

What the window does that [the roadmap](ROADMAP.md) asked of it, by area. Each time given is measured with real keys on a file of 2,677 lines, from the key's press to the screen showing its answer.

## Responsiveness

1. **Go to File puts the exact name first.** A file whose name is what was typed comes above a longer name that holds it, and a name that starts with it comes next.
2. **No browser autofill.** Every field of the window refuses the web view's list of words typed before, and a search's results stand clear.
3. **Completion as you type.** The list opens on its own as a name is begun, as well as on Ctrl+Space. One question to the language server answers every letter typed while it is out, and the list stands about 0.85 s after the first letter.
4. **The minimap moves with the text.** A scroll or a jump redraws the minimap in the same frame as the code: a page down settles in about 110 ms, minimap and all.
5. **A language server ready before it is asked.** The server for each tab a tree reopens starts as the tree opens, and a file handed to its server is read through at once. A language with no tab open starts no server, and a tree opened in place of another stops the servers the first started. Go to Definition three seconds after a file opens answers in about 135 ms, as quickly as the next one.
6. **A tab back at its saved text shows no change.** Typing a letter and taking it out again leaves the tab marked as saved.
7. **A large file as quick as a small one.** A file of 50 MB and 870,025 lines opens in about 120 ms, goes to its end in about 170 ms, takes a letter there in about 60 ms and pages in about 120 ms: each the time the file of 2,677 lines takes, and each settled as soon. The rest of the file is read in behind the view, by the page's clock 2.6 s from its top or 5.3 s from its end, and a row far down it is drawn as sharply as the first.
8. **Smooth motion counted.** Counted frame by frame on a display that draws 120 frames a second: the lattice draws at about 114 a second and misses none of 60, and a burning fuse holds all 120. A fast scroll through a file of 2,677 lines or of 870,025 holds 60 a second, missing at most 4 frames in 4 s, and reaches 77 to 88 of the display's 120.
9. **Start measured.** Launched cold from the release build, the window is drawn in about 0.96 s and its keys are bound in about 1.04 s: the middle of five launches, each read from the page's own clock against the moment of the launch. The page itself starts about 0.65 s in.

## Code intelligence

1. **Symbols of the whole tree.** Go to Symbol in Tree, Ctrl+T or `#` in Go to File, and Search Everywhere find a declaration in any file of the tree, from an index of what each file declares, read by the line as the outline reads the open file. A file is read again once it changes. The whole of this repository is read in about 1.1 s, behind the window, and a search answers in about 70 ms.
2. **Refactorings of orior's own.** Extract Variable, Ctrl+Alt+V, and Extract Constant, Ctrl+Alt+C, put the expression selected into a variable above its statement or a constant at the head of the file, and leave the new name selected in both places for the name typed next. Inline Variable, Ctrl+Alt+N, puts a variable's value where it is used and takes its declaration out, and refuses where the variable is given another value. Each is one change undo takes back, in Python, JavaScript, Rust, the C family, Ruby, R, MATLAB, Octave and PowerShell, from the Edit menu's Refactor group and the editor's own menu.
