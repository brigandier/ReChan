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
    void Clear() { entries.clear(); rejectedCues = 0; }
    std::string TextAt(double seconds) const;
    const std::vector<MovieSubtitle>& Entries() const { return entries; }
    size_t RejectedCueCount() const { return rejectedCues; }

private:
    std::vector<MovieSubtitle> entries;
    size_t rejectedCues = 0;
};
