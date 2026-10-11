# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. Under an item, each note names a complaint people make of the tools that do the same work, and what orior does so the reader never makes it. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
   - A scroll that stutters while a language server answers, a check runs or a file saves: the scroll waits on none of them.
   - An editor that draws 60 frames a second on a display of 120 or 144: the scroll is measured at the display's own rate, whatever it is.
2. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size. The page's own work for a key is 2 to 5 ms in files of 40 to 250,000 lines; what keeps the letter from the next frame at 120 a second is the frame's commit, 11 to 14 ms with the window's three layers the size of the window, which the trace of the scroll before this item reads.
   - Typing that lags while a language server, a linter or a plugin works: the letter is drawn before any of them is told of it.
   - Lag that grows with the file's length or a line's: the key's cost is measured in the longest file and on the longest line the tests hold.
3. **The page's slow modules in Rust, vectorized.** Each module of the page a trace measures as slow, such as the tokens, the diff, the fuzzy match, the search and the minimap's reading of lines, moves into a Rust crate of orior's own, its inner loops written in the processor's vector instructions through `std::arch` alone: SSE2, and AVX2 where the machine has it, on x86-64, as MSVC and every other x86-64 target build them; NEON on aarch64, on Windows on Arm, on Linux and on the Macs of Apple's own chips; and a plain loop where neither is there. A Mac of Intel's chips takes the x86-64 path. A module moves only where the call into Rust and back costs less than the work it saves, measured on the files the tests hold.
   - Tokens, diffs and searches that hold the page's one thread on a large file: each one that costs a frame runs in Rust beside the page and hands back only its answer.
   - A speedup on one machine that is a slowdown or a wrong answer on another: each vector path is tested beside the plain loop, on x86-64 and on aarch64, and gives the same answer.

## The backend

1. **One container, every process under it.** orior's backend is one process that holds every other it starts as a child of its own: the language servers, kernels, debuggers, shells, database links, plugins and runs. On Windows they stand in a job object, which ends each with the container and counts their memory as one; on Linux, in a cgroup of the user's own; on macOS, in a process group whose footprints the container adds up. Each child is supervised: one that dies is started again alone, and one that dies too often in a short time is stopped and said, as a supervision tree of Erlang's does.
   - Language servers that hold 12 or 13 GB out of sight of the editor: the status bar's memory is the container's and every child's together, each child's beside it, and a child over the budget the reader sets is said and can be started again from there.
   - A server or a shell left running after the editor that started it has gone: every child ends with the container.
2. **One comms layer: keys and channels.** Every process reaches another only through the container. Each registers with it as it starts and is given a key, and the key names the channels it may listen and speak on, as the container grants them, a parent granting its children theirs; a message on a channel its key does not name is refused and said. The container knows each child for the process it started by the system's own word for who is at the other end of the pipe, and not by a key alone.
   - An extension host that can read everything the editor can: a process, a plugin or a server has the channels it is granted and no others.
   - Messages between processes that no one can see: the container keeps each channel's traffic, and a view lists every process, its channels and what each has said.
3. **One transfer layer.** What is large, a file's text, a statement's rows, a run's output, goes through memory the container shares with the child, named on the comms layer, and is never copied through a message.
   - A big answer that stalls every channel behind it: the comms layer carries only what is small, and the transfer layer each large thing, side by side.

## Perception

What the eye cannot see is not drawn. The view, and the stretch its motion heads into, is read and drawn first and whole; motion no eye can follow is drawn at the lowest rate that still reads as motion; and a change too small or too short to be seen is not animated. Each figure here is a measured one.

1. **What is in view, and what the motion heads into.** The editor, the explorer, the grid of rows and every list read and draw what shows and the stretch their scroll heads into, as far ahead as its speed carries it in the time a read takes, and hold what they read until it lies far behind; what lies behind the motion is let go first.
   - Motion says where the view goes next: among people watching video all around them, the view 200 ms later lay the way the head was turning in 97% of cases.
2. **No animation below what can be seen.** A change that moves nothing by a pixel in a frame, or ends before it could be seen, is not animated: its end is drawn at once.
   - Motion the eye cannot pursue, small and unpredictable, needs fewer frames than motion it can pursue, and is given fewer.
3. **Detail as the motion allows.** While a view scrolls fast, what the motion hides from the eye, the minimap, the colors of lines past the screen and the gutter's marks, is drawn coarse or later, and drawn whole once the motion slows.
   - A change made while the eye moves fast is seldom seen: shifts of up to 1.2 degrees went unnoticed during eye movements of 66 ms or more.
4. **Motion the way the reader moves.** A pane, a list or a menu moves the way the reader's own action points, from where it was pressed toward where it goes.
   - The eye places a moving thing a little ahead along its path.
5. **The three limits of a response.** A press or a key is answered within 0.1 s; anything slower than 1 s shows it is working; anything slower than 10 s shows how far along it is and can be stopped.
   - Within 0.1 s a person feels they act on the thing itself; within 1 s their thought goes on unbroken; past 10 s they turn to something else.
   - A delay of the pointer under 50 ms goes unseen: every response to the pointer is held under it.
6. **Waits that read as short.** A bar of progress moves steadily, and faster toward its end where it can.
   - A bar that slows or stops near its end reads as a longer wait than the same time spent steadily.

## Tools

1. **Requests.** A file of HTTP requests, each sent with a press and its answer shown beside it.
   - A sign-in and a cloud account to send a request: none; requests are `.http` files in the tree, plain text kept with git.
   - Secrets in a file that gets committed: each environment's private values come from a file of the reader's, which orior adds to `.gitignore` as it makes it.
   - Answers lost once the next is sent: each request keeps its last answers, and one is compared with another.
   - Requests copied from a browser or a terminal: a curl line pasted becomes a request, and Copy as curl gives one back.
   - A client that holds a gigabyte to send text: requests go out from the window's own process.
2. **A Markdown preview.** A Markdown file shown as it reads beside its text, scrolled with it, or open as the preview alone, as the reader chooses for every Markdown file.
   - A preview that jumps back to the top as the text is typed, or scrolls out of step with it: each block of the preview is tied to the line it came from, and the two scroll by those lines.
   - A preview that differs from GitHub's: tables, task lists, footnotes, alerts, math and Mermaid are drawn as GitHub draws them.
   - Images by a path relative to the file that do not show: an image's path is taken from the file's own folder.
   - A large file whose preview holds up the editor: only the blocks that changed are drawn again, and never on the editor's time.
3. **A live preview pane.** A pane beside the editor that renders the file being typed as it is typed, for every language that sets how something looks and runs nothing: Markdown, HTML and the CSS it takes, a CSS file over a page of its own, Mermaid's diagrams, LaTeX, SVG and the like. Each key is drawn in it as the text changes, and the place the cursor stands in the text is shown in the pane.
   - A page reloaded whole on each change, its scroll, its form's fields and its state lost: a stylesheet changed is swapped in place, and a page changed is patched where it changed, at the same scroll.
   - LaTeX that takes seconds to show a change: the last good page stays while the next one is made, and SyncTeX takes the reader from the text to the page and back.
   - A diagram drawn again on every key: a diagram is drawn again only when its own block changes.
4. **Files in an archive.** A zip archive opened in the explorer as a folder, its files read without unpacking it.
   - Names that come out garbled: each name is read in the encoding the archive's flag gives, UTF-8 or the old DOS page, and the reader can choose another.
   - An archive inside an archive: it opens as a folder too.
   - Files in an archive that can only be read: a file changed in an archive is saved back into it, the archive written anew beside the old one and then put in its place.
   - Jars, wheels and tarballs that are archives too: a jar, a wheel, a tar and a gzipped tar open as a zip does.

## How developers work

Each item keeps to a habit measured in developers at work.

1. **Code seen without going to it.** A definition, a usage or a caller is seen in place, over the editor at the line, with no tab opened and no way back to find; a tab opened from one is a preview until it is edited.
   - Developers spent 35% of their time on the mechanics of moving between code, and a fifth of it reading code in one fixed view.
2. **What a file is for, before it is opened.** The pointer resting on a file in the explorer shows what the file is for, from its first comment, and what in the tree uses it.
   - Without cues before opening, developers open file after file to judge each one.
3. **The task's code in one place.** The lines a task needs, from any files, are gathered side by side in one place, each still the live text of its file, and kept with the task.
   - Developers kept what they found in memory, in notes, or in tabs that closed, and went back to find it again.
4. **Notices at the reader's breaks.** A notice that needs nothing done waits for a break in the work, a save, a commit, a run's end or the keys at rest, and none takes the keys; Focus holds every notice until it ends.
   - Suggestions offered at such a break were taken up 52% of the time; those offered in the middle of a task were mostly dismissed.
   - An interruption on the screen that demands attention slows the reading of code.
5. **A change reviewed by its structure.** A change is shown as the functions it touches, each beside those it calls, and not file by file. A diff names who made the commit; nothing else shows who wrote a line unless the reader turns it on.
   - In about 40% of pull requests more than half the changed functions call each other, and the function a caller needs is often in another file.
   - A function moved shows as one move, not as lines taken out of one file and lines put into another.
6. **The debugger as simple as print.** A print is a tool of its own: a line's values are printed by a message breakpoint without the file changing, and every print statement added for a task is listed and taken out in one step before a commit.
   - Most developers never start a debugger, use only its plainest parts, and print values instead.
   - Print statements left in by mistake are a common part of commits.
7. **Menus that stay where they are, and their keys in sight.** A menu never orders itself by use: where an item stands is learned and kept. Holding Ctrl or Alt shows each button's keys over it.
   - How fast an item is chosen depends on its place staying the same.
   - Many people never learn an item's keys unless the keys are shown where they already look.

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
