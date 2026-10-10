# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. Under an item, each note names a complaint people make of the tools that do the same work, and what orior does so the reader never makes it. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
   - A scroll that stutters while a language server answers, a check runs or a file saves: the scroll waits on none of them.
   - An editor that draws 60 frames a second on a display of 120 or 144: the scroll is measured at the display's own rate, whatever it is.
2. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size. The page's own work for a key is 2 to 5 ms in files of 40 to 250,000 lines; what keeps the letter from the next frame at 120 a second is the frame's commit, 11 to 14 ms with the window's three layers the size of the window, which the trace of the scroll before this item reads.
   - Typing that lags while a language server, a linter or a plugin works: the letter is drawn before any of them is told of it.
   - Lag that grows with the file's length or a line's: the key's cost is measured in the longest file and on the longest line the tests hold.

## Notebooks

1. **Notebooks.** A notebook opened as cells, each run on its own and its output shown under it, with a minimap, forms in a cell's code drawn as fields, a cell debugged, and kernels in any language the machine has.
   - Values that no cell on the page gives, left by cells run out of order or since deleted: each cell shows when it ran in the kernel's order, and a cell whose output stands on a cell changed or run since is marked as out of date.
   - A notebook that runs only in the order its writer ran it: Restart and Run All is one press, and Check from the Top runs it in a new kernel and stops at the first cell that fails.
   - Diffs of a notebook that are JSON, outputs and run counts: the diff and the merge show cells' source and output, run counts and metadata aside; outputs can be kept out of a commit; and a script of `# %%` cells opens as a notebook.
   - Kernels that are not found, or a search for them that never ends: the kernels are those of the machine's toolchains and the tree's environment, found as the toolchains window finds them, and an environment without ipykernel offers to install it.
   - A large output that freezes the window: past a size, an output shows its first and last lines, the whole a press away.
   - A kernel that will not stop or restart: Interrupt, then Restart, then Stop, which ends the kernel's process where it does not answer.

## Tools

1. **Databases.** Connect to a database, browse its tables, run a query from the editor and read the rows it gives.
   - A large schema whose reading takes minutes and is read again whole after each change: the schema is read a level at a time as it is opened, an object changed is read again alone, and the editor never waits on it.
   - An UPDATE or a DELETE with no WHERE run against the wrong database: a connection marked as one that matters asks before any statement that writes, and a write with no WHERE asks on every connection, with the count of rows it would change.
   - A large result that freezes the client: rows come a page at a time as the grid scrolls, and a query running can be stopped.
   - A client that starts slowly and holds gigabytes: the database window starts with the window and holds the rows shown.
   - Passwords kept in the client's files: passwords go to the system's keychain, and a tunnel goes through the system's ssh and its agent.
   - A transaction left open: a connection with a transaction open says so on the status bar until it is committed or rolled back.
   - A driver fetched without the reader asking: none is fetched until the reader says yes.
2. **Requests.** A file of HTTP requests, each sent with a press and its answer shown beside it.
   - A sign-in and a cloud account to send a request: none; requests are `.http` files in the tree, plain text kept with git.
   - Secrets in a file that gets committed: each environment's private values come from a file of the reader's, which orior adds to `.gitignore` as it makes it.
   - Answers lost once the next is sent: each request keeps its last answers, and one is compared with another.
   - Requests copied from a browser or a terminal: a curl line pasted becomes a request, and Copy as curl gives one back.
   - A client that holds a gigabyte to send text: requests go out from the window's own process.
3. **A Markdown preview.** A Markdown file shown as it reads beside its text, scrolled with it, or open as the preview alone, as the reader chooses for every Markdown file.
   - A preview that jumps back to the top as the text is typed, or scrolls out of step with it: each block of the preview is tied to the line it came from, and the two scroll by those lines.
   - A preview that differs from GitHub's: tables, task lists, footnotes, alerts, math and Mermaid are drawn as GitHub draws them.
   - Images by a path relative to the file that do not show: an image's path is taken from the file's own folder.
   - A large file whose preview holds up the editor: only the blocks that changed are drawn again, and never on the editor's time.
4. **A live preview pane.** A pane beside the editor that renders the file being typed as it is typed, for every language that sets how something looks and runs nothing: Markdown, HTML and the CSS it takes, a CSS file over a page of its own, Mermaid's diagrams, LaTeX, SVG and the like. Each key is drawn in it as the text changes, and the place the cursor stands in the text is shown in the pane.
   - A page reloaded whole on each change, its scroll, its form's fields and its state lost: a stylesheet changed is swapped in place, and a page changed is patched where it changed, at the same scroll.
   - LaTeX that takes seconds to show a change: the last good page stays while the next one is made, and SyncTeX takes the reader from the text to the page and back.
   - A diagram drawn again on every key: a diagram is drawn again only when its own block changes.
5. **Files in an archive.** A zip archive opened in the explorer as a folder, its files read without unpacking it.
   - Names that come out garbled: each name is read in the encoding the archive's flag gives, UTF-8 or the old DOS page, and the reader can choose another.
   - An archive inside an archive: it opens as a folder too.
   - Files in an archive that can only be read: a file changed in an archive is saved back into it, the archive written anew beside the old one and then put in its place.
   - Jars, wheels and tarballs that are archives too: a jar, a wheel, a tar and a gzipped tar open as a zip does.

## Plugins

1. **Plugins that draw.** A plugin lays marks, overlays and its own panels over the editor and the window, sets the label of a tab, reads the theme's colors, and changes the window's chrome.
   - Plugins that reach into the editor's own parts and break when it changes: a plugin draws through calls whose form is kept from one version to the next, and a plugin written for an older form goes on working.
2. **Plugins off the editor's thread.** Each plugin runs apart from the editor, and one that is slow or stuck holds up neither the keys nor the others.
   - One plugin's slowness that freezes all the others, which share its process: each plugin has its own, and one that does not answer is named on the status bar with its time, and can be stopped from there.
   - A window slowed by a plugin the reader cannot name: Find the Slow Plugin turns half of them off at a time until the one is found.
3. **Plugins that write files with care.** A plugin that serves files told when the system refuses a write, and the reader asked for the rights the write needs.

## Settings

1. **Keys.** Any command's keys changed in Preferences, and whole sets of keys to choose from, the mouse's buttons and wheel bound as keys are, the keys of a command shown where the reader reaches it another way, and keys that work whatever the keyboard's layout.
   - Keys that do the wrong thing on AZERTY, QWERTZ and the like, as Undo closing a tab: a key is bound by the character it types or by its place on the board, as the reader sets, and the window's own keys work on every layout.
   - Two commands on one key, found only when one of them fails: Preferences shows each key that more than one command holds.
2. **Settings by tree and by system.** Settings kept with a tree that stand over the reader's own, settings for one system only, a settings file that takes another's and changes some of it, settings that name others' values and the system's, and a theme for a tree or for a kind of file.
   - A setting whose value the reader cannot account for: Preferences shows each setting's value and the file it comes from.
3. **Themes that reach further.** A theme sets an image behind the editor, colors for every kind of token a language names, and file icons matched to names by patterns.
   - A theme whose text is hard to read on its own background: Preferences says where a color's contrast with the color under it is too low.
4. **Settings on every machine.** The reader's settings, keys, themes and plugins carried to another machine through a file or a repository of the reader's.
   - One machine's settings written over another's without a question: nothing is written over without the differences shown first, each carrying is kept so it can be undone, and settings for one machine stay on it.
   - Plugins' own settings that do not travel: a plugin's settings travel with it.
5. **Nothing from the network unasked.** orior fetches nothing, no server, no update, no package, until the reader says yes, and a setting holds it off the network entirely.
   - Editors that still call home with their telemetry off, through experiments, recommendations or plugins: orior sends nothing about the reader, and Network lists every address it has reached, when, and for what.
6. **A plugin catalog.** Plugins others publish, found by name and installed from File, Plugins, each set on or off from a file, and each told what it may read and change before it runs.
   - Plugins in a catalog that carry malware and run with all of the reader's rights: each plugin says what it reads, writes and reaches on the network, orior holds it to that, and an update that asks for more asks the reader again.
   - Plugins that update themselves unasked: a plugin is updated when the reader says so.
7. **Saving where rights are needed.** A file the reader may not write is saved after the system asks for the rights.
   - A save with raised rights that fails, or offers no way to raise them: on Windows the system's own prompt raises a helper that writes that file alone, on Linux pkexec does, and on macOS the system's authorization does.
8. **Other languages and other scripts.** The window in the reader's language, and text that runs right to left typed and drawn in its order.
   - A cursor that jumps the wrong way, or words drawn in reverse, where Arabic or Hebrew meets text that runs left to right: the arrows move by the order the text is read in, and the word keys stop at the words of either script.
9. **Packages for Linux.** orior installed from Flathub and the Snap Store as well as from its own release.
   - An editor from Flathub that cannot reach the machine's compilers, SDKs and tools from inside its sandbox: orior's Flatpak runs the tree's tools and its terminal's shell on the machine itself, through `flatpak-spawn --host`.
