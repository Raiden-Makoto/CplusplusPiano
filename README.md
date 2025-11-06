# CplusplusPiano

A low-latency virtual piano application built with C++ and Qt, featuring real-time audio playback using macOS Core Audio.

## Features

- 🎹 **3 Octaves** (C3 to C6) with realistic white and black key layout
- ⌨️ **Keyboard Mapping** - Play notes using your computer keyboard
- 🎵 **Low-Latency Audio** - Ultra-low latency audio playback using macOS Core Audio (AudioUnit)
- 🎚️ **Una Corda (Soft Pedal)** - Press `N` to activate soft pedal (21% volume reduction + muffled tone)
- 🎛️ **Damper Pedal (Sustain)** - Press `M` to sustain notes with fade-out effect
- ✨ **Visual Feedback** - Keys highlight when pressed with smooth fade animation
- 🖱️ **Mouse Support** - Click keys with your mouse to play notes

## Requirements

- macOS (uses Core Audio frameworks)
- Qt 6.x (core, widgets modules)
- C++17 compiler
- qmake and make

## Building

### Prerequisites

Install Qt via Homebrew:
```bash
brew install qt
```

### Build and Run

Use the provided script:
```bash
./run.sh
```

Or manually:
```bash
qmake CplusplusPiano.pro
make
./build/CplusplusPiano.app/Contents/MacOS/CplusplusPiano
```

## Controls

### Keyboard Keybindings

Each keyboard row represents one octave:

**First Octave (C3-B3) - Number Row:**
- `1` = C, `2` = C#, `3` = D, `4` = D#, `5` = E
- `6` = F, `7` = F#, `8` = G, `9` = G#, `0` = A
- `-` = A#, `=` = B

**Second Octave (C4-B4) - QWERT Row:**
- `q` = C, `w` = C#, `e` = D, `r` = D#, `t` = E
- `y` = F, `u` = F#, `i` = G, `o` = G#, `p` = A
- `[` = A#, `]` = B

**Third Octave (C5-B5) - ASDFG Row:**
- `a` = C, `s` = C#, `d` = D, `f` = D#, `g` = E
- `h` = F, `j` = F#, `k` = G, `l` = G#, `;` = A
- `'` = A#, `\` = B

**Fourth Octave (C6):**
- `z` = C6

### Pedals

- `N` - Una Corda (Soft Pedal) - Hold to reduce volume and apply muffled tone
- `M` - Damper Pedal (Sustain) - Hold to sustain notes

### Other

- `ESC` - Quit the application

## Technical Details

- **Audio Engine**: macOS Core Audio (AudioUnit) for minimal latency
- **Audio Format**: 44.1kHz, 16-bit, stereo WAV files
- **Mixing**: Real-time software mixing in audio callback
- **Thread Safety**: Lock-free pending notes queue for rapid key presses
- **Sample Rate Conversion**: Linear interpolation for mismatched sample rates
- **Effects**: 
  - Low-pass filter for una corda (soft pedal) effect
  - Volume decay for damper pedal sustain

## Project Structure

```
CplusplusPiano/
├── src/
│   ├── main.cpp              # Application entry point
│   ├── mainwindow.h          # Main window class declaration
│   ├── mainwindow.cpp        # Main window implementation
│   └── NotesFF/              # WAV audio samples for each note
├── build/                    # Build output directory
├── CplusplusPiano.pro        # Qt project file
├── run.sh                    # Build and run script
└── README.md                 # This file
```

## License

See LICENSE file for details.
