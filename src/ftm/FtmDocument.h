/*
** nsfimport - command line NSF to FamiTracker module converter
**
** Minimal FamiTracker document model and .ftm writer.
**
** This replaces the parts of FamiTracker 0.3.7 (CFamiTrackerDoc, CInstrument*,
** CDSample, CSoundGen note tables) that the NSF importer used. Files are written
** exactly as FamiTracker 0.3.7 saved them (file version 0x0420), which is what
** the original NSF Importer produced. Later versions (0.4.x, 0.5, 0CC-FamiTracker,
** Dn-FamiTracker) all open these files.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#ifndef FTM_DOCUMENT_H
#define FTM_DOCUMENT_H

#include <cstdint>
#include <string>
#include <vector>

namespace ftm {

// ---------------------------------------------------------------------------
// Limits (FamiTracker 0.3.7 values)

const int MAX_TRACKS         = 64;
const int MAX_INSTRUMENTS    = 64;
const int MAX_EFFECT_COLUMNS = 4;
const int MAX_FRAMES         = 128;
const int MAX_PATTERN        = 128;
const int MAX_PATTERN_LENGTH = 256;
const int MAX_DSAMPLES       = 64;
const int MAX_CHANNELS       = 11;   // 2A03 + the largest single expansion (VRC7)
const int OCTAVE_RANGE       = 8;
const int NOTE_RANGE         = 12;
const int NOTE_COUNT         = OCTAVE_RANGE * NOTE_RANGE;
const int MAX_VOLUME         = 0x10; // "no volume" marker in a pattern cell

// ---------------------------------------------------------------------------
// Pattern data

enum note_t {
    NONE = 0,
    C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B,
    HALT,     // "---" note cut
    RELEASE   // "===" note release
};

// Effect numbers as stored in .ftm files (stable since FamiTracker 0.3.x).
enum effect_t {
    EF_NONE = 0,
    EF_SPEED,             // Fxx
    EF_JUMP,              // Bxx
    EF_SKIP,              // Dxx
    EF_HALT,              // Cxx
    EF_VOLUME,            // Exx
    EF_PORTAMENTO,        // 3xx
    EF_PORTAOFF,          // unused
    EF_SWEEPUP,           // Hxy
    EF_SWEEPDOWN,         // Ixy
    EF_ARPEGGIO,          // 0xy
    EF_VIBRATO,           // 4xy
    EF_TREMOLO,           // 7xy
    EF_PITCH,             // Pxx
    EF_DELAY,             // Gxx
    EF_DAC,               // Zxx
    EF_PORTA_UP,          // 1xx
    EF_PORTA_DOWN,        // 2xx
    EF_DUTY_CYCLE,        // Vxx
    EF_SAMPLE_OFFSET,     // Yxx
    EF_SLIDE_UP,          // Qxy
    EF_SLIDE_DOWN,        // Rxy
    EF_VOLUME_SLIDE,      // Axy
    EF_NOTE_CUT,          // Sxx
    EF_RETRIGGER,         // Xxx
    EF_DELAYED_VOLUME,    // Mxy
    EF_FDS_MOD_DEPTH,     // Hxx (FDS)
    EF_FDS_MOD_SPEED_HI,  // Ixx (FDS)
    EF_FDS_MOD_SPEED_LO,  // Jxx (FDS)
    EF_DPCM_PITCH         // Wxx
};

struct stChanNote {
    unsigned char Note;
    unsigned char Octave;
    unsigned char Vol;
    unsigned char Instrument;
    unsigned char EffNumber[MAX_EFFECT_COLUMNS];
    unsigned char EffParam[MAX_EFFECT_COLUMNS];
};

// ---------------------------------------------------------------------------
// Machine / expansion chips

enum machine_t { NTSC = 0, PAL = 1 };

enum {
    SNDCHIP_NONE = 0x00,
    SNDCHIP_VRC6 = 0x01,
    SNDCHIP_VRC7 = 0x02,
    SNDCHIP_FDS  = 0x04,
    SNDCHIP_MMC5 = 0x08,
    SNDCHIP_N163 = 0x10,
    SNDCHIP_S5B  = 0x20
};

enum inst_type_t {
    INST_NONE = 0,
    INST_2A03 = 1,
    INST_VRC6 = 2,
    INST_VRC7 = 3,
    INST_FDS  = 4
};

// ---------------------------------------------------------------------------
// Instruments (one struct covering the instrument types the importer creates)

struct Instrument {
    inst_type_t Type = INST_NONE;
    std::string Name;

    // 2A03 / MMC5: DPCM key map
    unsigned char SampleIndex[OCTAVE_RANGE][NOTE_RANGE] = {}; // 0 = none, n = DPCM sample n-1
    unsigned char SamplePitch[OCTAVE_RANGE][NOTE_RANGE] = {}; // bit 7 = loop, bits 0-3 = rate

    void SetSample(int Octave, int Note, int Sample) { SampleIndex[Octave][Note] = (unsigned char)Sample; }
    void SetSamplePitch(int Octave, int Note, int Pitch) {
        SamplePitch[Octave][Note] = (unsigned char)((SamplePitch[Octave][Note] & 0x80) | (Pitch & 0x0F));
    }
    void SetSampleLoop(int Octave, int Note, bool Loop) {
        SamplePitch[Octave][Note] = (unsigned char)((SamplePitch[Octave][Note] & 0x0F) | (Loop ? 0x80 : 0));
    }
    void SetSampleLoopOffset(int, int, int) {} // not saved in .ftm files by 0.3.7

    // FDS
    unsigned char Wave[64] = {};
    unsigned char Modulation[32] = {};
    void SetWaveSample(int i, int v) { Wave[i] = (unsigned char)v; }
    void SetModulation(int i, int v) { Modulation[i] = (unsigned char)v; }

    // VRC7
    int Patch = 0;
    unsigned char CustomReg[8] = {};
    void SetPatch(int p) { Patch = p; }
    void SetCustomReg(int i, int v) { CustomReg[i] = (unsigned char)v; }
};

struct DSample {
    std::string Name;
    std::vector<unsigned char> Data;
    bool Used = false;
};

// ---------------------------------------------------------------------------
// Note period tables (equivalent to CSoundGen's lookup tables)

struct NoteTables {
    unsigned int NTSC[NOTE_COUNT];
    unsigned int PAL[NOTE_COUNT];
    unsigned int Saw[NOTE_COUNT];
    unsigned int FDS[NOTE_COUNT];
    NoteTables();
};

const NoteTables& GetNoteTables();

// ---------------------------------------------------------------------------
// A track (subsong): frames, patterns and per-track settings

struct Track {
    std::string Name;
    int Speed = 6;
    int Tempo = 150;
    int PatternLength = 64;
    int FrameCount = 1;
    unsigned char FrameList[MAX_FRAMES][MAX_CHANNELS] = {};
    int EffectColumns[MAX_CHANNELS] = {};
    // [channel * MAX_PATTERN + pattern] -> MAX_PATTERN_LENGTH rows, allocated on first write
    std::vector<std::vector<stChanNote>> Patterns =
        std::vector<std::vector<stChanNote>>((size_t)MAX_CHANNELS * MAX_PATTERN);
};

// ---------------------------------------------------------------------------
// Document (one module, up to MAX_TRACKS subsongs)

class Document {
public:
    Document();

    // module settings
    void SelectExpansionChip(int Chip);
    int  GetExpansionChip() const { return m_iExpansionChip; }
    void SetMachine(machine_t m) { m_iMachine = m; }
    machine_t GetMachine() const { return m_iMachine; }
    void SetEngineSpeed(int s) { m_iEngineSpeed = s; }
    void SetSongInfo(const char* Name, const char* Artist, const char* Copyright);
    int  GetChannelCount() const { return (int)m_vChannelTypes.size(); }

    // tracks: AddTrack creates a new subsong and makes it the current one;
    // the song settings below apply to the current track
    int  AddTrack(const std::string& Name); // returns index, or -1 if full
    int  GetTrackCount() const { return (int)m_Tracks.size(); }
    int  GetCurrentTrack() const { return m_iCurrentTrack; }

    // song settings (current track)
    void SetSongSpeed(int s) { Cur().Speed = s; }
    void SetSongTempo(int t) { Cur().Tempo = t; }
    void SetPatternLength(int l) { Cur().PatternLength = l; }
    void SetFrameCount(int c) { Cur().FrameCount = c; }
    int  GetFrameCount() const { return m_Tracks[m_iCurrentTrack].FrameCount; }
    void SetPatternAtFrame(int Frame, int Channel, int Pattern) { Cur().FrameList[Frame][Channel] = (unsigned char)Pattern; }
    void SetEffColumns(int Channel, int Columns) { Cur().EffectColumns[Channel] = Columns; } // 0 = one column
    void SetDataAtPattern(int Track, int Pattern, int Channel, int Row, const stChanNote* pNote);
    // erase every channel's data from (Pattern, Row) onward, including all later patterns
    void ClearFrom(int Track, int Pattern, int Row);

    // instruments
    int  AddInstrument(const char* Name, int Chip); // returns index, or -1 if full
    Instrument* GetInstrument(int Index) { return m_Instruments[Index].Type == INST_NONE ? nullptr : &m_Instruments[Index]; }
    int  GetInstrumentCount() const;

    // DPCM samples
    int  GetFreeDSample() const;                   // returns index, or -1 if full
    DSample* GetDSample(int Index) { return &m_DSamples[Index]; }
    int  GetDSampleCount() const;

    // file output
    bool Save(const std::string& Path, std::string& Error) const;

private:
    static bool IsCellFree(const stChanNote& n);
    Track& Cur() { return m_Tracks[m_iCurrentTrack]; }

    int m_iExpansionChip = SNDCHIP_NONE;
    machine_t m_iMachine = NTSC;
    int m_iEngineSpeed = 0;
    std::string m_strName, m_strArtist, m_strCopyright;
    std::vector<unsigned char> m_vChannelTypes;

    std::vector<Track> m_Tracks;
    int m_iCurrentTrack = 0;
    bool m_bTrackAdded = false;

    Instrument m_Instruments[MAX_INSTRUMENTS];
    DSample m_DSamples[MAX_DSAMPLES];
};

} // namespace ftm

#endif // FTM_DOCUMENT_H
