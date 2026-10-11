# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. Under an item, each note names a complaint people make of the tools that do the same work, and what orior does so the reader never makes it. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
   - A scroll that stutters while a language server answers, a check runs or a file saves: the scroll waits on none of them.
   - An editor that draws 60 frames a second on a display of 120 or 144: the scroll is measured at the display's own rate, whatever it is.
2. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size. The page's own work for a key is 2 to 5 ms in files of 40 to 250,000 lines; what keeps the letter from the next frame at 120 a second is the frame's commit, 11 to 14 ms with the window's three layers the size of the window, which the trace of the scroll before this item reads.
   - Typing that lags while a language server, a linter or a plugin works: the letter is drawn before any of them is told of it.
   - Lag that grows with the file's length or a line's: the key's cost is measured in the longest file and on the longest line the tests hold.

## Perception

What the eye cannot see is not drawn. The view, and the stretch its motion heads into, is read and drawn first and whole; motion no eye can follow is drawn at the lowest rate that still reads as motion; and a change too small or too short to be seen is not animated. Each figure here is a measured one.

1. **What is in view, and what the motion heads into.** The editor, the explorer, the grid of rows and every list read and draw what shows and the stretch their scroll heads into, as far ahead as its speed carries it in the time a read takes, and hold what they read until it lies far behind; what lies behind the motion is let go first.
   - Motion says where the view goes next: among people watching video all around them, the view 200 ms later lay the way the head was turning in 97% of cases.
2. **The lattice drawn at the rate it is seen.** The lattice is drawn at the lowest rate at which its flow still reads as motion, and a frame that would move no point by a whole pixel is not drawn.
   - No one follows the lattice's points one by one: people follow about four moving things at once, fewer the faster they move, and see a field of moving points as one flow.
   - The edge of the lattice's order moves too slowly to see from one frame to the next.
3. **No animation below what can be seen.** A change that moves nothing by a pixel in a frame, or ends before it could be seen, is not animated: its end is drawn at once.
   - Motion the eye cannot pursue, small and unpredictable, needs fewer frames than motion it can pursue, and is given fewer.
4. **Detail as the motion allows.** While a view scrolls fast, what the motion hides from the eye, the minimap, the colors of lines past the screen and the gutter's marks, is drawn coarse or later, and drawn whole once the motion slows.
   - A change made while the eye moves fast is seldom seen: shifts of up to 1.2 degrees went unnoticed during eye movements of 66 ms or more.
5. **Motion the way the reader moves.** A pane, a list or a menu moves the way the reader's own action points, from where it was pressed toward where it goes.
   - The eye places a moving thing a little ahead along its path.
6. **The three limits of a response.** A press or a key is answered within 0.1 s; anything slower than 1 s shows it is working; anything slower than 10 s shows how far along it is and can be stopped.
   - Within 0.1 s a person feels they act on the thing itself; within 1 s their thought goes on unbroken; past 10 s they turn to something else.
   - A delay of the pointer under 50 ms goes unseen: every response to the pointer is held under it.
7. **Waits that read as short.** A bar of progress moves steadily, and faster toward its end where it can.
   - A bar that slows or stops near its end reads as a longer wait than the same time spent steadily.

## Tools

1. **Databases.** Connect to a database, browse its tables, run a query from the editor and read the rows it gives.
   - A large schema whose reading takes minutes, cannot be stopped, and is read again whole after each change: the schema is read a level at a time as it is opened, a reading can be stopped, an object changed is read again alone, and the editor never waits on it.
   - An UPDATE or a DELETE with no WHERE run against the wrong database: a connection marked as one that matters opens read-only, the server itself refusing a write, or asks before any statement that writes, as the reader sets it; and a write with no WHERE asks on every connection, with the count of rows it would change.
   - A whole script run by a key meant for one statement, the cursor on a blank line or in a comment: Run takes the statement the cursor stands in or the text selected, runs nothing where the cursor stands in no statement, and the whole file runs only from Run File.
   - A large result that freezes the client: rows come a page at a time as the grid scrolls, and a query running can be stopped.
   - An export that runs the query again, its session's settings gone: an export writes the rows the result holds and reads the rest from the same run.
   - A client that starts slowly and holds gigabytes: the database window starts with the window and holds the rows shown.
   - Passwords kept in the client's files, under a key its own source publishes: passwords go to the system's keychain and nowhere else.
   - A tunnel that fails where the terminal's ssh works, on a cipher, a key's format or a jump host: a tunnel goes through the system's ssh with the reader's own config, keys and agent, and one that drops is made again.
   - A transaction left open, a query that only read holding its locks: each statement commits on its own until the reader begins a transaction, and a connection with one open says so on the status bar until it is committed or rolled back.
   - Drivers that fail to download, ask for another Java or run out of memory: orior speaks PostgreSQL's and MySQL's protocols itself and reads SQLite through the sqlite3 Python holds, with no driver and nothing fetched.
   - Times shown in the client's zone with no offset: a time with a zone shows with its offset, in the zone of the session that read it, which the status bar names.
   - Queries and their history lost at a restart: a query is a `.sql` file of the tree, kept as every file is, and each statement run is kept in its connection's history across restarts.
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
