/*
** nsfimport - command line NSF to FamiTracker module converter
**
** Based on NSF Importer v0.5 by Brad Smith (2011), which in turn uses
** NotSo Fatso v0.851 by Disch and parts of FamiTracker 0.3.7 by jsr.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#include "notsofatso/nsf_compat.h"
#include "notsofatso/NSF_Core.h"
#include "notsofatso/NSF_File.h"
#include "ftm/FtmDocument.h"
#include "NSF_Import.h"
#include "M3U.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <cstring>
#include <string>
#include <vector>

#define NSFIMPORT_VERSION "1.3 (based on NSF Importer v0.5)"

static void Usage(FILE* out)
{
    std::fprintf(out,
        "nsfimport " NSFIMPORT_VERSION "\n"
        "Convert an NSF/NSFe file into a FamiTracker module (.ftm). Every track\n"
        "becomes a subsong of one module, sharing instruments and DPCM samples.\n"
        "\n"
        "Usage:\n"
        "  nsfimport [options] <input.nsf> [output.ftm]\n"
        "  nsfimport --info <input.nsf>\n"
        "\n"
        "Options:\n"
        "  -s, --seconds N         maximum length to record per track\n"
        "                          (default: 120, max 3600)\n"
        "  -p, --pattern-length N  rows per pattern, 1-256 (default: 256)\n"
        "  -r, --start-row N       row of the first pattern to start on (default: 0)\n"
        "  -S, --silence N         end a track once the music has been silent for\n"
        "                          N seconds (default: 3; 0 = always record the\n"
        "                          full --seconds)\n"
        "  -w, --wav PREFIX        also write NotSo Fatso's audio output for each track\n"
        "                          to PREFIX_NN.wav (for comparing against the import)\n"
        "  -m, --m3u FILE          read subsong names from this playlist instead of\n"
        "                          looking for .m3u files next to the NSF\n"
        "      --no-m3u            don't read any playlist\n"
        "  -i, --info              print NSF information and track list, then exit\n"
        "  -q, --quiet             only print errors\n"
        "  -h, --help              show this help\n"
        "\n"
        "If no output file is given, the module is written next to the input as\n"
        "<name>.ftm. Subsongs are named from an .m3u playlist (NEZplug format) next\n"
        "to the NSF if there is one, else from NSFe track labels, else \"Track N\".\n"
        "FamiTracker allows at most 64 subsongs per module, so an NSF\n"
        "with more tracks is split into <name>.ftm, <name>_2.ftm, and so on.\n"
        "\n"
        "Output is saved in the same FamiTracker 0.3.7 module format the original\n"
        "importer produced; it opens in FamiTracker 0.3.7+, 0CC-FamiTracker and\n"
        "Dn-FamiTracker. Speed/tempo are set to one row per frame so that every\n"
        "register change is captured.\n");
}

static bool ParseUInt(const char* s, unsigned int& out)
{
    if (!s || !*s) return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s, &end, 10);
    if (*end != '\0' || v > 0xFFFFFFFFul) return false;
    out = (unsigned int)v;
    return true;
}

static std::string StripExtension(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        return path.substr(0, dot);
    return path;
}

static const char* Str(const char* s) { return s ? s : ""; }

static int PrintInfo(const char* path, const std::map<int, std::string>& Names,
                     const std::vector<std::string>& M3UFiles)
{
    NotSoFatso::CNSFFile File;
    if (File.LoadFile(path, 1, 1) != 0) {
        std::fprintf(stderr, "error: could not parse NSF file '%s'\n", path);
        return 1;
    }

    std::string chips;
    const char* names[] = { "VRC6", "VRC7", "FDS", "MMC5", "N163", "Sunsoft 5B" };
    for (int i = 0; i < 6; ++i) {
        if (File.nChipExtensions & (1 << i)) {
            if (!chips.empty()) chips += ", ";
            chips += names[i];
        }
    }

    std::printf("File:       %s (%s)\n", path, File.bIsExtended ? "NSFe" : "NSF");
    std::printf("Title:      %s\n", Str(File.szGameTitle));
    std::printf("Artist:     %s\n", Str(File.szArtist));
    std::printf("Copyright:  %s\n", Str(File.szCopyright));
    if (File.szRipper) std::printf("Ripper:     %s\n", File.szRipper);
    std::printf("Region:     %s\n", File.nIsPal == 1 ? "PAL" : (File.nIsPal >= 2 ? "NTSC/PAL (imported as NTSC)" : "NTSC"));
    std::printf("Expansion:  %s\n", chips.empty() ? "none" : chips.c_str());
    std::printf("Tracks:     %d (initial track %d)\n", File.nTrackCount, File.nInitialTrack + 1);

    if (File.nChipExtensions & 0x30)
        std::printf("Note:       N163 and Sunsoft 5B channels are not imported.\n");
    int chipCount = 0;
    for (int i = 0; i < 4; ++i) if (File.nChipExtensions & (1 << i)) ++chipCount;
    if (chipCount > 1)
        std::printf("Note:       only one expansion chip is imported (priority VRC7 > VRC6 > MMC5 > FDS).\n");

    for (const auto& m : M3UFiles)
        std::printf("Playlist:   %s\n", m.c_str());

    std::printf("\n");
    for (int t = 0; t < File.nTrackCount; ++t) {
        std::printf("  %3d", t + 1);
        if (File.pTrackTime && File.pTrackTime[t] >= 0) {
            int ms = File.pTrackTime[t];
            std::printf("  %d:%02d", ms / 60000, (ms / 1000) % 60);
        }
        auto n = Names.find(t + 1);
        if (n != Names.end())
            std::printf("  %s", n->second.c_str());
        else if (File.szTrackLabels && File.szTrackLabels[t] && File.szTrackLabels[t][0])
            std::printf("  %s", File.szTrackLabels[t]);
        else
            std::printf("  Track %d", t + 1);
        std::printf("\n");
    }
    return 0;
}

int main(int argc, char** argv)
{
    NSF_Import::NSF_Import_Settings Settings;
    bool bInfo = false, bQuiet = false, bNoM3U = false;
    std::string M3UPath;
    std::vector<std::string> Positional;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto NeedValue = [&](unsigned int& out) -> bool {
            if (i + 1 >= argc || !ParseUInt(argv[i + 1], out)) {
                std::fprintf(stderr, "error: %s needs a number\n", a.c_str());
                return false;
            }
            ++i;
            return true;
        };
        unsigned int v = 0;
        if (a == "-h" || a == "--help") { Usage(stdout); return 0; }
        else if (a == "-s" || a == "--seconds") { if (!NeedValue(v)) return 2; Settings.iSeconds = v; }
        else if (a == "-p" || a == "--pattern-length") { if (!NeedValue(v)) return 2; Settings.iPattern = v; }
        else if (a == "-r" || a == "--start-row") { if (!NeedValue(v)) return 2; Settings.iRow = v; }
        else if (a == "-S" || a == "--silence") { if (!NeedValue(v)) return 2; Settings.iSilenceSeconds = v; }
        else if (a == "-w" || a == "--wav") {
            if (i + 1 >= argc) { std::fprintf(stderr, "error: %s needs a filename prefix\n", a.c_str()); return 2; }
            Settings.WavPrefix = StripExtension(argv[++i]);
        }
        else if (a == "-m" || a == "--m3u") {
            if (i + 1 >= argc) { std::fprintf(stderr, "error: %s needs a filename\n", a.c_str()); return 2; }
            M3UPath = argv[++i];
        }
        else if (a == "--no-m3u") bNoM3U = true;
        else if (a == "-i" || a == "--info") bInfo = true;
        else if (a == "-q" || a == "--quiet") bQuiet = true;
        else if (a.size() > 1 && a[0] == '-') { std::fprintf(stderr, "error: unknown option '%s' (see --help)\n", a.c_str()); return 2; }
        else Positional.push_back(a);
    }

    if (Positional.empty() || Positional.size() > 2) { Usage(stderr); return 2; }
    const std::string Input = Positional[0];

    // subsong names from a playlist
    std::vector<std::string> M3UUsed;
    if (!bNoM3U) {
        if (!M3UPath.empty()) {
            if (!M3U::ReadTitles(M3UPath, Input, Settings.TrackNames)) {
                std::fprintf(stderr, "error: could not read playlist '%s'\n", M3UPath.c_str());
                return 1;
            }
            M3UUsed.push_back(M3UPath);
            if (Settings.TrackNames.empty())
                std::fprintf(stderr, "warning: '%s' has no entries for this NSF\n", M3UPath.c_str());
        }
        else M3U::FindTitles(Input, Settings.TrackNames, M3UUsed);
    }

    if (bInfo) return PrintInfo(Input.c_str(), Settings.TrackNames, M3UUsed);

    if (Settings.iSeconds == 0 || Settings.iSeconds > 3600) {
        std::fprintf(stderr, "error: --seconds must be between 1 and 3600\n");
        return 2;
    }
    if (Settings.iSilenceSeconds > 3600) {
        std::fprintf(stderr, "error: --silence must be between 0 and 3600\n");
        return 2;
    }
    if (Settings.iPattern < 1 || Settings.iPattern > 256) {
        std::fprintf(stderr, "error: --pattern-length must be between 1 and 256\n");
        return 2;
    }
    if (Settings.iRow >= Settings.iPattern) {
        std::fprintf(stderr, "error: --start-row must be less than the pattern length\n");
        return 2;
    }

    // how many tracks?
    NotSoFatso::CNSFFile Probe;
    if (Probe.LoadFile(Input.c_str(), 0, 1) != 0) {
        std::fprintf(stderr, "error: could not parse NSF file '%s'\n", Input.c_str());
        return 1;
    }
    const int TrackCount = Probe.nTrackCount;
    Probe.Destroy();
    if (TrackCount < 1) {
        std::fprintf(stderr, "error: '%s' has no tracks\n", Input.c_str());
        return 1;
    }

    // output name(s)
    std::string OutBase = Positional.size() == 2 ? StripExtension(Positional[1]) : StripExtension(Input);
    std::string OutFirst = Positional.size() == 2 ? Positional[1] : OutBase + ".ftm";
    const int Parts = (TrackCount + ftm::MAX_TRACKS - 1) / ftm::MAX_TRACKS;

    if (!bQuiet)
        for (const auto& m : M3UUsed)
            std::printf("names from playlist: %s\n", m.c_str());

    int Failures = 0;
    for (int Part = 0; Part < Parts; ++Part) {
        NSF_Import::NSF_Import_Settings PartSettings = Settings;
        PartSettings.iFirstTrack = Part * ftm::MAX_TRACKS + 1;
        PartSettings.iLastTrack = std::min(TrackCount, (Part + 1) * ftm::MAX_TRACKS);

        std::string Output = OutFirst;
        if (Part > 0) {
            char Suffix[16];
            std::snprintf(Suffix, sizeof(Suffix), "_%d.ftm", Part + 1);
            Output = OutBase + Suffix;
        }

        ftm::Document Doc;
        std::vector<NSF_Import::NSF_Track_Result> Results;
        std::string Error;
        if (!NSF_Import::NSF_Import(Input.c_str(), PartSettings, Doc, Results, Error)) {
            std::fprintf(stderr, "error: %s\n", Error.c_str());
            ++Failures;
            continue;
        }
        if (!Doc.Save(Output, Error)) {
            std::fprintf(stderr, "error: %s\n", Error.c_str());
            ++Failures;
            continue;
        }

        for (const auto& r : Results) {
            if (!bQuiet) {
                const double Rate = Doc.GetMachine() == ftm::PAL ? 50.0 : 60.0;
                const long Tenths = (long)(r.iFramesRecorded * 10.0 / Rate + 0.5);
                std::printf("  track %2d: %2ld:%02ld.%ld  %3u pattern%s  %-11s %s\n",
                    r.iTrack, Tenths / 600, (Tenths / 10) % 60, Tenths % 10,
                    r.iPatterns, r.iPatterns == 1 ? " " : "s",
                    r.bEndedOnSilence ? "(ended)" : (r.bTruncated ? "(cut short)" : "(full len)"),
                    r.Name.c_str());
            }
            if (r.bTruncated)
                std::fprintf(stderr, "warning: track %d ran out of frames (max %d patterns) and was cut short; "
                    "try a shorter --seconds or longer --pattern-length\n", r.iTrack, ftm::MAX_FRAMES);
            if (r.bOutOfInstruments)
                std::fprintf(stderr, "warning: track %d: ran out of instruments (FamiTracker allows %d); "
                    "some FDS/VRC7 sounds use instrument 00 instead\n", r.iTrack, ftm::MAX_INSTRUMENTS);
            if (r.bOutOfSamples)
                std::fprintf(stderr, "warning: track %d: ran out of DPCM sample slots; "
                    "some samples were replaced by note releases\n", r.iTrack);
            if (r.bSyncWarning)
                std::fprintf(stderr, "warning: track %d: the NSF's play routine ran less often than "
                    "once per frame; the import may be incomplete or stretched\n", r.iTrack);
        }
        if (!bQuiet) {
            int Inst = Doc.GetInstrumentCount(), Samp = Doc.GetDSampleCount();
            std::printf("-> %s (%d subsong%s, %d instrument%s, %d DPCM sample%s)\n",
                Output.c_str(), (int)Results.size(), Results.size() == 1 ? "" : "s",
                Inst, Inst == 1 ? "" : "s", Samp, Samp == 1 ? "" : "s");
        }
        if (Parts > 1 && Part + 1 < Parts && !bQuiet)
            std::printf("(more than %d tracks: continuing in another file)\n", ftm::MAX_TRACKS);
    }

    if (!Settings.WavPrefix.empty() && !bQuiet)
        std::printf("audio -> %s_NN.wav\n", Settings.WavPrefix.c_str());

    return Failures ? 1 : 0;
}
