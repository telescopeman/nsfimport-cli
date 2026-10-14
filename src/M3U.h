/*
** nsfimport - command line NSF to FamiTracker module converter
**
** Reads track titles from NEZplug-style .m3u playlists, e.g.
**   Game.nsf::NSF,1,Title Theme,1:30,1:20,7
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#ifndef NSFIMPORT_M3U_H
#define NSFIMPORT_M3U_H

#include <map>
#include <string>
#include <vector>

namespace M3U {

// Parses one playlist and adds titles for entries that refer to NsfPath
// (matched by file name, case-insensitively; entries with no file name match too).
// Track numbers in the map are 1-based. Earlier entries win over later ones.
// Returns false if the file can't be read.
bool ReadTitles(const std::string& M3UPath, const std::string& NsfPath,
                std::map<int, std::string>& Titles);

// Looks for .m3u files next to the NSF that mention it and reads their titles.
// Files that were used are appended to UsedFiles.
void FindTitles(const std::string& NsfPath, std::map<int, std::string>& Titles,
                std::vector<std::string>& UsedFiles);

}

#endif
