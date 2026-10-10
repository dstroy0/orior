# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Start under a second.** The window shows with its page drawn and takes keys within a second of the launch.
2. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
3. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size.
4. **The hot paths in Rust.** The work the page does over every line, the highlighter, the search over the tree and the matching of names, runs in orior's own Rust, called once for a whole answer, and the page draws what comes back.
5. **Memory held down.** A tree of thousands of files open, with its servers, stays within a set budget of memory, and the status bar's reading of it names what holds the most.

## Editing

1. **Macros.** Keys recorded as they are pressed, played back once or many times, and kept under a name a key can be bound to.
2. **A history of undo that branches.** Changes undone and then typed over stay in the history as a branch of it, and a view of the history steps to any of them.
3. **Jump to any place on the screen.** A key marks every place in sight that matches a letter or two, and the mark's letter puts the cursor there.
4. **Vim's keys.** The editor's keys as Vim has them, its modes, motions, operators, registers and the `.` that repeats, set on or off in Preferences.
5. **What is not drawn.** Line ends, tabs and spaces shown as marks where the reader asks, and comments hidden and shown again with one key.
6. **Folds that keep the close.** A folded block shows its closing bracket on the line it starts on.
7. **Print.** The file open, or the part selected, printed in its colors with its line numbers.
8. **Colors from the text.** A log or an output file with ANSI color codes in it shows in those colors, the codes hidden.

## Code intelligence

1. **More refactorings of orior's own.** Extract Function in Rust and the C family, its parameters typed from the language server; change a function's parameters across every call; move a declaration to another file. Each is one step that undo takes back, and each works where the language server offers none.
2. **Problems of the whole tree.** Problems lists what is wrong in every file of the tree, and not only in the files open, each kept up to date as files change on the disk or in the editor.
3. **Inspections.** Checks of orior's own over the tree's languages, run as files change, each listed in Problems beside the servers' diagnostics with a quick fix where one applies: an import that leads back round to the file that makes it, a name set and never read, code no path reaches.
4. **Call hierarchy.** The functions that call the one at the cursor, and those it calls, each a tree that opens a level at a time, a press going to the call.
5. **Types written in the text.** The types a language server infers for names and parameters, drawn after them in the line and not part of it, each kind set on or off.
6. **Diagnostics in their own form.** A diagnostic whose server writes it in Markdown shows its code, its links and its lists as written.
7. **Structural search and replace.** Find code by its shape, with placeholders for names, expressions and statements, and replace each match from a template that uses what the placeholders took.

## Version control

1. **Branches.** Every local and remote branch in a list, to create, switch to, rename and delete, and to merge or rebase onto the branch open.
2. **Conflicts.** A merge window with three panes, theirs, the result and yours, each conflict taken from either side or written by hand, a press marking the file resolved.
3. **Part of a file in a commit.** The Commit window takes single changes, or single lines, of a file, and leaves the rest for a later commit.
4. **Compare.** Two revisions of a file side by side, or a file beside another, or beside the clipboard, with changes to white space shown or set aside.
5. **The commit graph.** Every branch's commits drawn as a graph, each with its message, its author and its date, a press opening its changes and a search over them.
6. **Line history.** Beside each line, the commit that last changed it, a press opening that commit's changes.
7. **Stash and cherry-pick.** Changes put aside and brought back, and a commit of another branch applied to the one open.
8. **More than one repository in a tree.** A tree holding several repositories shows each one's changes, branch and commits apart.
9. **Compare by structure.** A change shown by the code it moved, renamed or reshaped, and not by the lines it touched.

## Testing

1. **A test runner.** The tree's tests in a tree of their own, by file and by test, each marked passed, failed or skipped; a failure opens where it failed; failed tests run again with one press.
2. **Tests from the gutter.** A mark beside each test in the editor runs it, or debugs it.
3. **Coverage.** A run that records which lines ran, shown in the gutter and as a share by file.

## Debugging

1. **Breakpoints with conditions.** A breakpoint that stops only when an expression holds or after a count of hits, and one that writes a message to the console in place of stopping.
2. **Stop on exceptions.** The debugger stops where an exception is thrown, or only where nothing catches it.
3. **Values under the pointer.** The value of a name the pointer rests on while the program is stopped, opened a level at a time where it holds more.
4. **Breakpoints and watches kept.** The breakpoints and the watches written out to a file and read back in, and the values a stop showed kept to compare with a later stop.
5. **Attach.** Debug a program already running, chosen from a list of processes.
6. **Two programs at once.** More than one debug session open, each with its own threads and stack, the toolbar acting on the one chosen.
7. **Across languages.** A Python program and the C it calls debugged together, a step going from the one into the other.
8. **Memory and machine code.** The bytes at an address, and the instructions about the line stopped at, with a breakpoint that stops when the bytes at an address change.

## Layout

1. **Groups of tabs.** Each side of a split holds tabs of its own, any file in either.
2. **Tool windows anywhere.** A tool window dragged to any side of the window, or out into a window of its own, the terminal among them.
3. **More than one window.** A window for each tree open at once, each with its own tabs and terminals, a window for each display.
4. **Terminals side by side.** More than one shell open at a time, each in a tab of the terminal panel, or two side by side.
5. **Tabs down the side.** The tabs in a column beside the editor in place of a row above it, and tabs gathered in named groups that fold.
6. **A status bar of the reader's.** Each item of the status bar moved, hidden or shown, and the side bar hidden until the pointer reaches the window's edge.
7. **More than one tree in a window.** Folders from different places open together in one window, each with its own place in the explorer.

## Running and measuring

1. **A profiler.** A run measured as it goes, its time and its memory by function, shown as a flame graph a press on which opens the function.
2. **Runs on another machine.** A tree opened over SSH, or in a container, with the editor, the terminal, the jobs and the debugger working there, a dev container's own description among the ways in.

## Tools

1. **Databases.** Connect to a database, browse its tables, run a query from the editor and read the rows it gives.
2. **Requests.** A file of HTTP requests, each sent with a press and its answer shown beside it.
3. **A Markdown preview.** A Markdown file shown as it reads beside its text, scrolled with it, or open as the preview alone.
4. **Files in an archive.** A zip archive opened in the explorer as a folder, its files read without unpacking it.

## Settings

1. **Keys.** Any command's keys changed in Preferences, and whole sets of keys to choose from, the mouse's buttons and wheel bound as keys are, and the keys of a command shown where the reader reaches it another way.
2. **Settings by tree and by system.** Settings kept with a tree that stand over the reader's own, settings for one system only, a settings file that takes another's and changes some of it, and a theme for a tree or for a kind of file.
3. **Settings on every machine.** The reader's settings, keys, themes and plugins carried to another machine through a file or a repository of the reader's.
4. **Nothing from the network unasked.** orior fetches nothing, no server, no update, no package, until the reader says yes, and a setting holds it off the network entirely.
5. **A plugin catalog.** Plugins others publish, found by name and installed from File, Plugins, each set on or off from a file, and each told what it may read and change before it runs.
6. **Saving where rights are needed.** A file the reader may not write is saved after the system asks for the rights.
7. **Other languages and other scripts.** The window in the reader's language, and text that runs right to left typed and drawn in its order.
