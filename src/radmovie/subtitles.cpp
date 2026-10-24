#include "radmovie/subtitles.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace {
bool ValidUtf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        unsigned char c = (unsigned char)text[i++];
        if (c < 0x80) {
            if (c < 32 && c != '\n' && c != '\r' && c != '\t') return false;
            continue;
        }
        int count;
        unsigned int cp;
        unsigned int minimum;
        if (c >= 0xc2 && c <= 0xdf) { count = 1; cp = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { count = 2; cp = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { count = 3; cp = c & 7; minimum = 0x10000; }
        else return false;
        while (count--) {
            if (i == text.size()) return false;
            c = (unsigned char)text[i++];
            if ((c & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (c & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}

std::string_view Trim(std::string_view text) {
    const size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

bool Timestamp(std::string_view text, double& seconds) {
    text = Trim(text);
    // SRT uses fixed-width hh:mm:ss,mmm timestamps.
    if (text.size() != 12 || text[2] != ':' || text[5] != ':' || text[8] != ',') return false;
    int fields[4] = {};
    const size_t offsets[] = {0, 3, 6, 9};
    const size_t lengths[] = {2, 2, 2, 3};
    for (int i = 0; i < 4; ++i) {
        auto part = text.substr(offsets[i], lengths[i]);
        for (char c : part) if (c < '0' || c > '9') return false;
        auto result = std::from_chars(part.data(), part.data() + part.size(), fields[i]);
        if (result.ec != std::errc{} || result.ptr != part.data() + part.size()) return false;
    }
    if (fields[1] >= 60 || fields[2] >= 60) return false;
    seconds = fields[0] * 3600.0 + fields[1] * 60.0 + fields[2] + fields[3] / 1000.0;
    return true;
}
}

bool MovieSubtitles::Parse(std::string_view contents) {
    Clear();
    if (contents.size() > 1024 * 1024 || !ValidUtf8(contents)) return false;
    if (contents.starts_with("\xef\xbb\xbf")) contents.remove_prefix(3);

    std::vector<std::string_view> block;
    auto flush = [&]() {
        if (block.empty()) return;
        if (block.size() < 3 || block.size() > 4) { ++rejectedCues; block.clear(); return; }
        bool validIndex = !Trim(block[0]).empty();
        for (char c : Trim(block[0])) validIndex &= c >= '0' && c <= '9';
        const auto timing = block[1];
        const size_t arrow = timing.find("-->");
        MovieSubtitle entry;
        if (validIndex && arrow != std::string_view::npos
            && Timestamp(timing.substr(0, arrow), entry.startSeconds)
            && Timestamp(timing.substr(arrow + 3), entry.endSeconds)
            && entry.endSeconds > entry.startSeconds) {
            for (size_t i = 2; i < block.size(); ++i) {
                if (!entry.text.empty()) entry.text += '\n';
                entry.text += block[i];
            }
            entries.push_back(std::move(entry));
        }
        else ++rejectedCues;
        block.clear();
    };
    while (!contents.empty()) {
        const size_t end = contents.find('\n');
        auto line = contents.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (Trim(line).empty()) flush();
        else block.push_back(line);
        if (end == std::string_view::npos) break;
        contents.remove_prefix(end + 1);
    }
    flush();
    std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.startSeconds < b.startSeconds;
    });
    return !entries.empty();
}

std::string MovieSubtitles::TextAt(double seconds) const {
    std::string text;
    size_t lines = 0;
    if (!std::isfinite(seconds) || seconds < 0.0) return text;
    for (const auto& entry : entries) {
        if (entry.startSeconds > seconds) break;
        if (seconds < entry.endSeconds) {
            const size_t cueLines = 1 + std::count(entry.text.begin(), entry.text.end(), '\n');
            // Bound external overlapping tracks without truncating a cue mid-sentence.
            if (lines + cueLines > 4) continue;
            if (!text.empty()) text += '\n';
            text += entry.text;
            lines += cueLines;
        }
    }
    return text;
}
