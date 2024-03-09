# Portal 2 (ASW) in CSGO

This project is aimed to port P2ASW to the leaked CSGO engine to no longer worry about Alien Swarm's limitations.<br>
Partially used code from https://github.com/tupoy-ya/Kisak-Strike and my other project, https://github.com/EpicSentry/HL2-CSGO<br>
What is P2ASW? Check it out here: https://github.com/EpicSentry/P2ASW/<br>
This repository uses Git LFS for libraries and the platform folder.<br>

**Features:**
- Removed Scaleform.
- Portal 2 GameUI (almost) fully functional.
- CSGO Usermessage system, everything now makes use of the CUsrMsg class.
- VPhysics DLL code included.
- Mostly working save/restore.
- Valve Bink video DLL compiling from source.

**Differences/stuff removed in P2GO:**<br>
- SourceTV/GOTV removed (Not important for Portal 2)<br>
- Do a search for "TODO!", these are things that are important that I couldn't get working or that cause crashes.
- Portal renderer is commented out because of crashes. (see viewrender.cpp)

**Using and building P2GO:**
- Generate the project using CreateSolution.bat and build with VS2015 (2022 with 2015 build tools will work).<br>
- Copy over all of your Portal 2 folder to the Game folder, excluding `platform` and any `bin` folders.<br>
- Copy everything from `copy these to portal2` into your newly copied over `portal2` folder, overwriting if necessary.<br>
- Now you can play the game!<br>
