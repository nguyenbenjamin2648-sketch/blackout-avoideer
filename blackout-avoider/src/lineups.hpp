// lineups.hpp -- loads named enjoyment profiles from data/lineups.txt.
//
// File format: one lineup per line as "name: v1 v2 v3 ...". Blank lines and lines
// beginning with '#' are ignored. Keeping inputs in a data file makes all reported
// results reproducible.
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace blackout {

using Lineup = std::pair<std::string, std::vector<int>>;

inline std::vector<Lineup> load_lineups(const std::string& path) {
    std::vector<Lineup> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = line.substr(0, colon);
        std::istringstream vals(line.substr(colon + 1));
        std::vector<int> enjoy;
        int v;
        while (vals >> v) enjoy.push_back(v);
        if (!enjoy.empty()) out.emplace_back(name, enjoy);
    }
    return out;
}

inline const std::vector<int>& find_lineup(const std::vector<Lineup>& all,
                                           const std::string& name) {
    for (const auto& p : all)
        if (p.first == name) return p.second;
    static const std::vector<int> empty;
    return empty;
}

}  // namespace blackout
