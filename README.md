
<div align="center">
  <h1>NES++</h1>
  <p>An NES Emulator written in C++ and partly C#</p>
  <p>The final goal of this emulator is to be able to play games up to atleast MMC3 with a decently functioning CPU, PPU and APU to boot. </p>
  <img src=".github/demo.gif" alt="demonstration"/>
</div>




# Usage
To use the emulator, run NESLauncher.exe to choose a ROM and start playing.

# Controls:

| Key        | Action   |
| ---------- | -------- |
| Arrow Keys | D-Pad    |
| Z          | A Button |
| X          | B Button |
| Enter      | Start    |
| Backspace  | Select   |
| Esc        | Quit     |


# Features
- A Simple Launcher made with WindowsForms in C#
- Config file for settings
- Functioning 6502 CPU Core
- Functioning PPU which supports dot by dot rendering, scrolling and sprites
- A very barebones APU
- Generic USB Controller Support

# Dependencies
- SDL2

# Known Issues
- APU is barebones: only the triangle channel is implemented and the quality isn't optimal.
- Only Mapper 0 (NROM) ROMs are currently supported. MMC1+ support is in progress.
- Balloon Trip mode in Balloon Fight has graphical or logic bugs.

# To Do List
- Finish the APU i.e fix the triangle channel's quality, implement the other channels
- Add save state functionality
- Refactor the CPU's code.
- Organize the code more efficiently in terms of classes in general
- Implement other mappers


