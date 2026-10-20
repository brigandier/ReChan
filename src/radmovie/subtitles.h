#pragma once

#include <string>
#include <string_view>
#include <vector>

struct MovieSubtitle {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    std::string text;
};

class MovieSubtitles {
public:
    bool Parse(std::string_view contents);
    void Clear() { entries.clear(); }
    std::string TextAt(double seconds) const;
    const std::vector<MovieSubtitle>& Entries() const { return entries; }

private:
    std::vector<MovieSubtitle> entries;
};
