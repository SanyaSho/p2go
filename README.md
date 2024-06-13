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

**Using and building P2GO:**
- Generate the project using CreateSolution.bat and build with VS2015 (2022 with 2015 build tools will work).<br>
- **VS 2022 users:** Do NOT upgrade the solution if prompted to! Hit cancel on the upgrade dialog box.<br>
- Copy over all of your Portal 2 folder to the Game folder, excluding `platform` and any `bin` folders.<br>
- Copy everything from `copy these to portal2` into your newly copied over `portal2` folder, overwriting if necessary.<br>
- Now you can play the game!<br>

# Troubleshooting compiling
Having problems building the project? Make sure you have the following:<br>
**For VS 2022 users:**
- Windows 8.1 SDK: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/<br>
- MSVC 140 (2015 Build tools): Available under the "Individual Components" section of the Visual Studio Installer.<br>
- If VPC complains about a regkey for .vcxproj files in solutions, please run .reg file included in this repository and run VPC again.<br>
VS 2015 should work out of the box with no additional changes necessary.<br>

# Debugging the engine
- Set launcher_main as the startup project (if it isn't already) by right clicking it and pressing "Set as Startup Project".
- Right click launcher_main, go to properties and click on the debugging section. Set "Command" to point to your compiled portal2.exe (in the game folder).
- Set "Command Arguments" to "-game "portal2" -insecure -sw" (feel free to add more such as +sv_cheats 1).
- Press "Local Windows Debugger" at the top of Visual Studio to then launch the game and debug it.
![image](https://github.com/EpicSentry/p2go/assets/82910317/a4648027-3309-4e14-b21c-83b2637bfcfc)<br>
Above is an example of a correctly set up debugger.<br>
