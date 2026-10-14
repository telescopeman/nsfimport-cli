/*
** nsfimport - command line NSF to FamiTracker module converter
**
** NEZplug extended .m3u playlist reader (track titles only).
**
** Line format:  filename::NSF,track,title,time,loop,fade,loopcount
**  - track is decimal (1-based) or $hex (0-based), following Game Music Emu
**  - commas inside the title are escaped as "\,"
**  - lines starting with '#' are comments
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
*/

#include "M3U.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace M3U {

static std::string Lower(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string BaseName(const std::string& Path)
{
    size_t slash = Path.find_last_of("/\\");
    return slash == std::string::npos ? Path : Path.substr(slash + 1);
}

static std::string DirName(const std::string& Path)
{
    size_t slash = Path.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : Path.substr(0, slash);
}

static std::string Trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// splits on unescaped commas, removing the escapes
static std::vector<std::string> SplitFields(const std::string& s)
{
    std::vector<std::string> Fields(1);
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) { Fields.back() += s[++i]; continue; }
        if (c == ',') { Fields.emplace_back(); continue; }
        Fields.back() += c;
    }
    return Fields;
}

// returns 1-based track number, or 0 if it can't be parsed
static int ParseTrack(const std::string& Field)
{
    std::string f = Trim(Field);
    if (f.empty()) return 0;
    char* End = nullptr;
    long v;
    if (f[0] == '$') {
        v = std::strtol(f.c_str() + 1, &End, 16) + 1; // hex is 0-based
        if (End == f.c_str() + 1) return 0;
    }
    else {
        v = std::strtol(f.c_str(), &End, 10);         // decimal is 1-based
        if (End == f.c_str()) return 0;
    }
    return (v >= 1 && v <= 256) ? (int)v : 0;
}

bool ReadTitles(const std::string& M3UPath, const std::string& NsfPath,
                std::map<int, std::string>& Titles)
{
    FILE* f = std::fopen(M3UPath.c_str(), "rb");
    if (!f) return false;
    std::string Data;
    char Buf[4096];
    size_t n;
    while ((n = std::fread(Buf, 1, sizeof(Buf), f)) > 0) Data.append(Buf, n);
    std::fclose(f);

    // skip a UTF-8 byte order mark
    if (Data.size() >= 3 && (unsigned char)Data[0] == 0xEF &&
        (unsigned char)Data[1] == 0xBB && (unsigned char)Data[2] == 0xBF)
        Data.erase(0, 3);

    const std::string NsfName = Lower(BaseName(NsfPath));
    size_t Pos = 0;
    while (Pos < Data.size()) {
        size_t Eol = Data.find_first_of("\r\n", Pos);
        if (Eol == std::string::npos) Eol = Data.size();
        std::string Line = Trim(Data.substr(Pos, Eol - Pos));
        Pos = Eol + 1;

        if (Line.empty() || Line[0] == '#') continue;
        size_t Sep = Line.find("::");
        if (Sep == std::string::npos) continue;

        std::string File = Trim(Line.substr(0, Sep));
        if (!File.empty() && Lower(BaseName(File)) != NsfName) continue;

        std::vector<std::string> Fields = SplitFields(Line.substr(Sep + 2));
        if (Fields.size() < 3) continue;
        if (Lower(Trim(Fields[0])).compare(0, 3, "nsf") != 0) continue; // NSF or NSFE

        int Track = ParseTrack(Fields[1]);
        std::string Title = Trim(Fields[2]);
        if (Track && !Title.empty() && !Titles.count(Track))
            Titles[Track] = Title;
    }
    return true;
}

static std::vector<std::string> ListM3UFiles(const std::string& Dir)
{
    std::vector<std::string> Files;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((Dir + "\\*.m3u").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                Files.push_back(Dir + "\\" + fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    if (DIR* d = opendir(Dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string Name = e->d_name;
            if (Name.size() > 4 && Lower(Name.substr(Name.size() - 4)) == ".m3u" && Name[0] != '.')
                Files.push_back(Dir + "/" + Name);
        }
        closedir(d);
    }
#endif
    std::sort(Files.begin(), Files.end());
    return Files;
}

void FindTitles(const std::string& NsfPath, std::map<int, std::string>& Titles,
                std::vector<std::string>& UsedFiles)
{
    for (const std::string& File : ListM3UFiles(DirName(NsfPath))) {
        std::map<int, std::string> Found;
        if (ReadTitles(File, NsfPath, Found) && !Found.empty()) {
            for (const auto& t : Found)
                if (!Titles.count(t.first)) Titles.insert(t);
            UsedFiles.push_back(File);
        }
    }
}

}
