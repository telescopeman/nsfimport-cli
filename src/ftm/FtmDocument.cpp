/*
** nsfimport - command line NSF to FamiTracker module converter
**
** Minimal FamiTracker document model and .ftm writer.
** The block layouts below follow FamiTracker 0.3.7 (CFamiTrackerDoc::WriteBlocks).
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#include "FtmDocument.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ftm {

// ===========================================================================
// Note tables
//
// Identical to CSoundGen::LoadMachineSettings in FamiTracker 0.3.7, which is
// what the importer was written against (C-0 = 32.7032 Hz, A-3 = 440 Hz).

static const double BASE_FREQ_NTSC = 1789773.0;
static const double BASE_FREQ_PAL  = 1662607.0;

NoteTables::NoteTables()
{
    const double BASE_FREQ = 32.7032;
    const double clock_ntsc = BASE_FREQ_NTSC / 16.0;
    const double clock_pal  = BASE_FREQ_PAL / 16.0;

    for (int i = 0; i < NOTE_COUNT; ++i) {
        const double Freq = BASE_FREQ * std::pow(2.0, double(i) / 12.0);
        PAL[i]  = (unsigned int)((clock_pal / Freq) - 0.5);
        NTSC[i] = (unsigned int)((clock_ntsc / Freq) - 0.5);
        Saw[i]  = (unsigned int)(((clock_ntsc * 16.0) / (Freq * 14.0)) - 0.5);
        FDS[i]  = (unsigned int)((Freq * 65536.0) / (clock_ntsc / 4.0) + 0.5); // untransposed (0.3.7)
    }
}

const NoteTables& GetNoteTables()
{
    static const NoteTables Tables;
    return Tables;
}

// ===========================================================================
// Document

static stChanNote BlankNote()
{
    stChanNote Blank;
    std::memset(&Blank, 0, sizeof(Blank));
    Blank.Vol = MAX_VOLUME;
    Blank.Instrument = MAX_INSTRUMENTS;
    return Blank;
}

Document::Document()
{
    SelectExpansionChip(SNDCHIP_NONE);
    m_Tracks.resize(1);
    m_Tracks[0].Name = "New song";
}

int Document::AddTrack(const std::string& Name)
{
    // the first call replaces the default empty track
    if (m_Tracks.size() == 1 && !m_bTrackAdded) {
        m_Tracks[0] = Track();
    }
    else {
        if ((int)m_Tracks.size() >= MAX_TRACKS) return -1;
        m_Tracks.push_back(Track());
    }
    m_bTrackAdded = true;
    m_iCurrentTrack = (int)m_Tracks.size() - 1;
    m_Tracks[m_iCurrentTrack].Name = Name;
    return m_iCurrentTrack;
}

void Document::SelectExpansionChip(int Chip)
{
    m_iExpansionChip = Chip;
    m_vChannelTypes = { 0, 1, 2, 3, 4 }; // 2A03: square 1/2, triangle, noise, DPCM
    switch (Chip) {
    case SNDCHIP_VRC6: m_vChannelTypes.insert(m_vChannelTypes.end(), { 5, 6, 7 }); break;
    case SNDCHIP_MMC5: m_vChannelTypes.insert(m_vChannelTypes.end(), { 8, 9 }); break;
    case SNDCHIP_FDS:  m_vChannelTypes.insert(m_vChannelTypes.end(), { 19 }); break;
    case SNDCHIP_VRC7: m_vChannelTypes.insert(m_vChannelTypes.end(), { 20, 21, 22, 23, 24, 25 }); break;
    default: m_iExpansionChip = SNDCHIP_NONE; break;
    }
}

void Document::SetSongInfo(const char* Name, const char* Artist, const char* Copyright)
{
    m_strName      = Name      ? std::string(Name).substr(0, 31)      : "";
    m_strArtist    = Artist    ? std::string(Artist).substr(0, 31)    : "";
    m_strCopyright = Copyright ? std::string(Copyright).substr(0, 31) : "";
}

void Document::SetDataAtPattern(int Track, int Pattern, int Channel, int Row, const stChanNote* pNote)
{
    if (Track < 0 || Track >= (int)m_Tracks.size() ||
        Pattern < 0 || Pattern >= MAX_PATTERN || Channel < 0 || Channel >= MAX_CHANNELS ||
        Row < 0 || Row >= MAX_PATTERN_LENGTH)
        return;
    std::vector<stChanNote>& Pat = m_Tracks[Track].Patterns[(size_t)Channel * MAX_PATTERN + Pattern];
    if (Pat.empty()) {
        if (IsCellFree(*pNote)) return;
        Pat.assign(MAX_PATTERN_LENGTH, BlankNote());
    }
    Pat[Row] = *pNote;
}

int Document::AddInstrument(const char* Name, int Chip)
{
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        Instrument& Inst = m_Instruments[i];
        if (Inst.Type != INST_NONE) continue;
        Inst = Instrument();
        switch (Chip) {
        case SNDCHIP_VRC6: Inst.Type = INST_VRC6; break;
        case SNDCHIP_VRC7: Inst.Type = INST_VRC7; break;
        case SNDCHIP_FDS:  Inst.Type = INST_FDS;  break;
        default:           Inst.Type = INST_2A03; break; // 2A03 and MMC5
        }
        Inst.Name = Name ? Name : "";
        return i;
    }
    return -1;
}

void Document::ClearFrom(int Track, int Pattern, int Row)
{
    if (Track < 0 || Track >= (int)m_Tracks.size()) return;
    ftm::Track& T = m_Tracks[Track];
    for (int c = 0; c < MAX_CHANNELS; ++c) {
        for (int p = Pattern; p < MAX_PATTERN; ++p) {
            std::vector<stChanNote>& Pat = T.Patterns[(size_t)c * MAX_PATTERN + p];
            if (Pat.empty()) continue;
            if (p > Pattern || Row <= 0) { Pat.clear(); continue; }
            for (int r = Row; r < MAX_PATTERN_LENGTH; ++r) Pat[r] = BlankNote();
        }
    }
}

int Document::GetInstrumentCount() const
{
    int n = 0;
    for (const auto& i : m_Instruments) if (i.Type != INST_NONE) ++n;
    return n;
}

int Document::GetFreeDSample() const
{
    for (int i = 0; i < MAX_DSAMPLES; ++i)
        if (!m_DSamples[i].Used) return i;
    return -1;
}

int Document::GetDSampleCount() const
{
    int n = 0;
    for (const auto& s : m_DSamples) if (s.Used) ++n;
    return n;
}

bool Document::IsCellFree(const stChanNote& n)
{
    if (n.Note != NONE || n.Vol != MAX_VOLUME || n.Instrument != MAX_INSTRUMENTS) return false;
    for (int i = 0; i < MAX_EFFECT_COLUMNS; ++i)
        if (n.EffNumber[i] != EF_NONE) return false;
    return true;
}

// ===========================================================================
// File writer

namespace {

class Block {
public:
    Block(const char* ID, int Version) : m_iVersion(Version) {
        std::memset(m_ID, 0, sizeof(m_ID));
        std::strncpy(m_ID, ID, sizeof(m_ID) - 1);
    }
    void Char(int v) { m_Data.push_back((unsigned char)(v & 0xFF)); }
    void Int(int v) {
        uint32_t u = (uint32_t)v;
        for (int i = 0; i < 4; ++i) m_Data.push_back((unsigned char)(u >> (8 * i)));
    }
    void Bytes(const void* p, size_t n) {
        const unsigned char* b = (const unsigned char*)p;
        m_Data.insert(m_Data.end(), b, b + n);
    }
    void FixedString(const std::string& s, size_t n) {
        for (size_t i = 0; i < n; ++i) Char(i < s.size() ? (unsigned char)s[i] : 0);
    }
    void String(const std::string& s) { Bytes(s.data(), s.size()); Char(0); }

    void WriteTo(std::vector<unsigned char>& Out) const {
        Out.insert(Out.end(), m_ID, m_ID + 16);
        AppendInt(Out, m_iVersion);
        AppendInt(Out, (int)m_Data.size());
        Out.insert(Out.end(), m_Data.begin(), m_Data.end());
    }
    static void AppendInt(std::vector<unsigned char>& Out, int v) {
        uint32_t u = (uint32_t)v;
        for (int i = 0; i < 4; ++i) Out.push_back((unsigned char)(u >> (8 * i)));
    }

private:
    char m_ID[16];
    int m_iVersion;
    std::vector<unsigned char> m_Data;
};

const unsigned int FILE_VER = 0x0420; // FamiTracker 0.3.7
const int SEQ_COUNT = 5;              // volume, arpeggio, pitch, hi-pitch, duty
const int FDS_SEQ_COUNT = 3;          // volume, arpeggio, pitch

} // anonymous namespace

// Writes the same blocks, in the same order and versions, as
// CFamiTrackerDoc::WriteBlocks in FamiTracker 0.3.7.
bool Document::Save(const std::string& Path, std::string& Error) const
{
    std::vector<unsigned char> Out;
    const char* HeaderID = "FamiTracker Module";
    Out.insert(Out.end(), HeaderID, HeaderID + std::strlen(HeaderID));
    Block::AppendInt(Out, (int)FILE_VER);

    const int Channels = GetChannelCount();

    // PARAMS (v4)
    {
        Block b("PARAMS", 4);
        b.Char(m_iExpansionChip);
        b.Int(Channels);
        b.Int((int)m_iMachine);
        b.Int(m_iEngineSpeed);
        b.Int(1);    // vibrato style: VIBRATO_NEW
        b.Int(4);    // first row highlight
        b.Int(16);   // second row highlight
        b.WriteTo(Out);
    }

    // INFO (v1)
    {
        Block b("INFO", 1);
        b.FixedString(m_strName, 32);
        b.FixedString(m_strArtist, 32);
        b.FixedString(m_strCopyright, 32);
        b.WriteTo(Out);
    }

    // HEADER (v3)
    {
        Block b("HEADER", 3);
        b.Char((int)m_Tracks.size() - 1); // number of tracks - 1
        for (const Track& t : m_Tracks)
            b.String(t.Name);
        for (int i = 0; i < Channels; ++i) {
            b.Char(m_vChannelTypes[i]);
            for (const Track& t : m_Tracks)
                b.Char(t.EffectColumns[i]);
        }
        b.WriteTo(Out);
    }

    // INSTRUMENTS (v2, or v4 when FDS instruments are present)
    {
        int Version = 2;
        if (m_iExpansionChip & SNDCHIP_FDS) Version = 4;
        for (const auto& i : m_Instruments) if (i.Type == INST_FDS) Version = 4;

        Block b("INSTRUMENTS", Version);
        b.Int(GetInstrumentCount());
        for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
            const Instrument& Inst = m_Instruments[i];
            if (Inst.Type == INST_NONE) continue;
            b.Int(i);
            b.Char(Inst.Type);
            switch (Inst.Type) {
            case INST_2A03:
                b.Int(SEQ_COUNT);
                for (int s = 0; s < SEQ_COUNT; ++s) { b.Char(0); b.Char(0); } // no sequences
                for (int o = 0; o < OCTAVE_RANGE; ++o) {
                    for (int n = 0; n < NOTE_RANGE; ++n) {
                        b.Char(Inst.SampleIndex[o][n]);
                        b.Char(Inst.SamplePitch[o][n]);
                    }
                }
                break;
            case INST_VRC6:
                b.Int(SEQ_COUNT);
                for (int s = 0; s < SEQ_COUNT; ++s) { b.Char(0); b.Char(0); }
                break;
            case INST_VRC7:
                b.Int(Inst.Patch);
                for (int r = 0; r < 8; ++r) b.Char(Inst.CustomReg[r]);
                break;
            case INST_FDS:
                b.Bytes(Inst.Wave, 64);
                b.Bytes(Inst.Modulation, 32);
                b.Int(0); // modulation speed
                b.Int(0); // modulation depth
                b.Int(0); // modulation delay
                for (int s = 0; s < FDS_SEQ_COUNT; ++s) {
                    b.Char(0);  // item count
                    b.Int(-1);  // loop point
                    b.Int(-1);  // release point
                    b.Int(0);   // setting
                }
                break;
            default:
                break;
            }
            b.Int((int)Inst.Name.size());
            b.Bytes(Inst.Name.data(), Inst.Name.size());
        }
        b.WriteTo(Out);
    }

    // SEQUENCES (v6) - none used
    {
        Block b("SEQUENCES", 6);
        b.Int(0);
        b.WriteTo(Out);
    }

    // FRAMES (v3)
    {
        Block b("FRAMES", 3);
        for (const Track& t : m_Tracks) {
            b.Int(t.FrameCount);
            b.Int(t.Speed);
            b.Int(t.Tempo);
            b.Int(t.PatternLength);
            for (int f = 0; f < t.FrameCount; ++f)
                for (int c = 0; c < Channels; ++c)
                    b.Char(t.FrameList[f][c]);
        }
        b.WriteTo(Out);
    }

    // PATTERNS (v4)
    {
        Block b("PATTERNS", 4);
        for (int t = 0; t < (int)m_Tracks.size(); ++t) {
            const Track& Trk = m_Tracks[t];
            for (int c = 0; c < Channels; ++c) {
                const int EffColumns = Trk.EffectColumns[c] + 1;
                for (int p = 0; p < MAX_PATTERN; ++p) {
                    const std::vector<stChanNote>& Pat = Trk.Patterns[(size_t)c * MAX_PATTERN + p];
                    if (Pat.empty()) continue;
                    int Items = 0;
                    for (int r = 0; r < MAX_PATTERN_LENGTH; ++r)
                        if (!IsCellFree(Pat[r])) ++Items;
                    if (!Items) continue;
                    b.Int(t);
                    b.Int(c);
                    b.Int(p);
                    b.Int(Items);
                    for (int r = 0; r < MAX_PATTERN_LENGTH; ++r) {
                        const stChanNote& n = Pat[r];
                        if (IsCellFree(n)) continue;
                        b.Int(r);
                        b.Char(n.Note);
                        b.Char(n.Octave);
                        b.Char(n.Instrument);
                        b.Char(n.Vol);
                        for (int e = 0; e < EffColumns; ++e) {
                            b.Char(n.EffNumber[e]);
                            b.Char(n.EffParam[e]);
                        }
                    }
                }
            }
        }
        b.WriteTo(Out);
    }

    // DPCM SAMPLES (v1)
    {
        Block b("DPCM SAMPLES", 1);
        b.Char(GetDSampleCount());
        for (int i = 0; i < MAX_DSAMPLES; ++i) {
            const DSample& s = m_DSamples[i];
            if (!s.Used) continue;
            b.Char(i);
            b.Int((int)s.Name.size());
            b.Bytes(s.Name.data(), s.Name.size());
            b.Int((int)s.Data.size());
            b.Bytes(s.Data.data(), s.Data.size());
        }
        b.WriteTo(Out);
    }

    // SEQUENCES_VRC6 (v6) - none used
    if (m_iExpansionChip & SNDCHIP_VRC6) {
        Block b("SEQUENCES_VRC6", 6);
        b.Int(0);
        b.WriteTo(Out);
    }

    Out.insert(Out.end(), { 'E', 'N', 'D' });

    FILE* f = std::fopen(Path.c_str(), "wb");
    if (!f) {
        Error = "could not open '" + Path + "' for writing";
        return false;
    }
    bool ok = std::fwrite(Out.data(), 1, Out.size(), f) == Out.size();
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) Error = "error writing '" + Path + "'";
    return ok;
}

} // namespace ftm
