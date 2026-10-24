#include "radmovie/subtitles.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

void TestPromptStyles();
void TestControllerFeedback();
void TestControllerSpeaker();

int main() {
    TestPromptStyles();
    TestControllerFeedback();
    TestControllerSpeaker();
    MovieSubtitles subtitles;
    assert(!subtitles.Parse(""));
    assert(!subtitles.Parse("not an SRT"));
    assert(!subtitles.Parse("1\n00:00:61,000 --> 00:00:62,000\nInvalid\n"));
    assert(!subtitles.Parse("1\n00:00:02,000 --> 00:00:01,000\nReversed\n"));
    assert(!subtitles.Parse("1\n00:00:01,000 --> 00:00:02,000 trailing\nInvalid\n"));
    assert(!subtitles.Parse("1\n00:00:01,000 --> 00:00:02,000\n\xc0\xaf\n"));
    assert(!subtitles.Parse(std::string(1024*1024+1, 'x')));
    assert(subtitles.Parse("\xef\xbb\xbf" "2\r\n00:00:02,000 --> 00:00:04,000\r\nSecond\r\n\r\n"
        "1\r\n00:00:01,000 --> 00:00:03,000\r\nOl\xc3\xa1!\r\nLine two\r\n"));
    assert(subtitles.TextAt(0).empty());
    assert(subtitles.TextAt(1.0) == "Ol\xc3\xa1!\nLine two");
    assert(subtitles.TextAt(2.0) == "Ol\xc3\xa1!\nLine two\nSecond");
    assert(subtitles.TextAt(3.0) == "Second");
    assert(subtitles.TextAt(4.0).empty());
    assert(subtitles.TextAt(-1.0).empty());
    assert(subtitles.TextAt(std::numeric_limits<double>::quiet_NaN()).empty());
    assert(subtitles.TextAt(std::numeric_limits<double>::infinity()).empty());
    // Queries may arrive at any render rate, jump forward after a stall, or replay.
    for (int fps : {15, 30, 60, 144}) {
        for (int frame = 0; frame < fps*5; ++frame) {
            const double t = frame / (double)fps;
            assert(subtitles.TextAt(t).empty() == (t < 1.0 || t >= 4.0));
        }
    }
    assert(subtitles.TextAt(1000.0).empty());
    assert(!subtitles.TextAt(1.0).empty());
    assert(subtitles.Parse("1\n00:00:01,000 --> 00:00:02,000\nValid\n\n"
        "2\n00:00:02,000 -> 00:00:03,000\nBad\n"));
    assert(subtitles.Entries().size() == 1);
    subtitles.Clear();
    assert(subtitles.Entries().empty() && subtitles.TextAt(1.0).empty());
    assert(!subtitles.Parse("broken"));
    assert(subtitles.Entries().empty());

    for (const char* locale : {"en", "de", "fr", "it", "es", "pt-BR"}) {
        for (const char* name : {"PROLOG", "FACTORY", "MAKING", "VICTORY", "CREDITS"}) {
            std::ifstream input(std::string("res/pc/subtitles/") + locale + "/" + name + ".srt", std::ios::binary);
            assert(input);
            std::string contents((std::istreambuf_iterator<char>(input)), {});
            for (int repeat = 0; repeat < 100; ++repeat) {
                assert(subtitles.Parse(contents));
                assert(!subtitles.TextAt(subtitles.Entries().front().startSeconds).empty());
                subtitles.Clear();
            }
            subtitles.Parse(contents);
            std::cout << "PASS " << locale << "/" << name << ": " << subtitles.Entries().size() << " cues, 100 replays\n";
        }
    }
    std::cout << "PASS SRT timing, UTF-8, invalid input, overlap, clear and render-rate independence\n";
}
