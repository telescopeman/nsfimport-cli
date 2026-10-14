# nsfimport

A command-line version of Brad Smith's **NSF Importer v0.5**, which turns NSF/NSFe music into an editable FamiTracker module (`.ftm`). Every track in the NSF becomes a subsong of one module, and the subsongs share instruments and DPCM samples.

The original importer was a modified copy of FamiTracker 0.3.7 for Windows only. You had to open the program, pick "Import NSF..." from a menu, fill in a dialog, and then save. This version keeps the conversion code and drops the rest of FamiTracker. You give it a file and it writes an `.ftm`. It builds on macOS, Linux and Windows.

## Building

You need a C/C++ compiler. On macOS, install the Xcode command line tools (`xcode-select --install`).

```sh
make
```

This produces `./nsfimport`. You can also build with CMake:

```sh
cmake -S . -B build && cmake --build build
```

## Usage

```
nsfimport [options] <input.nsf> [output.ftm]
nsfimport --info <input.nsf>

  -s, --seconds N         maximum length to record per track (default: 120, max 3600)
  -S, --silence N         end a track once the music has been silent for N seconds
                          (default: 3; 0 = always record the full --seconds)
  -p, --pattern-length N  rows per pattern, 1-256 (default: 256)
  -r, --start-row N       row of the first pattern to start on (default: 0)
  -w, --wav PREFIX        also write NotSo Fatso's audio for each track to
                          PREFIX_NN.wav (for comparing against the import)
  -m, --m3u FILE          read subsong names from this playlist instead of
                          looking for .m3u files next to the NSF
      --no-m3u            don't read any playlist
  -i, --info              print NSF information and track list, then exit
  -q, --quiet             only print errors
```

Examples:

```sh
nsfimport --info "Mega Man 2.nsf"        # title, chips, track list
nsfimport "Mega Man 2.nsf"               # -> Mega Man 2.ftm, one subsong per track
nsfimport -s 60 game.nsf music.ftm       # at most 60 seconds per track, custom output name
nsfimport -S 5 game.nsf                  # only end a track after 5 seconds of silence
nsfimport -S 0 game.nsf                  # never stop early (the old behavior)
nsfimport -s 30 -w audio/game game.nsf   # also writes audio/game_01.wav, _02.wav, ...
```

**Subsongs:**
* **Subsong names:** the importer takes each name from the first source that has one:
  1. An `.m3u` playlist in the same folder as the NSF that lists it. This is the NEZplug format that comes with most NSF sets, e.g. `Game.nsf::NSF,1,Title Theme,1:30,...`. Decimal track numbers are 1-based and `$hex` ones are 0-based, the same as Game Music Emu. Every `.m3u` next to the NSF is checked, and lines for other files are ignored. Use `--m3u FILE` to choose one, or `--no-m3u` to skip playlists.
  2. The NSFe's own track labels.
  3. "Track N".

  `--info` shows the names it will use. Subsongs stay in NSF track order, not playlist order.
* FamiTracker allows at most 64 subsongs per module. An NSF with more tracks is split into `name.ftm` (tracks 1-64), `name_2.ftm` (tracks 65-128), and so on.
* Instruments are capped at 64 per module and DPCM sample slots at 96. If either runs out, which only happens with lots of FDS wavetables or VRC7 patches, or lots of samples, you get a warning naming the track.

## Output

* Files are written exactly as FamiTracker 0.3.7 saved them (module version 0x0420), which is what the original importer produced. They open in FamiTracker 0.3.7 and later, 0CC-FamiTracker and Dn-FamiTracker. Newer trackers convert the file when they load it, so save it again from there if you want the newer format.
* Speed is 1 and tempo is 150 (125 for PAL), so you get one row per frame and every register change is captured.
* Each subsong has at most 128 patterns. With 256-row patterns that is about 9 minutes of NTSC music. If a track runs past that, the import stops early and you get a warning.
* **Ending on silence:** a track stops as soon as the emulator's output has been completely silent for `--silence` seconds (3 by default). The track is cut right after the last sound, and a `C00` (halt) is placed on the next row, in the DPCM channel's second effect column, so FamiTracker stops there instead of looping. This takes care of jingles, fanfares and sound-effect tracks.
* Pauses inside a song shorter than `--silence` are kept. If a song has a longer deliberate pause, raise `--silence`.
* Songs that loop forever never go silent, so they're recorded for the full `--seconds` (120 by default). The per-track summary shows `(ended)` or `(full len)` for each track.

## How it works (unchanged from v0.5)

The NSF plays inside the NotSo Fatso emulator. Once per frame, just before the play routine runs, the importer reads the emulated sound registers and turns them into pattern data:

* **Notes** are the closest note in FamiTracker's period table, with `Pxx` for the remaining fine pitch.
* **Volume** goes in the volume column. Duty cycle uses `Vxx`, hardware sweep uses `Hxy`/`Ixy`, and direct writes to `$4011` use `Zxx`.
* **DPCM** samples are pulled from memory into the 2A03 instrument's sample map. Sample offsets use `Yxx`.
* **FDS** wavetables and modulation tables each become an instrument, with `Hxx`/`Ixx`/`Jxx` for modulation.
* **VRC7** custom patches each become an instrument. `3FF` is used for pitch and patch changes without retriggering the note.
* **VRC6** and **MMC5** are supported. N163 and Sunsoft 5B are not. Only one expansion chip is imported, with priority VRC7 > VRC6 > MMC5 > FDS.

## Known limitations (from the original importer)

* **Hardware length counters** are rounded down to whole frames. Very short sounds, such as some hi-hats or triangle blips, can be missed.
* **Hardware volume envelopes** are sampled once per frame.
* **DPCM:** loop or length changes after a sample starts are ignored. Raw PCM playback shows up as a stream of `Zxx` commands.
* **VRC6 sawtooth** volume loses its lowest bit.
* **FDS modulation** can sound different in FamiTracker than in other emulators. This is a difference in playback, not an import bug.

## What changed from the GUI version

The conversion logic in `src/NSF_Import.cpp` is Brad Smith's code with the FamiTracker and Windows plumbing replaced. Specifically:

* `src/ftm/` is a small FamiTracker 0.3.7 document model and `.ftm` writer. It replaces `CFamiTrackerDoc`, the instrument classes and the `CSoundGen` note tables, which use the same formulas as 0.3.7.
* The settings dialog, the message boxes and the "frame counter out of sync" prompt are now command-line options, error messages and a warning.
* Instead of importing one track into a fresh document, the importer imports every track into one module. Each track gets its own subsong and its own clean emulator, as before. FDS, VRC7 and DPCM instruments are de-duplicated across the whole module.
* Reusing an existing DPCM sample now also checks that the sample bytes match. The same address can hold different data after a bank switch or in another track, and before this check that could pick the wrong sample.
* `src/notsofatso/nsf_compat.h` stands in for `<windows.h>` so NotSo Fatso builds on other platforms.
* Portability fixes:
  * Branch offsets now use `signed char`, which matters on ARM.
  * `fmopl.h` uses fixed-width integer types, so it works on 64-bit systems.
  * Fixed an off-by-one table overflow in `fmopl.c`.
  * DPCM sample extraction now wraps across 4 KB banks the same way the DMC does, instead of reading past the bank.
* The debug WAV header was corrected. It previously had the wrong block-align field and declared the data size up front.

The quirks of the import itself, such as which repeated notes get hidden and how VRC7 releases are handled, were left exactly as they were.

## Credits and license

* NSF Importer, by Brad Smith (2011)
* NotSo Fatso 0.851, by Disch
* FamiTracker 0.3.7, by jsr (used for the module format and note tables)
* fmopl, by Tatsuyuki Satoh, Ben Parnell, Xodnizel and Disch

GPL v2 or later, inherited from all of the above. See `LICENSE`.
