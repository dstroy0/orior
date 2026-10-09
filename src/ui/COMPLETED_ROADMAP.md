# The orior app: completed roadmap

What the window does that [the roadmap](ROADMAP.md) asked of it, by area. Each time given is measured with real keys on a file of 2,677 lines, from the key's press to the screen showing its answer.

## Responsiveness

1. **Go to File puts the exact name first.** A file whose name is what was typed comes above a longer name that holds it, and a name that starts with it comes next.
2. **No browser autofill.** Every field of the window refuses the web view's list of words typed before, and a search's results stand clear.
3. **Completion as you type.** The list opens on its own as a name is begun, as well as on Ctrl+Space. One question to the language server answers every letter typed while it is out, and the list stands about 0.85 s after the first letter.
4. **The minimap moves with the text.** A scroll or a jump redraws the minimap in the same frame as the code: a page down settles in about 110 ms, minimap and all.
5. **A language server ready before it is asked.** The servers for a tree's languages start as the tree opens, and a file handed to its server is read through at once. Go to Definition three seconds after a file opens answers in about 135 ms, as quickly as the next one.
6. **A tab back at its saved text shows no change.** Typing a letter and taking it out again leaves the tab marked as saved.
