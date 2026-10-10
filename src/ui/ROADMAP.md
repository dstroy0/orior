# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Start under a second.** The window shows with its page drawn and takes keys within a second of the launch.
2. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
3. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size.
4. **The highlighter in Rust.** The colors of every line come from orior's own Rust, worked out once for the lines a change reaches, and the page draws what comes back.
5. **Memory held down.** A tree of thousands of files open, with its servers, stays within a set budget of memory, and the status bar's reading of it names what holds the most.
6. **Smooth and gliding scrolls.** A wheel's step or a key's page scrolls in a short glide and not a jump, a touchpad's flick runs on and slows on every system, each set on or off.
7. **A restart that loses nothing.** orior closed or stopped and started again opens with every tab, every change not yet saved, every split and every terminal's place where it stood.

## Editing

1. **Macros.** Keys recorded as they are pressed, played back once or many times, and kept under a name a key can be bound to.
2. **A history of undo that branches.** Changes undone and then typed over stay in the history as a branch of it, and a view of the history steps to any of them.
3. **Jump to any place on the screen.** A key marks every place in sight that matches a letter or two, and the mark's letter puts the cursor there.
4. **Vim's keys.** The editor's keys as Vim has them, its modes, motions, operators, registers and the `.` that repeats, set on or off in Preferences.
5. **What is not drawn.** Line ends, tabs and spaces shown as marks where the reader asks, and comments hidden and shown again with one key.
6. **Folds that keep the close.** A folded block shows its closing bracket on the line it starts on.
7. **Print.** The file open, or the part selected, printed in its colors with its line numbers.
8. **Colors from the text.** A log or an output file with ANSI color codes in it shows in those colors, the codes hidden.
9. **Columns past the end of a line.** The cursor goes past the end of a short line into the space beyond it, and a column selection made by dragging with Alt held takes the same columns of every line, short lines among them.
10. **Indentation by file.** Each file's tab size and whether it indents with tabs or spaces, as an `.editorconfig` or a line in the file sets it, over the tree's own.

## Code intelligence

1. **More refactorings of orior's own.** Change a function's parameters across every call; move a declaration to another file. Each is one step that undo takes back, and each works where the language server offers none.
2. **Problems of the whole tree.** Problems lists what is wrong in every file of the tree, and not only in the files open, each kept up to date as files change on the disk or in the editor.
3. **Inspections.** Checks of orior's own over the tree's languages, run as files change, each listed in Problems beside the servers' diagnostics with a quick fix where one applies: an import that leads back round to the file that makes it, a name set and never read, code no path reaches, a docstring out of PEP 257's form, and a method a mixin class supplies taken as found.
4. **Call hierarchy.** The functions that call the one at the cursor, and those it calls, each a tree that opens a level at a time, a press going to the call.
5. **Types written in the text.** The types a language server infers for names and parameters, drawn after them in the line and not part of it, each kind set on or off.
6. **Diagnostics in their own form.** A diagnostic whose server writes it in Markdown shows its code, its links and its lists as written.
7. **Highlighting from a parse.** Each file highlighted from the tree of its code, parsed as it changes, and not from patterns over a line at a time: every name colored by what it is, and folds and Expand Selection taken from the same tree.
8. **Structural search and replace.** Find code by its shape, with placeholders for names, expressions and statements, and replace each match from a template that uses what the placeholders took.
9. **Code written for the reader.** A docstring drawn up from a function's parameters, its returns and what it raises, in a form of the reader's; a class's methods sorted by name; and a project begun from a template of the reader's.
10. **Regions in Structure.** The regions a file's comments mark show in Structure as nodes that hold what stands in them, and a file can be pinned there while another is open.

## Languages and formatting

1. **Type checkers and linters in the editor.** Mypy, flake8 and the tree's other checkers run as files change, each one's findings in Problems beside the language server's, with no setup past naming the checker.
2. **The tree's environments.** A virtual environment's path kept relative to the tree, a Pipfile or a `setup.cfg` read wherever it stands, and Nix and Pipenv each found and used as the tree's own environment.
3. **Formatting of the reader's.** The continuation indent set apart for each place it falls, an operator's sign moved to the next line where a line breaks, documentation held to a margin of its own, and Javadoc's tags lined up.
4. **Build files read.** Gradle with its version catalogs, Maven's `pom.xml` with its compiler and global settings shown as settings, and SCons, each completed and checked as it is written, a snapshot dependency updated with one press.
5. **More languages.** Zig, Mojo, Quarto's `.qmd`, Org, Jenkins pipelines, Ansible, API Blueprint, RAML and StringTemplate, each highlighted, folded and outlined, and served where a server for it is found.
6. **JSON by its schema.** A JSON file checked and completed from the schema it names, a schema's own schema among them.
7. **Python, fast and whole.** Python's server answers as quickly as any language's, recursive type hints resolve, and a file's imports, its environment and its tests are found with no setup.

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
10. **Review comments.** A comment left on a line of a change, as a pull request's review leaves one, kept with the change and listed with the others.

## Testing

1. **A test runner.** The tree's tests in a tree of their own, by file and by test, each marked passed, failed or skipped; a failure opens where it failed; failed tests run again with one press.
2. **Tests from the gutter.** A mark beside each test in the editor runs it, or debugs it.
3. **Coverage.** A run that records which lines ran, shown in the gutter and as a share by file.
4. **Tests in parallel.** The tests of a run spread over the machine's cores, each one's result coming in as it finishes.

## Debugging

1. **Breakpoints with conditions.** A breakpoint that stops only when an expression holds or after a count of hits, and one that writes a message to the console in place of stopping.
2. **Stop on exceptions.** The debugger stops where an exception is thrown, or only where nothing catches it, and a stop on an exception can be stepped past to carry on.
3. **Values under the pointer.** The value of a name the pointer rests on while the program is stopped, opened a level at a time where it holds more.
4. **Breakpoints and watches kept.** The breakpoints and the watches written out to a file and read back in, and the values a stop showed kept to compare with a later stop, two values compared side by side field by field.
5. **Attach.** Debug a program already running, chosen from a list of processes, or running in a container.
6. **Two programs at once.** More than one debug session open, each with its own threads and stack, the toolbar acting on the one chosen, the processes a program starts debugged with it.
7. **Across languages.** A Python program and the C it calls debugged together, Cython among them, a step going from the one into the other.
8. **Stepping of the tree's own.** The code a step goes into or passes over, set for each tree, and the frames a stop shows filtered by a pattern of the reader's.
9. **Back in time.** A run recorded as it goes, and the debugger stepping backward through it as well as forward, to the line and the values where a value went wrong.
10. **Memory and machine code.** The bytes at an address, and the instructions about the line stopped at, bytecode among them, with a breakpoint that stops when the bytes at an address change.

## Layout

1. **Groups of tabs.** Each side of a split holds tabs of its own, any file in either, a file moved to the next split with one key and the next file opened there.
2. **Tool windows anywhere.** A tool window dragged to any side of the window, or out into a window of its own, the terminal among them, each side's width kept for each tool window, and a panel the reader has no use for taken out entirely.
3. **More than one window.** A window for each tree open at once, each with its own tabs and terminals, a window for each display, and a tab dragged out of the window opening in a new one.
4. **Terminals side by side.** More than one shell open at a time, each in a tab of the terminal panel, or two side by side.
5. **Tabs down the side.** The tabs in a column beside the editor in place of a row above it, tabs gathered in named groups that fold, and a short list of files marked for a key each.
6. **A status bar of the reader's.** Each item of the status bar moved, hidden or shown, and the side bar hidden until the pointer reaches the window's edge.
7. **Menus and a toolbar of the reader's.** The menus and a toolbar under them laid out by the reader in their own JSON, each item a command.
8. **More than one tree in a window.** Folders from different places, or single files from them, open together in one window, each with its own place in the explorer, and a `.code-workspace` file opened as such a window.
9. **Focus that follows the pointer.** A pane, a list or the editor takes the keys when the pointer goes over it, set on or off.
10. **The system's title bar.** The window drawn with the system's own title bar where the reader asks for it.

## Explorer and files

1. **Type to search in the explorer.** Letters typed in the explorer find the files that match in the tree shown, each match marked, the next and the last a key away.
2. **Files between windows and the system.** Files copied or cut in the explorer pasted into another window of orior, or into the system's file manager, and files copied there pasted into the explorer.
3. **Every file but some.** The explorer, the search and the watching of files each kept from files by patterns of their own, one pattern able to keep all files but those it names.
4. **Search in a set of files.** Find in Files searches a set of files named and kept, as well as the whole tree.
5. **Breadcrumbs that go somewhere.** Each part of the breadcrumbs opens a list of the files and folders beside it, a press opening one.
6. **orior's own files in one place.** Its settings, its plugins, its themes and its kept state in one folder of the reader's, laid out as the documentation says.

## Running and measuring

1. **A profiler.** A run measured as it goes, its time and its memory by function, shown as a flame graph a press on which opens the function, on this machine or another.
2. **Runs on another machine.** A tree opened over SSH, or in a container, with the editor, the terminal, the jobs and the debugger working there, a dev container's own description among the ways in, a run's output read again after the connection comes back.
3. **Containers.** Docker containers and Compose services listed, started and stopped, a command run in one, a program in one debugged, and a Kubernetes cluster's pods and their logs read.
4. **Executables.** A program built into a file that runs on its own, from the tree's build scripts, with the build's output in a job.
5. **Deployments.** A build's files sent to the places a tree names, to several at once.

## Notebooks

1. **Notebooks.** A notebook opened as cells, each run on its own and its output shown under it, with a minimap, forms in a cell's code drawn as fields, a cell debugged, and kernels in any language the machine has.

## Tools

1. **Databases.** Connect to a database, browse its tables, run a query from the editor and read the rows it gives.
2. **Requests.** A file of HTTP requests, each sent with a press and its answer shown beside it.
3. **A Markdown preview.** A Markdown file shown as it reads beside its text, scrolled with it, or open as the preview alone, as the reader chooses for every Markdown file.
4. **Files in an archive.** A zip archive opened in the explorer as a folder, its files read without unpacking it.

## Plugins

1. **Plugins that draw.** A plugin lays marks, overlays and its own panels over the editor and the window, sets the label of a tab, reads the theme's colors, and changes the window's chrome.
2. **Plugins off the editor's thread.** Each plugin runs apart from the editor, and one that is slow or stuck holds up neither the keys nor the others.
3. **Plugins that write files with care.** A plugin that serves files told when the system refuses a write, and the reader asked for the rights the write needs.

## Settings

1. **Keys.** Any command's keys changed in Preferences, and whole sets of keys to choose from, the mouse's buttons and wheel bound as keys are, the keys of a command shown where the reader reaches it another way, and keys that work whatever the keyboard's layout.
2. **Settings by tree and by system.** Settings kept with a tree that stand over the reader's own, settings for one system only, a settings file that takes another's and changes some of it, settings that name others' values and the system's, and a theme for a tree or for a kind of file.
3. **Themes that reach further.** A theme sets an image behind the editor, colors for every kind of token a language names, and file icons matched to names by patterns.
4. **Settings on every machine.** The reader's settings, keys, themes and plugins carried to another machine through a file or a repository of the reader's.
5. **Nothing from the network unasked.** orior fetches nothing, no server, no update, no package, until the reader says yes, and a setting holds it off the network entirely.
6. **A plugin catalog.** Plugins others publish, found by name and installed from File, Plugins, each set on or off from a file, and each told what it may read and change before it runs.
7. **Saving where rights are needed.** A file the reader may not write is saved after the system asks for the rights.
8. **Other languages and other scripts.** The window in the reader's language, and text that runs right to left typed and drawn in its order.
9. **Packages for Linux.** orior installed from Flathub and the Snap Store as well as from its own release.
