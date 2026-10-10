# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Start under a second.** The window shows with its page drawn and takes keys within a second of the launch.
2. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one.

## Code intelligence

1. **Symbols of the whole tree.** Search Everywhere and Go to Symbol find a declaration in any file of the tree, not only in the file open, from an index the language servers and the outline readers keep up to date as files change.
2. **Refactorings of orior's own.** Extract a variable, a constant or a function from a selection; inline one again; change a function's parameters across every call; move a declaration to another file. Each is one step that undo takes back, and each works where the language server offers none.
3. **Inspections.** Checks of orior's own over the tree's languages, run as files change, each listed in Problems beside the servers' diagnostics with a quick fix where one applies.
4. **Structural search and replace.** Find code by its shape, with placeholders for names, expressions and statements, and replace each match from a template that uses what the placeholders took.

## Version control

1. **Branches.** Every local and remote branch in a list, to create, switch to, rename and delete, and to merge or rebase onto the branch open.
2. **Conflicts.** A merge window with three panes, theirs, the result and yours, each conflict taken from either side or written by hand, a press marking the file resolved.
3. **Part of a file in a commit.** The Commit window takes single changes, or single lines, of a file, and leaves the rest for a later commit.
4. **Compare.** Two revisions of a file side by side, or a file beside another, or beside the clipboard.
5. **Line history.** Beside each line, the commit that last changed it, a press opening that commit's changes.
6. **Stash and cherry-pick.** Changes put aside and brought back, and a commit of another branch applied to the one open.

## Testing

1. **A test runner.** The tree's tests in a tree of their own, by file and by test, each marked passed, failed or skipped; a failure opens where it failed; failed tests run again with one press.
2. **Tests from the gutter.** A mark beside each test in the editor runs it, or debugs it.
3. **Coverage.** A run that records which lines ran, shown in the gutter and as a share by file.

## Debugging

1. **Breakpoints with conditions.** A breakpoint that stops only when an expression holds or after a count of hits, and one that writes a message to the console in place of stopping.
2. **Stop on exceptions.** The debugger stops where an exception is thrown, or only where nothing catches it.
3. **Attach.** Debug a program already running, chosen from a list of processes.
4. **Memory and machine code.** The bytes at an address, and the instructions about the line stopped at.

## Layout

1. **Groups of tabs.** Each side of a split holds tabs of its own, any file in either.
2. **Tool windows anywhere.** A tool window dragged to any side of the window, or out into a window of its own.
3. **More than one window.** A window for each tree open at once, each with its own tabs and terminals.
4. **Terminals side by side.** More than one shell open at a time, each in a tab of the terminal panel, or two side by side.

## Running and measuring

1. **A profiler.** A run measured as it goes, its time and its memory by function, shown as a flame graph a press on which opens the function.
2. **Runs on another machine.** A tree opened over SSH, or in a container, with the editor, the terminal, the jobs and the debugger working there.

## Tools

1. **Databases.** Connect to a database, browse its tables, run a query from the editor and read the rows it gives.
2. **Requests.** A file of HTTP requests, each sent with a press and its answer shown beside it.

## Settings

1. **Keys.** Any command's keys changed in Preferences, and whole sets of keys to choose from.
2. **A plugin catalog.** Plugins others publish, found by name and installed from File, Plugins.
