/*
** NSF Importer
** Copyright (C) 2011 Brad Smith (rainwarrior@gmail.com)
** Command line adaptation for nsfimport.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#ifndef NSF_IMPORT_H
#define NSF_IMPORT_H

#include <map>
#include <string>
#include <vector>

namespace ftm { class Document; }

namespace NSF_Import {

// Replaces the settings dialog of the original importer.
struct NSF_Import_Settings
{
    int iFirstTrack = 1;          // 1-based range of tracks to import
    int iLastTrack = 0;           // 0 = through the last track
    unsigned int iSeconds = 120;  // how long to record each track
    unsigned int iPattern = 256;  // pattern length (rows)
    unsigned int iRow = 0;        // starting row in the first pattern
    unsigned int iSilenceSeconds = 3; // end a track after this much silence (0 = never)
    std::string WavPrefix;        // if non-empty, write NotSo Fatso's audio to <prefix>_NN.wav
    std::map<int, std::string> TrackNames; // subsong names by 1-based track (e.g. from an .m3u)
};

struct NSF_Track_Result
{
    int iTrack = 0;                   // NSF track number (1-based)
    std::string Name;                 // subsong name used in the module
    unsigned int iFramesRecorded = 0; // rows written
    unsigned int iPatterns = 0;       // patterns/frames used
    bool bTruncated = false;          // ran out of frames (128 patterns)
    bool bSyncWarning = false;        // NSF play routine fired less often than expected
    bool bOutOfInstruments = false;   // FDS/VRC7 instrument couldn't be created
    bool bOutOfSamples = false;       // DPCM sample couldn't be created
    bool bEndedOnSilence = false;     // track was ended (with Cxx) because the music went silent
};

// Plays every track in the range and puts each one into Doc as a subsong,
// sharing instruments and DPCM samples between them.
// Returns false and sets Error on failure.
bool NSF_Import(const char* FileName, const NSF_Import_Settings& Settings,
                ftm::Document& Doc, std::vector<NSF_Track_Result>& Results, std::string& Error);

// Called by the NotSo Fatso core just before each vsync (play routine call).
void NSF_Import_Vsync();

}

#endif
