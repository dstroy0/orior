# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **Scrolling at the display's rate.** A fast scroll holds every frame a display of 120 a second draws, and not only every other one. The frame's handover to the compositor, `Commit` and `LayerTreeHost::DoUpdateLayers` in a trace, takes about 10 ms of the 8.3 a frame has. Two measurements say what is in it: a trace of the scroll with the compositor's own categories (`cc`, `viz`, `gpu`, `disabled-by-default-cc.debug`), which splits it into the canvas flush, the layers' update, tiles and uploads; and the same scroll with the minimap not drawing, which says whether the map's canvas is the cost.
2. **A key on the screen in the next frame.** A letter typed shows in the frame after the key, measured from the key's event to the frame that draws it, in a file of any size. The page's own work for a key is 2 to 5 ms in files of 40 to 250,000 lines; what keeps the letter from the next frame at 120 a second is the frame's commit, 11 to 14 ms with the window's three layers the size of the window, which the trace of the scroll before this item reads.

## Layout

1. **The system's title bar.** The window drawn with the system's own title bar where the reader asks for it.

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
