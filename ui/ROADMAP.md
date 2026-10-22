# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. Under an item, each note names a complaint people make of the tools that do the same work, and what orior does so the reader never makes it. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **A key in the next frame, whatever is open beside it.** A preview, a Vim mode or a slow laptop does not delay the letter typed.
   - A preview beside the editor delays each letter, the two sharing a process, and a Vim mode adds a tenth of a second to every key ([typst-preview #245][45], [VSCodeVim #2021][46]). orior draws previews and plugins off the editor's thread, and its Vim keys are its own.
   - The editor is slow to type, scroll and find on a low-powered laptop with extensions off ([Lightrun][47]).
   - Completion and analysis can come late or get in the way of editing. Typing and the caret's moves answer at once whether analysis has finished or not, and the test of it types and moves through a file while analysis is still running.

## Perception

1. **No one part can freeze the window.** A plugin's stored history or a list's source read on each key never holds up a frame.
   - The window freezes and a core is pinned by one extension's oversized history, and a list waits 1 to 2 s on each key ([Helge Klein][65]).
2. **What is told sits at the edge of sight.** State shows quietly on the status bar and the gutter, and comes to the middle of the screen only when the reader must act.
   - Calm design keeps most of what is told at the edge of attention, asks the least attention it can, and works even when it fails ([calm technology][66], [calmtech.com][67], [Fuzzy Math][68]).
3. **A light theme as well as a dark.** orior's look comes in a light ground for bright rooms and small text, beside its dark one, and can follow the light of the system.
   - Light text grounds read better for small text in good light, readers do not notice they do better, and a dark ground is easier in dim light ([NN/G][69], [TIDS 2025][70], [Gadget Hacks][71]).
4. **The window says what made it slow.** A key, a frame or a command that runs past its budget is named on the status bar with the part that held it, a plugin, a language server or the page's own work, and the memory and processor each part takes are one press away.
   - A slow start, indexing, completion that drags, costly language servers and plugins that eat memory, with no way to see what takes the time; a feature that saves ten seconds later still angers where it adds a delay to every key. orior holds each key to its frame and names whatever runs past its budget.
   - Readers struggle to find which part causes a lag. Time and processor are put to each language server, plugin, index and piece of work behind, and the test of it slows each in turn and checks that the window names it.
   - "Why is indexing so slow" and "why is completion so slow" show the cost of the work and nothing of its cause.
5. **Everything working behind, in sight.** The status bar lists each piece of work running behind the editor, what it is doing, what it costs and how far it has come, and any of it can be stopped; what the compiler decided and what an optimization cost are read beside the code they concern.
   - Readers want transparency: background work, the compiler's decisions and the cost of an optimization kept hidden make them hate an IDE.
   - An IDE is to be deeply observable: the program's state, its data flow, the build's decisions, what each thing costs and what each change did, each open to the reader.
   - "Why does it keep reloading" shows behavior that breaks what the reader expects. Each thing orior does again on its own says why, where it does it.
   - IntelliJ's Maven auto-import, a question of some 47,000 views, shows three things at once: a behavior made automatic, its cost to the machine, and a way back not plain enough to keep the question from being asked ([JetBrains][484]); the switch was then taken away ([JetBrains][485]), and readers report it on after every box was cleared ([JetBrains][486]). Work orior does on its own, as reading a build file again, shows its cost where it runs and is turned off from there.

## Notices and messages

1. **A notice never takes the keys.** A notice stands beside the work and waits; the field the reader types in keeps every letter.
   - A notice takes the keys from the field one is typing in and the letters are lost ([Devolutions][72]).
   - Notices pop up in the corner, pile over the work and take clicks, and a notice of weight is missed among many ([IntelliJ][73], [Asana forum][74], [libadwaita #242][75], [Genesys][76]). orior shows one notice at a time, keeps the rest in a list on the status bar, and never covers the line the caret is on.
   - A setting for where notices show does nothing ([Cursor forum][77]). Each setting of orior's is tested to do what it says.
2. **A plain No.** Every question orior asks can be answered No, and No is kept.
   - Prompts come with no plain No, asking to install tools the reader did not ask for ([Joel Dare][78]).
3. **Errors in the reader's words, with the way out.** Each error orior shows says what happened and what to do next, in plain words, and neither praises nor blames.
   - Anxiety peaks before error messages; messages are written in the programmer's words; an interface should neither praise nor condemn, and be plain, predictable and in the reader's hands ([Ewha thesis][79], [DTUI ch. 12][80], [UX Collective][81]).
   - Frustration costs a third to a half of the time spent at a computer ([Lazar et al.][82]).
4. **A quiet mode, and each kind of help set apart.** One key quiets every hint, suggestion and notice for work without a break; each kind of help has its own switch and its own threshold, and the keys do the same whatever suggestions stand.
   - Readers want suggestions on demand, a switch for each feature where one switch turns all of them, thresholds of their own for hints and warnings, and keys that act the same whichever suggestions are shown.
   - Readers want flow: small interruptions, again and again, break concentration.
   - Help should take work away and not ask for attention.
   - Some readers want suggestions only when they ask. Suggestions that come on their own, those that follow from the context, and those asked for each have a setting of their own, and the test of it turns each on and off apart from the others.
   - "How do I disable" shows a default readers do not want, as suggestions that come on their own; "How do I stop" shows an interruption or a surprise, again and again.

## Tools

1. **Databases at the speed of their own tools.** A query runs as fast as in the vendor's tool, its time is the server's, and the result shows whole.
   - Queries quick in the vendor's tool are slow in the editor's, the task view says 300 s where the history says 0.8 s, columns of one name keep a result from showing, the schema is stale until refreshed by hand, and one does not know whether a query has run ([Oracle forum][83], [vscode-database-client][84], [DataGrip][85]). orior names columns of one name apart, reads the schema again when it changes, and marks each query's state.
   - A server of 600 databases makes the tool very slow ([DataGrip][86]). orior reads a database's tables only when it is opened.
2. **A Markdown preview that scrolls with its text.** The preview keeps the same line in view as the text, past images and headings, at any speed, and shows local images from anywhere.
   - The preview and text scroll apart where heights change, the preview jumps back when scrolled fast, and local images outside the folder are not shown ([vscode #64685][87], [Cursor forum][88], [DEV][89]).
3. **Diagrams drawn as typed.** A Mermaid diagram half typed keeps its last good drawing, a node named with a kept word is drawn, and no browser starts.
   - A node named end or subgraph fails, a diagram typed half way stalls the preview, and each render starts a browser ([asciidoctor-intellij][90]).
4. **A live page that reloads as typed.** The live preview reloads as the text changes, for a file opened alone and a page with no head or body.
   - The page reloads only on save, not for a file opened alone or a page without a head or body, and the browser does not open ([Live Server][91], [kitemetric][92]).
5. **Requests at once, collections whole.** The request tool is open in the first frame and runs a whole collection.
   - The request tool takes minutes to start, and running a collection is limited ([DEV][93]).

## How developers work

1. **A peek only when asked.** A peek opens on its key alone, a press inside it goes where pressed, its text can be edited in place, and closing it leaves the editor as it was.
   - A peek opens while typing, a press in it goes to the top of the file, its definition cannot be edited, and closing it takes the toolbar with it ([SmartBear][94], [Cursor forum][95], [Rider][96], [VisualGDB][97]).
2. **Keys found from either end.** Typing a key in the command palette shows what it does, and the menu bar can always be brought back from its place.
   - A shortcut can be found from its command but not a command from its keys, and the menu bar disappears with no way back known ([Figma forum][98], [PlatformIO][99]).
3. **Tracepoints as cheap as a print.** A tracepoint hit often does not slow the program, and counts calls and times a method.
   - A tracepoint hit often slows the program, and counting calls or timing a method is easy with a print and not with a tracepoint ([DZone][100], [Microsoft Learn][101]).
4. **Suggestions only when the reader is between thoughts.** Unasked suggestions wait for a pause, and come less often as they are passed over.
   - Unasked suggestions are felt as interruptions and load the mind; timing them by what the reader is doing and how often they are taken keeps the flow ([arXiv 2504.06808][102], [arXiv 2511.18842][103], [arXiv 2511.18849][104]).
   - Interruptions not tied to the task break attention, and switching tasks loads the mind ([arXiv 1805.05504][105]).
5. **A task taken up where it was left.** After a break, the window shows the last edit, the place the caret was, and the files the task touched, and nothing about it has moved.
   - The context as it was left aids the return and changing it slows it, and a cue to the last act or the next one cuts the lag after long breaks ([Hodgetts and Jones][106], [Trafton][107], [Hodgetts][108], [PMC][109]).
   - Gathering the methods a task touched from what the reader did lets the reader take the task up again ([Parnin and Görg][110]).
6. **A trail of where the reader has been.** Each jump through the code is kept as a trail the reader can see and walk back along.
   - Readers lose their way through missing context of how they got somewhere, thrashing between views and side tasks ([de Alwis and Murphy][111], [Relo][112], [Code Bubbles][113]).
   - Code laid out in space costs a different time in navigation than tabs ([arXiv 2304.09301][114]).
7. **What the reader needs, inside the window.** A library's documentation, the build's errors and a tool's output are read in the window, beside the code they concern.
   - The mind is eased when the IDE holds what the reader needs and no switch is made; long output not of the tree adds load ([arXiv 2605.23135][115], [arXiv 2507.03156][116], [Code Compass][117]).
   - Building a mental model of the code and settling into a new project are what frustrate developers most ([arXiv 1601.05330][118], [arXiv 1703.04993][119], [arXiv 1701.02952][120]). orior's file cards, outline and gathered lines are its way into a tree new to the reader.
8. **A change tried without fear.** Any edit, refactoring or command can be undone the same way everywhere, and a refactoring shows its result before it is made.
   - Knowing an error can be undone lets the reader explore, but undo that differs from place to place is not trusted ([Shneiderman][121], [Dix][122], [Dix][123], [Glimpse][124], [CHI 2023][125]).
   - Readers copy code and leave the original alone for fear of breaking the system ([ICSE 2012][126]).
9. **Values seen as code is written.** A function's values show beside its lines as they are edited, run on the inputs the reader gives.
   - Liveness changes how developers move through code, and running code as it is written checks it ([Kubelka et al.][127], [arXiv 1807.08578][128], [arXiv 2306.09541][129], [SEEDE][130]).
10. **A review without bias.** A change under review shows its parts by what they change, with the writer's name out of sight.
    - Code review aids can take bias out of review ([arXiv 2407.01407][131]).
11. **A function understood in one place.** The callers, the callees and the types a function reaches stand beside it in a view of their own, which opens no tab and leaves none behind.
    - Tabs that pile up, names whose origin is not plain, and twenty files opened to understand one function. orior's view of a function follows its calls in place and closes clean.
12. **Data traced from input to output.** A value is followed from where it enters the program to every place it reaches, and an edit shows what it changes elsewhere: the calls, the tests and the files it reaches.
    - Readers want the IDE to trace data from input to output, explain chains of calls, and show an edit's consequences elsewhere.
13. **Every automated change says why, and goes back whole.** A quick fix, a refactoring, a format or a completion shows the exact change before it is made, the rule that made it and the checks run on it; it never reaches past what was asked, and one undo takes it back.
    - Automated changes widen a small edit into a large rewrite, and nothing says what rule made them or whether behavior changed.
    - Readers want trust: a refactoring or a quick fix that silently breaks something loses it.
    - Readers want confidence: an IDE that calls a change safe without showing the evidence earns none. orior shows the checks a change passed beside it.
    - Correctness comes before cleverness: an automated change keeps the behavior intended, and checking that it does is made easy.
    - "How do I undo an automatic refactoring" shows a change made unasked or hard to take back.
14. **A key says what it does here.** Where one key does different things in different places, the key's hint says what it will do where the caret stands, before it is pressed.
    - Readers want predictability: the same key doing different things in different contexts makes them hate an IDE.

## Plugins

1. **No plugin can hold up another.** Each plugin runs on its own, and one that hangs or grows is stopped and named while the rest go on.
   - One extension's work blocks every other and the editor, an extension hangs the host on each start, a panel's re-layout stalls the window, and a panel's memory grows to gigabytes ([vscode wiki][132], [Cursor forum][133]).

## Settings

1. **Saving with rights that works on every package.** A save that needs rights asks once and saves, from any package of orior's.
   - The elevated save asks for the password then fails, asks again and saves nothing, or the dialog never opens ([Arch forum][134], [Arch bugs][135], [NixOS][136]).
2. **Keys on every layout.** Keys follow the layout in use now, the system's remaps, layouts such as Neo, and any key of a national layout.
   - Shortcuts follow the layout the editor started with, remaps are not followed, Neo does not work, a national key cannot be bound, and another program takes a key ([vscode #17521][137], [vscode wiki][138], [GitLab][139]). orior says which program holds a key it cannot have.
3. **Settings carried, the machine's own kept.** What is carried merges with what is this machine's, and a setting holding a local path stays on its machine.
   - Replacing local settings with those carried wipes the machine's own, a local path breaks the others, and merges fail with errors ([VS Code][140], [SonarLint][141], [code-settings-sync #396][142]).
4. **No plugin's network unasked.** A plugin reaches the network only when the reader allows it, and every request is listed.
   - The editor asks for its experiments with experiments off, extensions send their own data whatever the setting, and a request is made as the editor opens ([vscode PR #104293][143], [code-server #5821][144]).
5. **A catalog to trust.** A plugin is checked before it is listed, its rights are shown, it updates only when the reader says, and one reported bad is gone at once.
   - A publisher is verified by owning a cheap domain, a typosquat was verified and sent the machine's details away, few publishers are verified, a malicious extension stays listed for weeks, and a taken-over extension updates everyone ([BleepingComputer][145], [Cybersecurity News][146], [Koi][147]).
6. **Text in any direction.** Right-to-left text shows its words in order with the caret where it is, and punctuation on its own side.
   - Right-to-left text shows words in the wrong order and the caret in the wrong place, and a stylesheet that forces left to right breaks Hebrew and Arabic ([Geany][148], [GIMP #416][149]).
7. **One color for one meaning, in every language.** A theme colors a variable the same in every language, by its meaning.
   - The same variable takes five colors in five languages, and extensions lay their own colors over the theme's ([VS Code][150], [OCaml][151], [Eva Theme][152]).
8. **A tree's settings that add to the reader's.** A tree's setting changes only what it names, and the window says which settings come from the tree.
   - A tree's setting replaces the reader's whole, and a setting committed with a tree silently changes each developer's editor ([Sourcegraph][153], [nomad-FAIR][154]).
   - Settings that touch the whole window are ignored in a window of several trees, and which layer wins is not plain ([VS Code][155], [openclaw][156]).
   - "How do I configure formatting for one project" can show controls that are not plain, or a setting rightly the reader's own. A tree's setting is set where the tree is open, and the window says which settings are the tree's.
9. **Packages for Linux that reach the system.** Every package of orior's reaches the system's shell, toolchains and container socket, with its terminal and debugger at work.
   - The sandboxed package cannot reach the shell, SDKs or Docker, and its terminal, containers and debugger fail ([Flathub #370][157], [DEV][158], [vscode-cpptools #1550][159]).
10. **An update that changes nothing of the reader's.** An update keeps every key, pane, setting and plugin as the reader left them, and nothing changes without one.
    - Updates change keys, the layout and the sidebar without asking, the window changes from the server with no update, and an update wipes plugins, settings, keys and history ([Cursor forum][160], [Cursor forum][161], [Cursor forum][162], [Antigravity][163]).
    - An update took away the reader's toolbars and settings ([IntelliJ][164]).
    - Each major release brings regressions, and small annoyances pile on ([IntelliJ][165]). Every interaction of orior's is walked by the harness before a release.
    - Readers want control: an IDE that changes its behavior without their leave is one they come to hate.
    - "How do I get back the old layout" shows a way of working lost to a change. Every change to the window's layout can be put back, from where it was made.
11. **orior's files, read and carried as files.** orior keeps every setting and state in plain files in one folder, the same on every system, that any backup reads.
    - Settings, state and plugins sit in different folders on each system, state is kept in a database backups cannot carry, and importing fails on a missing database ([VS Code][166], [Cursor forum][167], [Cursor forum][168], [codegenes][169]).

## Getting set up to work

1. **A tree that builds the moment it opens.** orior reads what the tree is built with, finds or offers each tool and the version the tree names, and builds with no settings written by hand.
   - More time goes to the environment than to code, and a new machine takes days or weeks from unclear documents ([naildrivin5][170], [Cloudomation][171], [MDN][172]).
   - Tool versions differ between machines, and a new hire spends a day getting the right Python, Ruby or Go ([moon][173], [Thoughtworks][174]). orior reads the versions a tree pins and warns where the machine's differ.
   - In C and C++ the build system and the toolchain are not tied together ([CMake Discourse][175]). orior shows which compiler each build uses, beside the build.
   - On Windows Python is left off PATH, and a package install fills the screen with compiler errors ([Medium][176]). orior finds each installed Python wherever it is, and says what a failed install lacks.
2. **One Python for the code help and the terminal.** Every terminal runs the interpreter the code help reads, and every environment of the tree is found.
   - The code help and the terminal use different interpreters, the tree points at the system Python, and an environment is missing from the picker ([SystemCraft][181], [DEV][182], [vscode-python #24573][183], [Arch forum][184], [Lightrun][185]).
3. **PATH read again without a restart.** A tool put on PATH is seen at once by orior, its terminals and its runs.
   - A tool put on PATH is not seen until the IDE restarts, at times only after a reboot, and a tool must be named by its exact file name ([IntelliJ][186], [IntelliJ][187], [IntelliJ][188]).
4. **Language support with no wait.** The languages orior knows are with it before a file opens, and the few plugins it needs do not slow it.
   - Language support is downloaded only once a file opens, and a good setup needs many plugins that slow it ([G2][189], [G2][190]).
5. **The setting that caused a failure, named.** Where a build or a run fails on a setting, orior names the setting, the file that set it and what else sets it: an SDK, a variable of the environment, a compiler's version or a plugin.
   - Trees work on one machine and not another, with SDKs, variables of the environment and compiler versions at odds and settings spread across many files, and no word of which setting caused the failure.

## Code intelligence

1. **The tree's own symbols first.** Symbols of the tree stand above those of its dependencies, one slow server does not hold up the list, and the list fills before a file is opened.
   - One slow server holds up the list, dependencies flood one's own symbols, and the list is empty until a file is opened ([vscode #105870][191], [Go #37236][192], [Go #66523][193]).
2. **Refactorings that do not fail.** Rename, Extract Function and Change Signature work where a server cannot, and say why where they cannot.
   - Rename says it cannot rename, fails in a file with asserts, and Extract Method takes the wrong lines ([python-language-server #1393][194], [#2057][195], [Unity][196]).
   - Extract Function refuses a fragment with more than one output, misses parameters so the result does not compile, does not find calls repeated with other values, and shows nothing at the top level ([IntelliJ][197], [ReSharper][198], [Industrial Logic][199], [PyCharm][200]). orior gives back several outputs together and finds the same lines elsewhere.
   - Change Signature fails where a function carries a contract, and an overload kept for old calls can make calls ambiguous ([IntelliJ][201]). orior checks every call still names one function before it changes them.
3. **Problems of the whole tree, kept true.** Problems show for every file of the tree, and those of a file closed or mended go at once.
   - Problems show only for open files, the whole tree's are paid in one extension, and a closed file's problems stay ([DEVSENSE][202], [ruff][203]).
4. **No false red.** What orior marks as an error is one the build gives, and no cache to clear stands between them.
   - False errors stay for weeks until a restart, unused checks flag what is used, the build server finds errors the IDE does not, syntax errors cannot be asked for alone, and suppressions add their own noise ([IntelliJ][204], [IntelliJ][205], [IntelliJ][206], [GitLab][207]).
   - Red marks over code that builds, mended by clearing caches only for a while and at a quarter of an hour each time ([IntelliJ][208], [IntelliJ][209], [Rider][210]).
5. **Every caller found.** The call hierarchy holds every call a text search finds, through function pointers too.
   - The list of callers is empty where a text search finds many, and calls through pointers are missing ([NXP][211], [Polyspace][212]).
   - Finding references is unreliable ([r/Unity3D][213]). orior adds the text's own matches to the server's, marked apart.
6. **Types that read as hints.** Types written in the text are plainly not code, keep line heights even, and go at once when turned off.
   - Hints are hard to tell from code, make heights uneven, distract, do not clear when turned off, and the toggle was taken away ([users.rust-lang.org][214], [users.rust-lang.org][215], [users.rust-lang.org][216], [rust-analyzer #3924][217]).
7. **One mark for one mistake.** A mistake shows once, plainly, and many warnings do not clutter the lines.
   - The squiggle is easy to miss, inline messages clutter a file of many warnings, one mistake gives several marks, and marks for every hint are too much ([Error Lens #206][218], [yzhums][219]).
   - Debugging for those new to it valued finding where an error is most ([arXiv 2509.21067][220]).
8. **Colors that hold while typing.** A line half typed keeps the colors of the file around it.
   - A parser's error nodes paint half the file red as one types, and a grammar's bug miscolors a whole language ([Nord #269][221], [emacs-devel][222], [jez.io][223]).
9. **Structural search for every language.** A pattern can say read or write, starts from the code selected, and works in every language orior colors.
   - A pattern cannot say read or write, templates are hard to start from, and few languages are searched ([IntelliJ][224], [IntelliJ][225]).
10. **A hover the pointer can reach.** A hover stays while the pointer moves to it and its links can be pressed.
    - The hover disappears before the pointer reaches it ([Cursor forum][226], [CodeMirror][227]).
11. **Regions in every language.** Regions show in Structure and fold in every language, whatever plugin is installed.
    - Regions show in some languages and not others, and an extension breaks their folding ([DEV][228], [vscode #12146][229], [Go #67398][230], [DEVSENSE][231]).
12. **Docstrings in the tree's form.** A docstring orior writes takes the form the tree already uses, writes types beyond the simple ones right, and leaves out what the tree's renderer adds.
    - Generated docstrings are left unformatted, types beyond the simple ones are written wrong, and types in the docstring break a renderer that adds them ([SiLA2 #6][232], [mkdocstrings #57][233], [sphinx-autodoc-typehints #281][234]).
13. **Mistakes told as they are typed.** What the compiler would say shows as the line is typed, before a build.
    - Mistakes are told only when the compiler runs ([cplusplus.com][235], [SD Times][236]).
14. **The tree understood once, and read again only where it changed.** What orior knows of the whole tree is kept between starts, and a change is read again in the files it touches alone: navigation and problems stay true while no file is analyzed twice for nothing.
15. **SonarQube's analysis of every language the tree is written in.** The SonarQube plugin analyses each language orior's own tree holds, in the order orior uses them: C, C++ and CUDA through the CFamily analyzer, fetched as the extension fetches it; Rust through the community Rust plugin on the server; shell through the community ShellCheck plugin on the server; and R, Octave and PowerShell, for which SonarQube has no analyzer, through their own checkers, lintr, the Octave parser and PSScriptAnalyzer, their findings shown beside SonarQube's in the findings panel.
   - The language server runs JavaScript, TypeScript, Python, Java, C#, Go, PHP, HTML, XML, secrets and infrastructure files today. C and C++ wait on the CFamily analyzer, which SonarQube for IDE downloads on the first C or C++ file and verifies against the signature it ships; Rust and shell have analyzers only for the server, so their findings come from a scan of the tree, as the duplications do; R, Octave and PowerShell have none.
    - Knowing the whole tree is of great use, and it can take much of the machine. The test of it checks that a definition and a problem are found as before while the work done after a change is measured and kept to the files the change reaches.

## Languages and formatting

1. **One formatter, the one the tree uses.** Format on save runs the tree's formatter and no other, formats only the lines changed when asked, and keeps the tree's line endings.
   - Format on save runs a formatter other than the one expected, formatting only lines changed breaks, and line-ending conversion makes every save a change ([vscode #146910][237], [prettier-vscode #638][238], [israynotarray][239]).
   - A linter and a formatter rewrite what the other wrote, a loop on every save ([typescript-eslint][240], [Josh Goldberg][241], [Lightrun][242], [devplaybook][243]). orior runs them in one order once, and names a rule that undoes another.
   - Black keeps a collection one item a line by a trailing comma, and equal code then formats differently, and another formatter collapses what Black keeps open ([black #2135][244], [black #1941][245], [ruff #1200][246]).
   - "Disable ALL Auto Formatting on Save" gathered 26 answers: readers tried settings, met formatting a formatter does of its own, and in the end bound Save to Save Without Formatting ([CodeGenes][482]). orior's one switch for formatting on save stops every formatter's, its own and each plugin's.
   - A reader wants formatting for most files and none for Markdown, a question of more than 11,000 views ([Classmethod][483]). orior takes a language out of formatting in one place, beside the switch for all.
   - "How do I stop reformatting my code" shows an interruption that comes back on each save.
   - IntelliJ's question of how to turn formatting off has some 55,000 views over more than ten years, its answers giving several ways to turn it off or narrow it ([JetBrains][487], [JetBrains][488]).
2. **Every environment of the tree found.** A new environment, one in a subfolder, and conda's are found as they appear.
   - A new environment is not found until refreshed, one outside the paths searched is missed, conda's are missing off the PATH, and a conda environment in the tree errors ([VS Code][247], [vscode-python #24640][248], [#12164][249]).
   - Too many tools do one job, and the active environment must be kept in mind ([Chris Warrick][250], [Pinecone][251], [KDnuggets][252]). orior shows the environment in use on the status bar.
3. **Checkers that keep up.** A linter's mark goes the moment the error is mended, and a type checker runs on what changed and leaves the rest.
   - Messages update seconds after the save, and mypy on every save takes much of the machine ([Lightrun][253], [vscode-mypy][254]).
4. **JSON and YAML by the right schema.** A file is checked by the schema it names, and YAML too, as the schema's own tool checks it.
   - A schema is matched to the wrong file, a valid file fails, a $schema is refused, and YAML is not checked without an extension ([vscode-yaml #397][255], [F5][256]).
5. **Build files read whole.** Every source gets the settings of its target, a compiler the build tool does not know is read too, and the commands are read again when the environment changes.
   - An unknown compiler makes the tool ignore every setting, a source outside a target gets none, commands go stale, and a file whose includes cannot be found is not checked ([vscode-cmake-tools #1096][257], [fvdb][258], [VS Code][259]).
   - CMake's generated files call back into cmake, there is no help with outside dependencies, and a release changes behavior ([annoying build systems][260], [cmake-developers][261], [curl][262]). orior names the CMake version a tree needs.
6. **Python held in little memory.** Code help for a large package stays under a set memory, typing in a long file does not pin a core, and definitions answer at once.
   - The server passes 2 GB and must be restarted, a file over 1,000 lines pins a core, and a definition takes 60 s ([Genesys][263], [vscode-python #25357][264], [NVIDIA][265], [Cursor forum][266]).
7. **Compiler errors cut to the line that matters.** A long error is folded to its cause, with the rest a press away.
   - Compiler errors confuse, and a template error is eighty lines to search; readers take a message that says how to mend it ([NSF study][270], [gcc-help][271], [D #7768][272], [GameDev.net][273]).
8. **JavaScript with nothing to choose first.** A JavaScript tree runs and is checked with what orior has, and the choices of build tools wait until wanted.
   - Build tools, boilerplate and linters must be chosen before any code, the wiring is hard to follow, and dependencies explode ([JavaScript fatigue][274], [Tools for Radicals][275], [r/programming][276], [Vivian Voss][277]).

## Explorer and files

1. **The explorer's find shows every match.** Typing in the explorer narrows the tree to every match, by name or by path.
   - The find shows some matches and cannot narrow to a path ([spiceai][278], [Lightrun][279]).
2. **Files dragged as the system drags them.** Ctrl copies on every system, a file dragged out arrives as a file, and files dropped into a remote tree arrive.
   - Ctrl moves where it should copy on Linux, a dragged file arrives as a link, and drops into a remote tree fail ([CachyOS][280], [XYplorer][281]).
3. **One switch says what is left out.** One list says what the explorer and search leave out and why, and a result left out names the rule that left it.
   - Files git ignores are left out with that turned off, switches overlap, and nested ignore files are not followed ([vscode #188273][282], [vscode wiki][283], [Cursor forum][284]).
   - Which list hides a result is the most common reason one is missing, patterns set for the tree stop working, and too many patterns pass the command-line length ([vscode wiki][285], [Cursor forum][286], [bobbyhadz][287]).
4. **Breadcrumbs that keep the editor still.** The bar holds its line between tabs, and a deep path scrolls.
   - The bar moves the editor between tabs, and a deep path cannot be scrolled ([DEV][288], [IntelliJ][289], [Cursor forum][290]).
5. **A file in an archive saved back.** A file changed inside an archive is written back into it.
   - A file changed inside an archive cannot be saved back ([Zip Editor][291], [IntelliJ][292]).

## Editing

1. **Macros that record everything.** A macro records a find with its key and every press on a button, menu, popup or dialog.
   - A macro does not record a find with its key nor presses on buttons, menus or dialogs ([Sublime forum][293], [IntelliJ][294]).
2. **Undo of a block, kept with the file.** Undo can be of the lines selected, edits that change nothing do not fill it, and it is kept when the window closes.
   - A redo past an undo loses states, edits that do nothing fill the history, undo is of the whole file, and the history goes with the session ([undo-tree][295], [gitlab-vscode-extension #1824][296], [DEV][297]).
3. **Vim's keys as fast as Vim.** Every Vim key answers in the frame, none lost or doubled while completion shows, and none in conflict with orior's own.
   - Every key is slower, keys are lost or doubled while completion shows, motions are unusable, and Vim's keys conflict with the editor's ([VSCodeVim #8603][298], [Cursor forum][299]).
   - Vim keys inside an IDE fall short ([mwop.net][300], [DEV][301]).
4. **Folds that stay as left.** Every fold opens again as it was left, and background work never opens one.
   - Folds are not as left after the editor opens again, and are opened again as background work ends ([PlatformIO][302], [Rider][303]).
5. **Colors from every form.** Every color written as hex, rgb, rgba or hsl gets its swatch, whatever plugin colors the language.
   - Hex and rgba get no swatch, and an extension's colors drop the built-in ones ([Cursor forum][304], [vscode #182214][305], [#182534][306]).
6. **Space past the line only when asked.** A press past a line's end puts the caret at the end; columns past it come only on their own key.
   - A press past the end inserts spaces, virtual space comes on unasked, and padding lines is clumsy ([FastReport][307], [Notepad++][308], [Sublime forum][309]).
7. **Indentation as set, and told.** The status bar says where a file's indentation comes from, and a setting the reader makes wins over a guess.
   - The guess overrides the settings, an extension changes tab size, a tree's settings override the reader's, and tab size does not behave as set ([vscode #41542][310], [#172171][311], [IntelliJ][312]).
8. **Jump labels where the reader looks.** Labels stand on any character sought, on empty lines, on tabs and on the tree, drawn over the text and never written into it.
   - Labels land on first letters only and in the wrong places, empty lines get none, labels written into the text break it, and tabs and the tree cannot be reached ([vscode-neovim][313], [VSCodeVim #2388][314], [jumpy2][315]).
9. **Characters not drawn, marked with care.** Zero-width characters are shown, and the parts of an emoji are not marked.
   - Zero-width characters break code unseen, and marking all outside ASCII marks the parts of an emoji flag ([israynotarray][316], [zed #19846][317], [vscode-gremlins][318]).
10. **Closing brackets that read the line.** A closing bracket or quote is added only where one is missing, typed over where it stands, and one switch turns it off for every language.
    - Brackets and quotes are inserted blind to the line, quotes double in markup, the setting does not stop it for every language, and the completion list takes the Enter meant for a new line ([KDE #308614][319], [NetBeans #262685][320], [NetBeans #205881][321], [IntelliJ][322], [Eclipse][323], [SmartBear][324]). orior's list takes Tab, and Enter only when the reader has moved in the list.
11. **Completion that never takes the edit.** A suggestion is plainly a suggestion until it is taken; Enter and Tab do what they always do unless the reader has moved into the list; one Escape puts it away; it puts in the word asked for and not a block; and how eagerly it comes is the reader's to set.
    - Suggestions come too eagerly, take the keys, change what Enter or Tab does, or put in a large block where one word was wanted, and the reader loses hold of the edit.
    - A reader asks how to turn off every suggestion that comes on its own, pressing Escape before each new line and lost among settings of like names ([Stack Overflow][480]). orior turns every suggestion that comes on its own off with one switch, named as the reader would name it.
    - A reader asks how to stop the completion list, which comes up as they type and covers much of the code, and the settings they tried did not stop it ([freeCodeCamp][481]). orior's list never covers the line typed, and the switch that stops it is found by the words of the question.

## Version control

1. **Branches that keep their changes.** Switching branches keeps each branch's changes not committed with it, with no stash by hand.
   - A switch is refused until the tree is clean, and changes are not kept with their branch ([bobbyhadz][325], [IntelliJ][326]).
2. **A three-way merge with its base.** A conflict shows the base beside both sides, and each action shows the result before it is taken.
   - The merge editor has no base pane, Accept Both may not build, and the actions give no sign of the result ([VS Code][327], [DEV][328], [Kaleidoscope][329], [IntelliJ][330]).
3. **Any lines into a commit.** Any lines of a hunk staged in part can be staged further, in the diff and the editor.
   - Stage Selected Ranges breaks again after it was mended, and a hunk staged in part cannot be staged further ([Cursor forum][331], [zed][332]).
4. **Whitespace ignored as the reader means.** Compare ignores whitespace at the ends, at the start, or everywhere, inside a hunk that mixes both.
   - Ignoring whitespace leaves a mixed hunk noisy, and one mode is all there is ([pierre #734][333], [IntelliJ][334]).
5. **A commit graph of any size.** The graph of a repository of hundreds of thousands of commits scrolls at the display's rate from the first frame.
   - The graph loads slowly and takes the processor, readers turn off parts to bear it, and it freezes on 40k commits ([neo-git-graph][335], [Cursor forum][336], [GitKraken][337]).
6. **Blame that is quiet and quick.** A line's blame shows after the caret rests, from one read kept, never a git spawned on save.
   - Blame distracts when at once and is slow when it waits, spawned a git on every save, and the tool is heavy and in part paid ([HN][338], [GitLens v17][339], [gitconnected][340]).
7. **Every stash, and cherry-pick in the graph.** Every stash git holds is listed, and a commit is cherry-picked from the graph.
   - A stash another tool made is not shown, cherry-pick has no place, and the graph lacks what the old list had ([vscode #257960][341], [vscode #226648][342], [Fishtank][343]).
8. **Every repository in a folder.** A folder of several repositories shows them all, each with its staged changes, and every tool works in each.
   - Not all repositories show, staged changes vanish, and tools work only for the first root ([Cursor forum][344], [Cursor forum][345], [vscode #396][346], [azdo #111][347]).
9. **Review comments on the lines written on.** A comment stays on the lines it was written on, and says when a push has moved them.
   - A comment shows on the new lines of its number, and how it goes stale is not shown ([vscode-pull-request-github #8374][348]).
10. **Compare by structure at any size.** The structural compare holds a file of many changes in little memory, and says where it falls back to lines.
    - The structural diff scales poorly and takes memory, large files fall back to text, and the side-by-side view can confuse ([difftastic][349], [difftastic][350], [difftastic][351]).
    - Changes at the end of a long line are hard to see side by side ([difftastic][352]).

## Testing

1. **Every test found, or why not.** Discovery shows each test as it is found, and a file whose tests cannot be read says why beside it.
   - Discovery stalls, a file shows none of its tests, the reason is only in an output channel, and the view is hidden until a plugin finds tests ([vscode-python #24650][353], [#24646][354], [#9216][355], [Cursor forum][356]).
2. **A run mark on every test, or a word why.** Every test gets its mark in the gutter, and a framework orior cannot run is named.
   - Marks are missing with no reason, and some tests get them and others not ([IntelliJ][357], [IntelliJ][358], [flutter-intellij][359]).
3. **Coverage true to the line.** Comments and blank lines take no mark, coverage shows in the editor, and the reader chooses gutter or whole line, in colors that stand out.
   - Comments are marked missed and blank lines covered, coverage shows only in the tree, marks do not stand out, and every line comes gray ([IntelliJ][360], [IntelliJ][361], [IntelliJ][362], [Cursor forum][363]).
4. **Each test's output its own.** Tests run at once each keep their output apart, and the reader marks tests that must run alone.
   - Output of tests run at once is mixed, logs interleave, and some tests must stay in a row ([openstack-dev][364], [Gradle forum][365], [IntelliJ][366], [MATLAB][367]).
5. **The tests a change touches.** orior finds the tests an edit reaches and runs them on one key, their results beside the edit.
   - Readers want the IDE to name the tests a change affects.

## Debugging

1. **Conditions tested in the program.** A condition or a hit count is tested where the program runs, holds every time, and takes effect as it is set.
   - A breakpoint takes minutes to hit, a hit count works once, a condition waits for another breakpoint, conditions slow the program, and an expression crashes the debugger ([cactus #1852][368], [Arduino forum][369], [VisualGDB][370], [werat.dev][371], [MS C++ blog][372]).
   - The debugger sticks with many breakpoints ([G2][373]).
2. **Exceptions of the reader's own.** The debugger stops on exceptions in the tree's own code, and one exception or file is muted from the stop itself.
   - Breaking on every exception stops in libraries over and over, nothing can be muted, and breaking in outside code slows the debugger ([MS blog][374], [WebKit #66632][375], [GJS MR 655][376], [Rider][377]).
3. **The value under the pointer, always.** The hover shows what the variables pane shows, for the frame the line is in, with names read whole.
   - The hover shows undefined, a frame not chosen shows nothing, and a name with a dash is split ([vscode #221503][378], [cbl-gdb #19][379], [seer #153][380]).
4. **Watches kept with the tree.** Watches stay with the tree when the debug window closes and orior restarts.
   - Watches are cleared when the window closes, and an expression added does not show ([IntelliJ][381], [IntelliJ poll][382], [GameDev.tv][383]).
5. **A process picker that lists at once.** The list of processes stands at once, filtered as typed, and the one chosen is attached.
   - The picker spins with no list, an error shows in its place, and a process chosen errors ([vscode-python-debugger #899][384], [objectscript #1411][385], [powershell #1322][386]).
6. **Programs started in order, stopped together.** Programs run together start in the order given, and one key stops them all.
   - The order is not kept, and stop takes one program at a time from a menu ([Rider][387], [IntelliJ][388]).
7. **Library code skipped, and entered when asked.** Stepping skips library code by default, in every launch and attach, and a key steps into it.
   - Just-my-code off is ignored when attaching, the standard library cannot be stepped into, and library code cannot be skipped by default and entered when wanted ([vscode-python-debugger #139][389], [#617][390], [ptvsd #2103][391], [Rider][392]).
8. **Machine code beside its source.** Machine code shows each instruction with its source line, the pointer moves as it steps, and memory reads in every width.
   - Opcodes are missing and the pointer does not move, wider formats show dashes, the views cannot be found, and source cannot be read beside the machine code ([noir #4904][393], [memview #48][394], [ST community][395], [PlatformIO][396], [IISc][397]).
9. **Python and C++ stepped together, on every system.** One session steps from Python into C++ and back, on Linux, Windows and the Mac.
   - Debugging both is a patchwork of pdb, prints and gdb, and the one tool that joins them is on one system ([TVM forum][398], [Visual Studio][399], [TotalView][400]).
10. **Back in time on every machine.** A record key and reverse steps work where a recorder runs, and where none does orior keeps a buffer of what ran last.
    - Recording is not in the IDE, and runs on Linux only and not on ARM ([Qt Creator][401], [rr][402], [arXiv 1705.05937][403]).
    - Stepping back from an exception by a buffer of what ran last, and following data past the edge of one system ([SD Times][236], [Strange Loop][404]).
11. **Edit and continue.** A change made while stopped takes effect without a restart, where the language allows.
    - Edit and continue, hot swap and data breakpoints are wished for ([Ayende][405], [IntelliJ][406], [Wallaby][407], [DEV][408]).
12. **The first failure, and what led to it.** Where errors cascade, orior shows the earliest that matters, with the chain of calls across awaits and the state that changed on the way to it, the later failures folded under it.
    - Stack traces that say little, asynchronous runs hard to follow, and errors buried under a cascade of failures that follow from them: the debugger tells what failed and not why.

## Layout

1. **Tabs that open where the reader is.** A file opens as a tab in the group in focus, and a tab can hold a set of files.
   - A tab holds one file and cannot hold a set ([vscode #143024][409]).
2. **Tool windows that come back where they were.** A floating window docks back on its own key, and every window opens on its monitor at its size.
   - A floating window docks back only by resetting the layout, a saved layout does not put it back, windows reopen tiny, and they forget their monitor ([IntelliJ][410], [IntelliJ][411], [IntelliJ][412]).
3. **Windows kept as windows.** A second window closes with its tabs, opens again after a restart, and is not hidden with the first.
   - Closing a floating window moves its tabs back, a restart does not bring it back, a terminal moved back breaks, and floating windows vanish when the main one is minimized ([DevClass][413], [vscode #10121][414], [VS blog][415]).
4. **Every terminal in sight.** Terminals split in a grid, none pushed out of sight, and two tabs show at once in one pane.
   - Deep splits push terminals out of sight, and two tabs cannot be seen at once ([beyondjapan][416], [IntelliJ][417]).
5. **Tabs down the side that take little room.** Side tabs narrow to their marks when the editor is split.
   - Side tabs take too much space when split, and one row of tabs fills in minutes ([Rider][418], [ReSharper][419], [Cursor forum][420], [DEV][421]).
6. **A status bar that stays clear.** Plugins add to the status bar only what the reader allows, each item says what it is of, and none shows to no purpose.
   - Plugins fill the bar, items are not clear, an item readers need is hidden, and an account mark shows for one account ([DEV][422], [cmake-tools #3423][423], [vscode-csharp #2146][424], [GitLab #1791][425]).
7. **A toolbar edited on the toolbar.** A toolbar is changed by dragging on it, every button in sight, and the menu shows whole by default.
   - The toolbar editor is hard to find and painful, buttons hide behind others, and the menu collapses by default ([IntelliJ][426], [IntelliJ][427]).
8. **Several trees as they sit on disk.** Trees in one window show in the explorer as they sit on disk.
   - The explorer shows every tree at the top whatever its place ([kitemetric][428]).
9. **Focus only where the reader puts it.** The pointer passing over orior never raises it, its caret shows only when it has the keys, and no dialog takes focus from a popup.
   - The IDE takes focus as the pointer passes, shows a caret while keys go elsewhere, a dialog takes focus from a popup, and a hidden pointer moves focus ([Code::Blocks][429], [KDE #348034][430], [IntelliJ][431], [IntelliJ][432], [Launchpad #1238410][433]).
10. **The system's title bar with orior's menu.** The system's title bar keeps orior's menu bar once, its controls, and a tiling manager's edges.
    - With the system's bar the menu is gone, the setting is not honored on Linux, the menu shows twice, and resize cursors show under a tiling manager ([Cursor forum][434], [Cursor forum][435], [Cursor forum][436], [Cursor forum][437]).
11. **A clean window that shows more as asked.** The window opens with the editor and the tree alone; every panel, toolbar and badge comes when asked; each feature says what it is where it stands, in one pattern across orior and its plugins; and keys and commands stay as they are.
    - Too many panels, toolbars, pop-ups, badges and nested menus, and more time spent keeping the window than writing code.

## Running and measuring

1. **Another machine that stays reached.** orior's part on another machine installs or updates with orior itself, keeps the link alive when idle, and signs in once.
   - The server install fails or hangs, versions part after an update, readers delete the server by hand, the link drops when idle, and sign-in is asked again and again ([Cursor forum][438], [Cursor forum][439], [MS Q&A][440], [MS Q&A][441], [USC HPC][442], [techoverflow][443]).
2. **A profile that reads at a glance.** The profiler samples at little cost, shows a tree of calls as a tree, names the hottest code first, and needs no kernel setting changed by hand.
   - Tracing changes the program measured, a long trace is too dense, a tree printed as text is hard to follow, a flame graph takes practice, and Linux needs kernel settings first ([Brendan Gregg][444], [moderndescartes][445], [DEV][446], [IntelliJ][447]).
3. **Containers at the speed of the disk.** A tree on the Windows drive runs in a container as fast as one in the container's own disk.
   - A shared folder makes builds, tests and git slow, readers move the tree to bear it, and stale builds need a rebuild with no cache ([Microsoft Learn][448], [endjin][449], [mcuoneclipse][450], [PyRIT][451]).
4. **Uploads that finish or say why.** Upload on save covers every file, the server holds across restarts, and a stalled upload says why and never keeps orior from closing.
   - Upload on save misses files edited on the host, the default server breaks after a restart, sign-in fails three saves in four, an upload hangs and keeps the IDE open, and uploads stall ([IntelliJ][452], [IntelliJ][453], [IntelliJ][454], [IntelliJ][455], [IntelliJ][456]).
5. **A build window that shows the error first.** The build's errors and warnings stand apart from its trace, each filtered by a press, long lines with plain bounds, and the whole log kept when the build ends.
   - Errors are buried with no filter, long lines wrap with no bounds in one color, the default hides the command, the log is replaced by a summary, and only there-were-errors is said ([KDevelop][457], [GNOME Builder #1044][458], [Qt][459], [Rider][460], [U++][461]).
6. **A quick edit, build and run.** A change runs again with one key, building only what it touched.
   - IDEs lack a quick edit-compile-run loop ([SD Times][236]).

## Notebooks

1. **A notebook that shows the state it runs on.** Cells run out of order are marked, the state shows beside them, notebooks scroll and draw outputs at the display's rate, compare cell by cell, and are never called changed by a clone.
   - The kernel keeps state the cells no longer show, scrolling lags, outputs take the processor, git calls a notebook changed after a clone, there is no diff, and work is lost to a failed cache ([vscode #147253][462], [refonte][463], [sympy][464], [PyCharm][465]).

## What readers wish for

1. **A light tool that grows as asked.** orior stays quick with every feature, and the reader turns off what is not wanted.
   - IDEs are slow, eat memory, start and index slowly, and grow heavy as features pile on ([devRant][466], [Capterra][467], [G2][468], [DEV][469], [DEV][470], [HN][471]).
   - The IDE is in the way, too clever, and the reader does not know what it does ([DEV][472]). orior shows each command it runs.
   - An editor of no lag that lets the reader add only the features wanted ([cplusplus.com][235], [IntelliJ][473]).
   - Cost, and a tool hard to learn and unkind to beginners ([G2][474]). orior is free, and every key and feature is found from the command palette.
2. **A compiler's view open to the tools.** The facts orior reads of the code are open to its plugins and its command line.
   - A compiler usable as a library is the ground of good IDE and build tools ([D forum][475]).
3. **An assistant that works the reader's way.** An assistant in orior runs locally where it can, keeps the tree's history and the reader's choices, shows the context it uses and lets it be edited, marks what it wrote, shows each prompt's cost, and asks before it acts.
   - Developers want the context set and seen by hand, nothing lost, no logging in again, code kept home, AI code marked, and behavior chosen per task ([JetBrains Research][476]).
   - Developers want agents that work unattended and return changes for review, shown and paused, guided by requirements kept in files, with permissions, checkpoints and skills kept under version control ([RedMonk][477]).
   - Users of editors built around AI report limits of context and memory, intrusive suggestions, high cost, poor generated code and a poor interface: intelligence added does not by itself make the work better, and it has to be useful, well timed and trusted.
4. **Measures of the reader's own work.** orior shows the reader what they did in a day, for them alone.
   - Two thirds of developers doubt the measures of their work reflect what they give ([JetBrains][478]).
5. **An assistant's change reviewed as a diff.** Every change an assistant makes is shown as an exact diff in the tree's own conventions, with its tests' results beside it, and is refused or taken back in one step.
   - In Stack Overflow's 2025 survey, 66% of those using AI tools named outputs that are almost right as a frustration, and 45% the debugging of code AI wrote ([Stack Overflow 2025][479]).
6. **No way of working forced.** Every feature of orior can be left unused, and orior works beside the reader's own tools: their git, their terminal, their build and the keys of the editor they came from.
   - Readers want freedom: an IDE that forces a way of working or a set of tools on them is one they come to hate.
   - "How do I make completion work like another editor" shows a way of working orior should bend to. orior's completion, keys and layout can each take the ways of the editors readers come from.
7. **Help that answers the question as people ask it.** The command palette takes a question in the reader's words, as "turn off suggestions" or "stop formatting on save", and answers with the setting or the command, changed there; a setting many readers turn off is off by default.
   - What readers search for shows the work's friction where no one files a report: how to turn something off, stop it, take it back, set it or get round it.
   - A question can show a feature that is there and cannot be found, a default readers do not want, a thing the tool cannot do, a thing done in too many steps, or behavior and words that are hard to understand. Each wants its own answer: where thousands ask how to turn a feature off, the answer may be a better default where a better guide is no answer at all.
   - The four questions about turning something off show the editor acting where the reader does not want it to, controls that are hard to understand, behavior spread across settings, plugins and languages, and a way round to be found where the reader would say what they want.
   - Views, answers and questions asked twice measure how many ask, with limits: views pile up over time, many answers can show confusion more than numbers, and a question closed as asked before does not count everyone who met it.
   - What the questions show is not that readers turn features off: it is that they need help again and again to take back control of what the tool does for them. These are kinds of question to look into, and not every one shows a bad design.
   - An answer of the form "install this plugin, change these four settings and restart" shows more than a want of documentation. What takes a plugin, settings and a restart elsewhere is one setting in orior, in force at once.
   - The richest openings may lie between what an IDE can do and what a reader can easily make it do.
8. **The questions asked of IDEs, gathered and weighed.** For VS Code, IntelliJ IDEA, Visual Studio, PyCharm and Eclipse, the searches, the Stack Overflow questions with their views, answers, questions marked as asked before and dates, the GitHub issues, the pages that say how to turn a thing off or get round it, and the release notes that later changed it are gathered; the questions that mean one need are put together, as "disable the IntelliSense popup", "stop autocomplete while typing" and "only show suggestions on demand"; and each need is weighed apart by how many ask, whether it is still asked years on, how many settings, plugins and steps the way round takes, and whether what exists meets it. orior's roadmap takes the needs in that order.
   - The pages found so far are each checked on their own; which problems are most common wants the whole set of searches, across search engines, before it is claimed.

<!-- docs-check: quoting -->
[1]: https://gist.github.com/hyperupcall/99e355405611be6c4e0a38b6e3e8aad0
[2]: https://go.googlesource.com/vscode-go/+/a64f4f0ab48c3fb74f7a4b7ad07f84e8cb9bbc4a%5E%21/
[3]: https://users.aalto.fi/~ltuuri/apu/editors/vscode/annoyances
[4]: https://issues.redhat.com/browse/CRW-3475
[5]: https://forum.cursor.com/t/add-toggle-for-installation-appears-to-be-corrupt-notification/708
[6]: https://forum.cursor.com/t/agent-panel-opens-aggressively-by-default-editor-window-in-agents-doesn-t-focus-the-current-project/160789
[7]: https://issues.redhat.com/browse/CRW-3539
[8]: https://forum.cursor.com/t/sometimes-i-connot-find-my-cursor-window-that-has-already-opened/165346
[9]: https://bugs.freepascal.org/view.php?id=23839
[10]: https://bz.apache.org/netbeans/show_bug.cgi?id=123659
[11]: https://bugs.freepascal.org/view.php?id=35947
[12]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206950825/comments/207149659
[13]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/207077515/comments/207489799
[14]: https://blog.jetbrains.com/idea/2019/12/intellij-platform-roadmap-for-2020/
[15]: https://blog.jetbrains.com/?p=315068/
[16]: https://assets.theregister.com/2021/07/07/visual_studio_code_workspace_trust/
[17]: https://code.visualstudio.com/blogs/2021/07/06/workspace-trust
[18]: https://til.hashrocket.com/posts/z20q7mylrg-disable-broken-vscode-feature
[19]: https://www.infosecurity-magazine.com/infosec/atom-community-splits-over/
[20]: https://mail.haskell.org/pipermail/ghc-devs/2016-December/013392.html
[21]: https://wiki.debian.org/PrivacyIssues
[22]: https://bfdi.bund.de/SharedDocs/Pressemitteilungen/EN/2023/17-Berlin-Group-telemetry.html?nn=253562
[23]: https://resharper-support.jetbrains.com/hc/en-us/community/posts/206079839-Intellisense-code-completion-pop-up-slow
[24]: https://lightrun.com/answers/vuejs-vetur-intellisense-in-latest-vetur-is-painfully-slow
[25]: https://github.com/microsoft/vscode-cpptools/issues/12017
[26]: https://blog.jetbrains.com/idea/2015/08/experimental-zero-latency-typing-in-intellij-idea-15-eap/
[27]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206869565/comments/206393565
[28]: https://www.assembla.com/spaces/adRvL6yMKr24I9aaeP0Qfc/tickets/605
[29]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206237779/comments/207027139
[30]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206912245/comments/206523805
[31]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/7008059652754/comments/7037407299090
[32]: https://github.com/microsoft/vscode/issues/107999
[33]: https://lemm.ee/comment/7267826
[34]: https://github.com/Microsoft/vscode-cpptools/issues/1640
[35]: https://forum.sublimetext.com/t/minimap-scroll-behavior-has-some-issues/2866
[36]: https://forum.sublimetext.com/t/scroll-wheel-on-minimap/473
[37]: https://github.com/microsoft/vscode/issues/192027
[38]: https://github.com/microsoft/vscode/issues/164352
[39]: https://github.com/microsoft/vscode/issues/99171
[40]: https://github.com/Microsoft/vscode/issues/27317
[41]: https://github.com/Microsoft/vscode/issues/8673
[42]: https://github.com/microsoft/vscode/issues/206720
[43]: https://lightrun.com/answers/vscodevim-vim-undo-via-u-does-not-remove-file-dirty-flag
[44]: https://kitemetric.com/blogs/how-to-prevent-vscode-from-losing-unsaved-tabs-across-multiple-windows
[45]: https://github.com/Enter-tainer/typst-preview/issues/245
[46]: https://www.goodfirstissue.org/VSCodeVim/Vim/issues/2021
[47]: https://lightrun.com/answers/microsoft-vscode-slow-reponse-to-scroll-and-typing
[48]: https://community.sonarsource.com/t/orphaned-processes-and-high-cpu-utilization-with-vs-code-and-sonarlint-extension/60430
[49]: https://discourse.julialang.org/t/lingering-julia-processes/56699
[50]: https://pawsey.atlassian.net/wiki/spaces/US/pages/51931360/Visual+Studio+Code+for+Remote+Development
[51]: https://forum.cursor.com/t/cursor-remote-ssh-session-fails-when-opening-large-files/150960
[52]: https://www.thundercompute.com/docs/vscode/operations/file-transfers
[53]: https://forum.cursor.com/t/accessibility-support-prefers-reduced-motion-in-ai-chat-panels/152656
[54]: https://blog.jetbrains.com/platform/2026/02/editor-improvements-smooth-caret-animation-and-new-selection-behavior/
[55]: https://forum.cursor.com/t/cursors-explorer-tab-has-serious-performance-issues/166778
[56]: https://discourse.julialang.org/t/julia-in-vscode-takes-ages-to-first-execution/113299
[57]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/115000755064/comments/115000630144
[58]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360004020699/comments/360000550720
[59]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360003478399/comments/360002285780
[60]: https://www.nngroup.com/articles/progress-indicators/
[61]: https://arxiv.org/pdf/2507.22352
[62]: https://www.researchgate.net/publication/380458347_Investigating_the_Impact_of_Progress_Indicator_Design_on_User_Perception_of_Delay
[63]: https://lawsofux.com/doherty-threshold/
[64]: https://newsletter.kentbeck.com/p/the-precious-eyeblink
[65]: https://helgeklein.com/blog/fixing-vs-code-ui-unresponsiveness-caused-by-github-copilot-extension
[66]: https://en.wikipedia.org/wiki/Calm_technology
[67]: https://calmtech.com/
[68]: https://fuzzymath.com/blog/calm-technology-enterprise-web-application-ui-design/
[69]: https://gi-radar.de/tl/DC-0313
[70]: https://doaj.org/article/449f4dd4e9e14e5584f777600e5320f0
[71]: https://android.gadgethacks.com/news/dark-mode-vs-light-mode-what-research-says-about-when-each-works-best/
[72]: https://forum.devolutions.net/topics/27204/disable-toasts
[73]: https://www.jetbrains.com/help/idea/notifications.html
[74]: https://forum.asana.com/t/overlapping-pop-up-notification-toasts-piling-up-over-tasks/161393
[75]: https://gitlab.gnome.org/GNOME/libadwaita/-/issues/242
[76]: https://spark.genesys.com/latest/components/feedback/toasts/usage-FKiiY5xJ
[77]: https://forum.cursor.com/t/setting-prefer-notifications-same-as-chat-does-not-work/163357
[78]: https://joeldare.com/why-im-starting-to-hate-vscode
[79]: https://dspace.ewha.ac.kr/handle/2015.oak/209767
[80]: https://faculty.kfupm.edu.sa/ICS/garout/Teaching/SWE312/DTUI-Ch12.pdf
[81]: https://uxdesign.cc/bad-error-messages-are-harming-your-users-1ee52b8e520f
[82]: https://sites.umiacs.umd.edu/node/17449
[83]: https://forums.oracle.com/ords/apexds/post/sql-query-results-slow-7502
[84]: https://github.com/cweijan/vscode-database-client
[85]: https://intellij-support.jetbrains.com/hc/zh-cn/community/posts/206762529/comments/207251065
[86]: https://intellij-support.jetbrains.com/hc/zh-cn/community/posts/206762529/comments/207693089
[87]: https://github.com/Microsoft/vscode/issues/64685
[88]: https://forum.cursor.com/t/markdown-preview-on-the-side-does-not-interact-well/128014
[89]: https://dev.to/woai3c/implementing-synchronous-scrolling-in-a-dual-pane-markdown-editor-5d75
[90]: https://lightrun.com/answers/asciidoctor-asciidoctor-intellij-plugin-mermaid-problems
[91]: https://lightrun.com/answers/ritwickdey-vscode-live-server-live-reload-not-working
[92]: https://kitemetric.com/blogs/conquering-the-live-reload-is-not-possible-error-in-vs-code-live-server
[93]: https://dev.to/satokenta/choosing-the-best-api-testing-tool-postman-vs-thunder-client-2onp
[94]: https://community.smartbear.com/t5/TestComplete-Questions/Code-Editor-Unexpected-peek-popup/td-p/225949
[95]: https://forum.cursor.com/t/quick-preview-not-navigating-correctly/428
[96]: https://rider-support.jetbrains.com/hc/en-us/community/posts/360001333220/comments/360000187400
[97]: https://sysprogs.com/w/?p=20388
[98]: https://forum.figma.com/suggest-a-feature-11/improve-the-keyboard-shortcuts-panel-41297
[99]: https://community.platformio.org/t/breadcrumbs-what-are-they/21472
[100]: https://dzone.com/articles/when-breakpoints-dont-break
[101]: https://learn.microsoft.com/en-au/visualstudio/debugger/using-tracepoints?view=vs-2019
[102]: https://arxiv.org/html/2504.06808v1
[103]: https://arxiv.org/html/2511.18842v1
[104]: https://arxiv.org/pdf/2511.18849
[105]: https://arxiv.org/html/1805.05504v1
[106]: https://www.interruptions.net/literature/Hodgetts-JEP_LMC06.pdf
[107]: https://www.interruptions.net/literature/Trafton-HFES05.pdf
[108]: https://uwe-repository.worktribe.com/index.php/output/1028733/interrupting-problem-solving-colour-cues-facilitate-task-resumption
[109]: https://pmc.ncbi.nlm.nih.gov/articles/PMC12271330
[110]: https://www.academia.edu/30032828/Building_Usage_Contexts_During_Program_Comprehension
[111]: https://cs.ubc.ca/~murphy/papers/disorientation/disorientation-vlhcc06.pdf
[112]: https://webhome.cs.uvic.ca/~mstorey/etx2005/papers-3/5%20Relo%20Helping%20Users%20Manage%20Context%20During%20Interactive%20Exploratory%20Visualization.pdf
[113]: https://www.eecs.ucf.edu/~jjl/pubs/p2503-bragdon.pdf
[114]: https://ar5iv.labs.arxiv.org/html/2304.09301
[115]: https://arxiv.org/html/2605.23135v1
[116]: https://arxiv.org/html/2507.03156v1
[117]: https://arxiv.org/html/2405.06271v1
[118]: https://arxiv.org/pdf/1601.05330
[119]: https://arxiv.org/pdf/1703.04993
[120]: https://arxiv.org/pdf/1701.02952
[121]: https://mac.konigi.com/blog/shneidermans-eight-golden-rules-interface-design
[122]: https://alandix.com/academic/talks/risk/
[123]: https://alandix.com/academic/papers/interact97
[124]: https://www.merl.com/publications/docs/TR2005-024.pdf
[125]: https://www.medien.informatik.uni-muenchen.de/pubdb/publications/pub/rossmy2023chi/rossmy2023chi.pdf
[126]: https://www.slideshare.net/slideshow/2012-icse-programcomprehension/13247057
[127]: https://repositorio.uchile.cl/handle/2250/169413
[128]: https://arxiv.org/pdf/1807.08578v1.pdf
[129]: https://arxiv.org/pdf/2306.09541
[130]: https://jeffhuang.com/papers/SEEDE_ASE18.pdf
[131]: https://arxiv.org/pdf/2407.01407
[132]: https://github.com/pajenterprise/vscode-wiki/blob/master/Explain-extension-causes-high-cpu-load.md
[133]: https://forum.cursor.com/t/extension-host-freezes/167335
[134]: https://bbs.archlinux.org/viewtopic.php?id=282251
[135]: https://bugs.archlinux.org/task/62154
[136]: https://discourse.nixos.org/t/sandboxless-vscodium/78979/9
[137]: https://github.com/microsoft/vscode/issues/17521
[138]: https://github.com/toumorokoshi/vscode-wiki/blob/master/Keybinding-Issues.md
[139]: https://gitlab.com/gitlab-org/gitlab/-/work_items/435632
[140]: https://code.visualstudio.com/docs/configure/settings-sync
[141]: https://community.sonarsource.com/t/vscode-plugin-sync-feature/24682
[142]: https://github.com/shanalikhan/code-settings-sync/issues/396
[143]: https://github.com/microsoft/vscode/pull/104293/files
[144]: https://github.com/coder/code-server/issues/5821
[145]: https://www.bleepingcomputer.com/news/security/malicious-vscode-extensions-with-229m-installs-found-on-microsoft-marketplace/
[146]: https://cybersecuritynews.com/flaws-vs-code-marketplace-malicious-extensions
[147]: https://www.koi.ai/blog/vscode-extension-trivia-real-or-cake
[148]: https://lists.geany.org/hyperkitty/list/users@lists.geany.org/message/NS2M6MHWOOYLN5IQAO4LTCSRA2YKRPV6
[149]: https://gitlab.gnome.org/GNOME/gimp/issues/416
[150]: https://code.visualstudio.com/docs/configure/themes.md
[151]: https://discuss.ocaml.org/t/inconsistent-highlighting-on-vscode-with-ocaml-platform/14394
[152]: https://marketplace.visualstudio.com/items/fisheva.Eva-Theme
[153]: https://community.sourcegraph.com/t/in-vs-code-workspace-preinstruction-settings-override-user-settings/1161
[154]: https://gitlab.mpcdf.mpg.de/nomad-lab/nomad-FAIR/-/merge_requests/1122/pipelines
[155]: https://code.visualstudio.com/docs/editing/workspaces/multi-root-workspaces
[156]: https://hub.openclaw.ai/ivangdavila/vscode
[157]: https://github.com/flathub/com.visualstudio.code/issues/370
[158]: https://dev.to/antek5421/dev-containers-not-working-check-if-youre-using-flatpak-vs-code-21c5
[159]: https://github.com/microsoft/vscode-cpptools/issues/1550
[160]: https://forum.cursor.com/t/every-update-keeps-breaking-key-bindings-and-latest-one-broke-sidebar-location/148301
[161]: https://forum.cursor.com/t/just-updated-my-ui-layout-was-completely-changed-without-consent/151610
[162]: https://forum.cursor.com/t/unwanted-forced-updates-applied-without-consent-or-warning/167558
[163]: https://discuss.ai.google.dev/t/antigravity-2-0-breaks-core-developer-tools/146988
[164]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/20802107307154/comments/22024504910226
[165]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206218419/comments/206443975
[166]: https://code.visualstudio.com/docs/editor/portable
[167]: https://forum.cursor.com/t/stop-hiding-settings-inside-database/162622
[168]: https://forum.cursor.com/t/cannot-import-settings-and-extensions-from-vscode-insiders/53423
[169]: https://www.codegenes.net/blog/what-are-all-configuration-files-used-by-visual-studio-code-and-where-does-it-store-memory-of-what-notifications-to-not-show-again/
[170]: https://naildrivin5.com/blog/2019/09/24/a-reasonable-dev-environment.html
[171]: https://cloudomation.com/cloudomation-blog/why-the-local-development-environment-should-be-replaced/
[172]: https://developer.mozilla.org/en-US/docs/Learn_web_development/Extensions/Client-side_tools/Introducing_complete_toolchain
[173]: https://www.mintlify.com/moonrepo/moon/concepts/toolchain
[174]: https://www.thoughtworks.com/radar/tools/devbox
[175]: https://discourse.cmake.org/t/tutorial-for-setting-up-toolchain-environment/3178
[176]: https://medium.com/@Fredtaylor1/why-is-the-windows-dev-environment-such-a-pain-to-set-up-and-why-do-so-many-people-still-use-it-3fe4ac07fce1
[177]: https://computerscience.chemeketa.edu/guides/vscode-setup/vs-code-issues/
[178]: https://community.gamedev.tv/t/unreal-engine-c-dev-include-path-not-defined-cant-find-iostream-help/104944
[179]: https://code.visualstudio.com/docs/cpp/customize-cpp-settings.md
[180]: https://vscode-docs-arc.readthedocs.io/en/latest/cpp/config-msvc/
[181]: https://buttondown.com/SystemCraftPress/archive/vs-code-running-the-wrong-python-again/
[182]: https://dev.to/dmitryzub/debugging-pycharmintellij-idea-no-module-named-error-inside-virtual-environment-a7g
[183]: https://github.com/microsoft/vscode-python/issues/24573
[184]: https://bbs.archlinux.org/viewtopic.php?id=260813
[185]: https://lightrun.com/answers/microsoft-vscode-python-python-interpreter-not-detected
[186]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206230739/comments/206489315
[187]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360000143679/comments/360000037440
[188]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206347339/comments/207385069
[189]: https://www.g2.com/products/visual-studio-code/reviews?page=41
[190]: https://www.g2.com/products/visual-studio-code/reviews?page=27
[191]: https://github.com/microsoft/vscode/issues/105870
[192]: https://golang.org/issue/37236
[193]: https://golang.org/issue/66523
[194]: https://github.com/microsoft/python-language-server/issues/1393
[195]: https://github.com/microsoft/python-language-server/issues/2057
[196]: https://discussions.unity.com/t/the-extraction-method-is-not-functioning-properly/951326
[197]: https://jetbrains.com/help/idea/2023.1/extract-method.html
[198]: https://resharper-support.jetbrains.com/hc/en-us/community/posts/206745635/comments/206644769
[199]: https://industriallogic.com/blog/improving-extract-method
[200]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/207024575/comments/206891885
[201]: https://www.jetbrains.com/help/idea/change-signature.html
[202]: https://docs.devsense.com/vscode/problems/workspace-diagnostics/
[203]: https://app.semanticdiff.com/gh/astral-sh/ruff/commit/dc56c3361838e871d06b472c1611c0601ed500f3
[204]: https://intellij-support.jetbrains.com/hc/zh-cn/community/posts/19455961339026/comments/19603912160402
[205]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206836205/comments/206312365
[206]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/16278101898642/comments/16302502539410
[207]: https://handbook.gitlab.com/handbook/tools-and-tips/editors-and-ides/jetbrains-ides/code-inspection/why-are-there-noinspection-comments
[208]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206916485/comments/206539235
[209]: https://intellij-support.jetbrains.com/hc/zh-cn/community/posts/10055563435282/comments/15833724352530
[210]: https://rider-support.jetbrains.com/hc/en-us/community/posts/360010156979/comments/16353692509970
[211]: https://community.nxp.com/t5/MCUXpresso-IDE/Open-Call-Hierarchy-not-working/m-p/1356688
[212]: https://mathworks.com/help//releases/R2021a/polyspace_code_prover_access/ug/call-hierarchy.html
[213]: https://redlib.hackliberty.org/r/Unity3D/comments/1l7s879/why_do_people_dislike_vs_code
[214]: https://users.rust-lang.org/t/type-suggestion-in-vscode-and-rust-analyzer-is-not-visually-friendly/73375
[215]: https://users.rust-lang.org/t/what-is-this-added-text-and-how-do-i-hide-it/84468
[216]: https://users.rust-lang.org/t/visual-studio-code-rust-analyzer-cannot-disable-inlay-hints/66650
[217]: https://github.com/rust-lang/rust-analyzer/issues/3924
[218]: https://github.com/usernamehw/vscode-error-lens/issues/206
[219]: https://yzhums.com/11518/
[220]: https://arxiv.org/abs/2509.21067v2
[221]: https://github.com/nordtheme/vim/issues/269
[222]: https://lists.gnu.org/archive/html/emacs-devel/2022-11/msg00868.html
[223]: https://blog.jez.io/tree-sitter-limitations
[224]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206270349/comments/207144589
[225]: https://www.jetbrains.com.cn/en-us/help/idea/structural-search-and-replace.html.md
[226]: https://forum.cursor.com/t/how-to-disable-shell-details-hover-in-terminal-panel/85731
[227]: https://discuss.codemirror.net/t/hover-tooltip-disappears-when-try-to-hover-on-the-tooltip/8877
[228]: https://dev.to/matthewbill/typescript-regions-5h1f
[229]: https://github.com/microsoft/vscode/issues/12146
[230]: https://golang.org/issue/67398
[231]: https://community.devsense.com/d/788/1
[232]: https://gitlab.com/SiLA2/sila_python/-/issues/6
[233]: https://polar.sh/mkdocstrings/python/issues/57
[234]: https://redirect.github.com/tox-dev/sphinx-autodoc-typehints/discussions/281
[235]: https://cplusplus.com/forum/lounge/59829/
[236]: https://sdtimes.com/debugging/integration-watch-gimme-better-tools-please
[237]: https://github.com/microsoft/vscode/issues/146910
[238]: https://github.com/prettier/prettier-vscode/issues/638
[239]: https://israynotarray.com/en/vscode/2021/07/09/vscode-auto-fix-eslint-errors-on-save/
[240]: https://typescript-eslint.io/users/what-about-formatting
[241]: https://blog.joshuakgoldberg.com/configuring-eslint-prettier-and-typescript-together-faqs/
[242]: https://lightrun.com/answers/eslint-eslint-conflict-with-prettier
[243]: https://devplaybook.cc/blog/prettier-vs-eslint
[244]: https://github-redirect.dependabot.com/psf/black/issues/2135
[245]: https://github-redirect.dependabot.com/psf/black/issues/1941
[246]: https://github.com/astral-sh/ruff/issues/1200
[247]: https://code.visualstudio.com/docs/python/environments
[248]: https://github.com/microsoft/vscode-python/issues/24640
[249]: https://github.com/microsoft/vscode-python/issues/12164
[250]: https://go.chriswarrick.com/fourteen
[251]: https://www.pinecone.io/blog/pain-poetry-python
[252]: https://www.kdnuggets.com/2023/07/forget-pip-conda-requirementstxt-poetry-instead-thank-later.html
[253]: https://lightrun.com/answers/microsoft-vscode-python-speed-up-linting
[254]: https://github.com/Jammf/vscode-mypy
[255]: https://github.com/redhat-developer/vscode-yaml/issues/397
[256]: https://community.f5.com/discussions/technicalforum/as3-per-app-json-schema-issue/346776
[257]: https://github.com/microsoft/vscode-cmake-tools/issues/1096
[258]: https://fvdb-core.readthedocs.io/latest/markdown/clangd.html
[259]: https://code.visualstudio.com/docs/cpp/faq-cpp
[260]: https://github.com/anordal/annoying-build-systems
[261]: https://cmake.org/pipermail/cmake-developers/2014-October/023409.html
[262]: https://forge.etsi.org/rep/cyber/103523_MSP/tlmsp/tlmsp-curl/-/commit/1d265692b70e6f453cb8f9b550fe4858b4fa2524
[263]: https://community.genesys.com/discussion/purecloudplatformclientv2-slow-or-fails-to-load-in-my-vscode
[264]: https://github.com/microsoft/vscode-python/issues/25357
[265]: https://forums.developer.nvidia.com/t/is-it-possible-to-reduce-the-amount-of-ram-that-pylance-is-using/292263.md
[266]: https://forum.cursor.com/t/cursor-pyright-exceedingly-slow/153748
[267]: https://blog.rust-lang.org/2025/09/10/rust-compiler-performance-survey-2025-results
[268]: https://thenewstack.io/where-does-the-time-go-rusts-problem-with-slow-compiles/
[269]: https://users.rust-lang.org/t/rust-lang-compile-slow-very-slow/54956
[270]: https://par.nsf.gov/biblio/10106801
[271]: https://gcc.gnu.org/pipermail/gcc-help/2007-March/062780.html
[272]: https://issues.dlang.org/bugs/7768/
[273]: https://gamedev.net/forums/topic/378873-stdstl-is-the-root-to-all-evil/
[274]: https://medium.com/%40ericclemmons/javascript-fatigue-48d4011b6fc4
[275]: https://hackernoon.com/tools-for-radicals-73b7cbbfc276
[276]: https://redlib.hbubli.cc/r/programming/comments/80290k/webpack_4/dusr9tc/?context=3
[277]: https://www.vivianvoss.net/blog/the-esmodules-replacement
[278]: https://answer.demo.spiceai.io/questions/10010000000038279
[279]: https://lightrun.com/answers/microsoft-vscode-new-explorer-allow-to-filter-by-path
[280]: https://discuss.cachyos.org/t/drag-and-drop-files-with-ctrl-not-working-in-vscode/20328
[281]: https://www.xyplorer.com/xyfc/viewtopic.php?p=236481
[282]: https://github.com/microsoft/vscode/issues/188273
[283]: https://redirect.github.com/microsoft/vscode/wiki/Search-Issues
[284]: https://forum.cursor.com/t/indexing-and-glob-follow-gitignore-without-honoring-vs-code-search-settings-search-useignorefiles-search-useglobalignorefiles/151379
[285]: https://github.com/toumorokoshi/vscode-wiki/blob/master/Search-Issues.md
[286]: https://forum.cursor.com/t/search-exclude-setting-seems-to-no-longer-be-working/6105
[287]: https://bobbyhadz.com/blog/exclude-folders-from-search-in-vscode
[288]: https://dev.to/nitishk72/upgrading-vs-code-workspace-1cfe
[289]: https://intellij-support.jetbrains.com/hc/zh-cn/community/posts/206886035/comments/206442475
[290]: https://forum.cursor.com/t/breadcrumbs-in-editor-are-not-scrollable-for-deeply-nested-files/154892
[291]: https://sourceforge.net/p/zipeditor/wiki/
[292]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206127349/comments/206722879
[293]: https://forum.sublimetext.com/t/macros-dont-record-find-f3/49862
[294]: https://www.jetbrains.com/help/idea/2023.1/using-macros-in-the-editor.html
[295]: https://elpa.gnu.org/packages/undo-tree-readme.txt
[296]: https://gitlab.com/gitlab-org/gitlab-vscode-extension/-/issues/1824
[297]: https://dev.to/namasivaayam_l_1a17186e37/why-global-undo-sucks-building-line-level-undoredo-for-vs-code-5a45
[298]: https://github.com/VSCodeVim/Vim/issues/8603
[299]: https://forum.cursor.com/t/vim-mode-navigation-is-very-laggy/138606
[300]: https://mwop.net/blog/211-Why-Vim-Plugins-for-IDEs-Fail.md
[301]: https://dev.to/taku25/vim-and-i-254g
[302]: https://community.platformio.org/t/function-folding-not-permanent/39054
[303]: https://rider-support.jetbrains.com/hc/en-us/community/posts/115000744504-Remember-code-folding
[304]: https://forum.cursor.com/t/color-decorators-do-not-work-for-hex-codes/145330
[305]: https://github.com/microsoft/vscode/issues/182214
[306]: https://github.com/microsoft/vscode/issues/182534
[307]: https://forum.fast-report.com/en/discussion/comment/43226
[308]: https://community.notepad-plus-plus.org/post/79853
[309]: https://forum.sublimetext.com/t/virtual-space/455
[310]: https://github.com/Microsoft/vscode/issues/41542
[311]: https://github.com/microsoft/vscode/issues/172171
[312]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360010735720/comments/360003129800
[313]: https://lightrun.com/answers/vscode-neovim-vscode-neovim-easymotion-display-wrong-letters
[314]: https://www.goodfirstissue.org/VSCodeVim/Vim/issues/2388
[315]: https://github.com/DavidLGoldberg/jumpy2/blob/main/README.md
[316]: https://israynotarray.com/en/misc/2024/07/15/zero-width-space-zwsp-invisible-characters-that-break-code/
[317]: https://git.secluded.site/zed/commit/58e5d4ff0245368aa36b3274361d51a2a6dbe25b
[318]: https://github.com/deltoro05/vscode-gremlins
[319]: https://bugs.kde.org/show_bug.cgi?id=308614
[320]: https://bz.apache.org/netbeans/show_bug.cgi?id=262685
[321]: https://bz.apache.org/netbeans/show_bug.cgi?id=205881
[322]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360009349559/comments/360002030000
[323]: https://www.eclipse.org/forums/index.php/t/210333/
[324]: https://community.smartbear.com/discussions/testcomplete-questions/testcomplete-editor-turn-off-insert-pair-aka-automatically-close-quotes-and-brac/141121/replies/141123
[325]: https://bobbyhadz.com/blog/vscode-git-please-clean-your-repository-working-tree-before-checkout
[326]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360008739800/comments/360001774400
[327]: https://code.visualstudio.com/docs/sourcecontrol/merge-conflicts
[328]: https://dev.to/vscodetips/vs-code-tip-of-the-week-3-way-merge-editor-416h
[329]: https://blog.kaleidoscope.app/2022/04/12/resolving-merge-conflicts-in-vscode-with-kaleidoscope/
[330]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206217899/comments/206963609
[331]: https://forum.cursor.com/t/please-update-vscode-version-to-the-latest-stable-one/52592
[332]: https://git.secluded.site/zed/commit/45146b6f30f6c8c1fa4bd076fae34ad2b4e886d4
[333]: https://upd.dev/pierrecomputer/pierre/issues/734
[334]: https://www.jetbrains.com/help/idea/2025.2/differences-viewer.html
[335]: https://www.mintlify.com/asispts/neo-git-graph/guides/troubleshooting
[336]: https://forum.cursor.com/t/gitlens-commit-graph-freezes-cursor-renderer-fine-in-vs-code/168712
[337]: https://www.gitkraken.com/blog/gitlens-vs-vs-code-git-graph-use-cases-from-reddit-2026
[338]: https://grivy.duckdns.org/threads?id=RaoulP
[339]: https://help.gitkraken.com/gitlens/GL-Release-v17-x
[340]: https://levelup.gitconnected.com/better-git-line-blame-a-lightweight-gitlens-alternative-for-vs-code-21617e2d1ebb
[341]: https://goodfirstissue.org/Microsoft/vscode/issues/257960
[342]: https://github.com/microsoft/vscode/issues/226648
[343]: https://www.getfishtank.com/insights/streamline-your-workflow-cherry-picking-with-git-in-vscode
[344]: https://forum.cursor.com/t/cursor-source-control-tab-issue/141422
[345]: https://forum.cursor.com/t/staged-git-changes-disappear-in-multi-root-workspace-after-pylance-uninstallation-and-restart/132908
[346]: https://github.com/Microsoft/vscode/issues/396
[347]: https://github.com/ankitbko/vscode-pull-request-azdo/issues/111
[348]: https://github.com/microsoft/vscode-pull-request-github/issues/8374
[349]: https://github.com/osa1/difftastic/blob/master/CHANGELOG.md
[350]: https://github.com/mbrukman/difftastic
[351]: https://crates-io.ganymede-static.progval.net/crates/difftastic/0.9.0
[352]: https://github.com/grunweg/difftastic/blob/master/CHANGELOG.md
[353]: https://github.com/microsoft/vscode-python/issues/24650
[354]: https://github.com/microsoft/vscode-python/issues/24646
[355]: https://github.com/microsoft/vscode-python/issues/9216
[356]: https://forum.cursor.com/t/test-explorer-not-working-in-some-workspace/158363
[357]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/115000590884/comments/115000597624
[358]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/6846387322258/comments/6851878911634
[359]: https://lightrun.com/answers/flutter-flutter-intellij-unable-to-see-test-icons-in-gutter
[360]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206901535/comments/207005319
[361]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206901845/comments/207006179
[362]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206000449/comments/205790065
[363]: https://forum.cursor.com/t/apex-test-coverage-highlighting-is-broken/89056
[364]: https://lists.openstack.org/pipermail/openstack-dev/2014-March/029588.html
[365]: https://discuss.gradle.org/t/logs-in-the-test-report-are-interleaved-between-tests-if-running-tests-in-parallel-with-junit/51900
[366]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206254769/comments/207087299
[367]: https://au.mathworks.com/help/matlab/ref/matlab.unittest.testrunner.runinparallel.html
[368]: https://github.com/hyperledger/cactus/issues/1852
[369]: https://forum.arduino.cc/t/debugger-ide-2-3-1-breakpoint-issues/1225284
[370]: https://sysprogs.com/w/?p=949
[371]: https://werat.dev/blog/how-conditional-breakpoints-work/
[372]: https://devblogs.microsoft.com/cppblog/enhanced-breakpoint-expressions-for-c-debugging-in-visual-studio
[373]: https://www.g2.com/products/visual-studio-code/reviews?page=68
[374]: https://learn.microsoft.com/en-us/archive/blogs/jmstall/making-catch-rethrow-more-debuggable
[375]: https://bugs.webkit.org/show_bug.cgi?id=66632
[376]: https://gitlab.gnome.org/GNOME/gjs/-/merge_requests/655
[377]: https://www.jetbrains.com/help/rider/2021.1/Debugging_Exceptions.html
[378]: https://github.com/microsoft/vscode/issues/221503
[379]: https://gitlab.cobolworx.com/COBOLworx/cbl-gdb-vsextension/-/issues/19
[380]: https://github.com/epasveer/seer/issues/153
[381]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206874405/comments/206932889
[382]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206811405/comments/206258985
[383]: https://community.gamedev.tv/t/how-to-do-this-on-vscode/105725
[384]: https://github.com/microsoft/vscode-python-debugger/issues/899
[385]: https://github.com/intersystems-community/vscode-objectscript/issues/1411
[386]: https://github.com/PowerShell/vscode-powershell/issues/1322
[387]: https://www.jetbrains.com/help/rider/2024.1/Run_Debug_Multiple.html
[388]: https://www.jetbrains.com/help/idea/2024.3/run-debug-multiple.html
[389]: https://github.com/microsoft/vscode-python-debugger/issues/139
[390]: https://github.com/microsoft/vscode-python-debugger/issues/617
[391]: https://github.com/microsoft/ptvsd/issues/2103
[392]: https://rider-support.jetbrains.com/hc/en-us/community/posts/360000092204-Step-over-core-framework-library-code-while-debugging
[393]: https://github.com/noir-lang/noir/issues/4904
[394]: https://github.com/mcu-debug/memview/issues/48
[395]: https://community.st.com/stm32cubeide-for-visual-studio-code-mcus-133/about-memory-view-for-debugging-with-stm32-vs-code-160469
[396]: https://community.platformio.org/t/pio-unified-debugger-doesnt-show-peripherals-registers-memory-and-disassembly-views-in-vscode/9640
[397]: https://labs.dese.iisc.ac.in/embeddedlab/?p=208
[398]: https://discuss.tvm.apache.org/t/mixed-mode-debug-of-python-c/2914
[399]: https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2017/python/debugging-mixed-mode-c-cpp-python-in-visual-studio?view=vs-2017
[400]: https://totalview.io/blog/debugging-python-and-c/c-mixed-language-applications
[401]: https://bugreports-test.qt.io/browse/QTCREATORBUG-22150
[402]: https://www.itechguides.com/products/rr/
[403]: https://ar5iv.labs.arxiv.org/html/1705.05937
[404]: https://thestrangeloop.com/2022/time-travel-debugging-javascript-applications.html
[405]: https://ayende.net/blog/2911/the-ideal-ide
[406]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206326759/comments/206857025
[407]: https://wallabyjs.com/blog/time-travel-debugger.html
[408]: https://dev.to/awto/js-ts-debugger-with-time-traveling-persistent-state-api-and-more-5cn2
[409]: https://github.com/microsoft/vscode/issues/143024
[410]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206840535/comments/206322095
[411]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206844595/comments/206871709
[412]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206062729/comments/205962695
[413]: https://devclass.com/2023/12/11/visual-studio-code-gets-floating-editor-windows-but-there-are-oddities/
[414]: https://github.com/microsoft/vscode/issues/10121
[415]: https://devblogs.microsoft.com/visualstudio/?p=256030
[416]: https://beyondjapan.com/en/blog/2018/03/vscode121/
[417]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360000018899/comments/360001789320
[418]: https://rider-support.jetbrains.com/hc/en-us/community/posts/17423252869394/comments/17461124699794
[419]: https://resharper-support.jetbrains.com/hc/en-us/community/posts/206056459/comments/205943565
[420]: https://forum.cursor.com/t/vertical-tabs-support/150737
[421]: https://dev.to/cppshane/vs-code-still-doesnt-have-vertical-tabs-1g3m
[422]: https://dev.to/werliton/declutter-your-dev-environment-a-guide-to-a-clean-vs-code-setup-g5j
[423]: https://github.com/microsoft/vscode-cmake-tools/issues/3423
[424]: https://github.com/dotnet/vscode-csharp/issues/2146
[425]: https://gitlab.com/gitlab-org/gitlab-vscode-extension/-/issues/1791
[426]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/15571730917394/comments/24374553183634
[427]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360000640260/comments/360000118820
[428]: https://kitemetric.com/blogs/vs-code-workspace-folders-enhancing-hierarchical-structure-in-the-explorer
[429]: https://forums.codeblocks.org/index.php/topic,2965.0/wap2.html
[430]: https://bugs.kde.org/show_bug.cgi?id=348034
[431]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206832045/comments/206301895
[432]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206295429/comments/207221289
[433]: https://bugs.edge.launchpad.net/ubuntu/+source/gnome-settings-daemon/+bug/1238410/comments/31
[434]: https://forum.cursor.com/t/menu-bar-is-only-visible-with-custom-title-bar/127540
[435]: https://forum.cursor.com/t/window-title-bar-style-native-not-honored-in-linux/83501
[436]: https://forum.cursor.com/t/menu-bar-bug-on-linux-mint-22-1-cinnamon/68030
[437]: https://forum.cursor.com/t/no-native-titlebar-in-linux-with-window-titlebarstyle-set-to-native/71071
[438]: https://forum.cursor.com/t/ssh-connection-cannot-be-established-due-to-vscode-remote/76812
[439]: https://forum.cursor.com/t/remote-ssh-connection-to-cursor-server-failed-failed-to-install-server-within-the-timeout/112270
[440]: https://learn.microsoft.com/en-us/answers/questions/5651168/why-ssh-connection-to-host-via-vs-code-is-getting
[441]: https://learn.microsoft.com/en-us/answers/questions/5544611/vscode-ssh-password-prompt-issue-with-remote-serve
[442]: https://hpc-discourse.usc.edu/t/cannot-connect-to-server-using-remote-in-vs-code/287
[443]: https://techoverflow.net/2022/10/04/how-i-fixed-visual-studio-code-remote-ssh-waiting-for-server-log-ssh
[444]: https://www.brendangregg.com/flamegraphs.html
[445]: https://www.moderndescartes.com/essays/flamegraphs/
[446]: https://dev.to/nateberkopec/a-trick-for-reading-flamegraphs-5an0
[447]: https://jetbrains.com/help/idea/2021.3/async-profiler.html
[448]: https://learn.microsoft.com/en-gb/windows/dev-environment/docker/dev-containers
[449]: https://endjin.com/blog/supercharge-dev-containers-on-windows
[450]: https://mcuoneclipse.com/tag/container
[451]: https://microsoft.github.io/PyRIT/latest/getting-started/troubleshooting/devcontainers
[452]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360010505180/comments/360002809459
[453]: https://intellij-support.jetbrains.com/hc/en-us/articles/36404619616914
[454]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/207038515/comments/207388399
[455]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/207889145/comments/209317989
[456]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/14806438157842/comments/14807323351442
[457]: https://mail.kde.org/pipermail/kdevelop/2017-June/019441.html
[458]: https://gitlab.gnome.org/GNOME/gnome-builder/issues/1044
[459]: https://qatools.knowledgebase.qt.io/coco/howto/visualstudio-verbosity/
[460]: https://rider-support.jetbrains.com/hc/en-us/community/posts/4406194358802/comments/4406226410898
[461]: https://www.ultimatepp.org/forums/pdf.php?th=11846
[462]: https://github.com/microsoft/vscode/issues/147253
[463]: https://www.refontelearning.com/blog/marimo-vs-jupyter-reactive-notebooks
[464]: https://groups.google.com/g/sympy/c/xKrYfQwyM3Q
[465]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/5969250908178-Disappointing-experience-with-Jupyter-in-Pycharm
[466]: https://devrant.com/rants/2242012/i-freaking-hate-slow-ides-especially-ones-made-in-java-i-used-to-use-an-ide-text
[467]: https://www.capterra.com/p/136010/IntelliJ-IDEA/reviews/?page=15
[468]: https://www.g2.com/survey_responses/intellij-idea-review-4648609
[469]: https://dev.to/waterlink/why-are-some-developers-so-allergic-to-ides-24hl
[470]: https://dev.to/dagnelies/how-was-coding-in-the-last-century-151
[471]: https://news.ycombinator.com/item?id=3595677
[472]: https://dev.to/waterlink/why-are-some-developers-so-allergic-to-ides-24hl/comments
[473]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206326759/comments/206857005
[474]: https://www.g2.com/survey_responses/intellij-idea-review-3889549
[475]: https://lists.puremagic.com/pipermail/digitalmars-d/2012-May/134086.html
[476]: https://kairos.jetbrains.com/research/articles/what-developers-want-and-what-gets-built
[477]: https://redmonk.com/kholterhoff/2025/12/22/10-things-developers-want-from-their-agentic-ides-in-2025/
[478]: https://devecosystem-2025.jetbrains.com/
[479]: https://survey.stackoverflow.co/2025/ai
[480]: https://stackoverflow.com/questions/78163815
[481]: https://forum.freecodecamp.org/t/how-to-disable-auto-complete-in-vs-code/706036
[482]: https://www.codegenes.net/blog/vscode-disable-all-auto-formatting-on-save/
[483]: https://dev.classmethod.jp/articles/the-setting-to-disable-formatting-for-certain-languages-does-not-work-after-updating-prettier-in-vscode/
[484]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/206248039/comments/207064159
[485]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/360007057999/comments/360001572299
[486]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/7122600922258/comments/7123075590674
[487]: https://intellij-support.jetbrains.com/hc/en-us/community/posts/207017425/comments/204250970
[488]: https://www.jetbrains.com/help/idea/2025.3/reformat-and-rearrange-code.html
<!-- docs-check: end quoting -->
