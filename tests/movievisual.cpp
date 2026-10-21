#include "radmovie/movieplayer.h"
#include "extra/customtext.h"
#include "gen/common.h"
#include "gen/display.h"
#include "pc/settings.h"
#include "pc/textmgr.h"
#include "pc/tim.h"
#include "p3d/context.h"
#include "pddi/pddi.h"
#include "pddi/pddidev.h"
#include "vendor/stb/stb_image_write.h"
#include "../vendor/stb/stb_truetype.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

// Optional renderer regression test; requires locally supplied original movies.
struct MovieSubtitleVisualTest {
    static void LegacySubtitle(Utf8TextView text) {
        ScreenDraw::Batch batch;
        g_textManager->PushState();
        if (g_textManager->SetFontByName("Legal")) {
            g_textManager->SetScale(SCREEN_SCALE_Y(0.48f));
            g_textManager->SetWrapWidth(SCREEN_SCALE_X(DEFAULT_SCREEN_WIDTH * 0.88f));
            g_textManager->SetLineSpacing((s32)SCREEN_SCALE_Y(2.0f));
            g_textManager->SetAlignment(TextAlign_Center);
            g_textManager->SetColor(255, 255, 255);
            g_textManager->SetPromptsEnabled(false);
            g_textManager->SetShadow(true, SCREEN_SCALE_Y(1.0f), SCREEN_SCALE_Y(1.0f));
            g_textManager->SetOutline(true, SCREEN_SCALE_Y(1.0f));
            const auto bounds = g_textManager->MeasureString(text);
            g_textManager->PrintString(text, SCREEN_WIDTH * 0.5f,
                SCREEN_HEIGHT - SCREEN_SCALE_Y(18.0f) - bounds.height);
        }
        g_textManager->PopState();
    }

    static bool StateRestored(const TextRenderState& state) {
        const auto& after = g_textManager->GetState();
        return state.font == after.font && state.scaleX == after.scaleX && state.scaleY == after.scaleY
            && state.wrapWidth == after.wrapWidth && state.lineSpacing == after.lineSpacing
            && state.align == after.align && state.color.r == after.color.r && state.color.g == after.color.g
            && state.color.b == after.color.b && state.color.a == after.color.a
            && state.shadow.enabled == after.shadow.enabled && state.shadow.offsetX == after.shadow.offsetX
            && state.shadow.offsetY == after.shadow.offsetY && state.outline.enabled == after.outline.enabled
            && state.outline.thickness == after.outline.thickness && state.promptsEnabled == after.promptsEnabled;
    }

    static bool Draw(MoviePlayer& movie, Display& display, double seconds, bool legacy,
                     std::vector<unsigned char>& pixels, int width, int height) {
        display.BeginFrame();
        g_settings.subtitlesEnabled = false;
        movie.Render();
        const auto state = g_textManager->GetState();
        g_settings.subtitlesEnabled = true;
        if (legacy) LegacySubtitle(Utf8TextView(movie.subtitles.TextAt(seconds)));
        else movie.RenderSubtitles(seconds);
        const bool restored = StateRestored(state);
        display.GetView().EndRender();
        p3d::context->EndFrame();
        int actualWidth = 0, actualHeight = 0;
        const bool read = p3d::display->ReadBackBuffer(pixels, actualWidth, actualHeight);
        p3d::display->SwapBuffers();
        p3d::display->PollEvents();
        return read && restored && width == actualWidth && height == actualHeight;
    }

    static bool Save(const std::filesystem::path& path, const std::vector<unsigned char>& pixels, int w, int h) {
        return stbi_write_png(path.string().c_str(), w, h, 3, pixels.data(), w * 3) != 0;
    }

    static bool CheckGlyphs(const std::vector<MovieSubtitle>& entries, const char* fontPath) {
        std::ifstream file(fontPath, std::ios::binary);
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        stbtt_fontinfo font{};
        if (bytes.empty() || !stbtt_InitFont(&font, bytes.data(), 0)) return false;
        for (const auto& cue : entries) {
            for (size_t i = 0; i < cue.text.size();) {
                unsigned int cp = (unsigned char)cue.text[i++];
                if (cp == '\n' || cp == '\r') continue;
                int extra = 0;
                if (cp >= 0xf0) { cp &= 7; extra = 3; }
                else if (cp >= 0xe0) { cp &= 15; extra = 2; }
                else if (cp >= 0xc0) { cp &= 31; extra = 1; }
                while (extra-- && i < cue.text.size()) cp = (cp << 6) | (cue.text[i++] & 63);
                const bool packed = (cp >= 32 && cp <= 126) || (cp >= 160 && cp < 592) || cp == 0x20ac;
                if (!packed || !stbtt_FindGlyphIndex(&font, (int)cp)) return false;
            }
        }
        return true;
    }

    static int Run(const char* movieDirectory, const char* outputDirectory) {
        namespace fs = std::filesystem;
        fs::create_directories(outputDirectory);
        std::ofstream metrics(fs::path(outputDirectory) / "metrics.tsv");
        metrics << "width\theight\tlocale\tmovie\tcue\tstart\tend\ttext_width\ttext_height\tlines\n";
        Display display;
        g_textManager->ResetState();
        g_textManager->SetFontByName("Legal");
        g_textManager->SetScale(0.7f);
        g_textManager->SetColor(13, 29, 47, 123);
        g_textManager->SetShadow(true, 3, 4);
        g_textManager->SetOutline(true, 3);
        const bool savedEnabled = g_settings.subtitlesEnabled;
        const auto savedLanguage = g_customText.GetLanguage();
        const char* names[] = {"PROLOG", "FACTORY", "MAKING", "VICTORY", "CREDITS"};
        int checked = 0, captures = 0, overlaps = 0;
        bool fallbackChecked = false;
        for (int height : {1080, 2160}) {
            const int width = height * 16 / 9;
            // A decorated window can be clamped below 2160 pixels by the desktop work area.
            p3d::display->SetFullscreen(height == 2160);
            p3d::display->SetResolution(width, height);
            p3d::display->SetVSync(false);
            p3d::display->PollEvents();
            display.BeginFrame();
            display.GetView().EndRender();
            p3d::context->EndFrame();
            if ((int)SCREEN_WIDTH != width || (int)SCREEN_HEIGHT != height) {
                std::printf("Requested %dx%d, actual framebuffer %dx%d\n", width, height,
                    (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT);
                return 21;
            }
            if (height == 1080) {
                MoviePlayer smoke;
                const auto path = fs::path(movieDirectory) / "PROLOG.str";
                g_customText.SetLanguage(LangPortugueseBrazil);
                if (!smoke.Open(path.string().c_str())) return 33;
                g_settings.subtitlesEnabled = true;
                const auto start = std::chrono::steady_clock::now();
                do {
                    display.BeginFrame();
                    smoke.Render();
                    display.GetView().EndRender();
                    p3d::context->EndFrame();
                    p3d::display->SwapBuffers();
                    p3d::display->PollEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                } while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1250));
                if (smoke.framesDecoded < 2 || smoke.finished) return 34;
                std::puts("PASS native playback startup, monotonic-clock rendering and early interruption");
                std::fflush(stdout);
            }
            for (const char* name : names) {
                const auto path = fs::path(movieDirectory) / (std::string(name) + ".str");
                g_customText.SetLanguage(LangPortugueseBrazil);
                MoviePlayer movie;
                if (!movie.Open(path.string().c_str())) return 22;
                const auto ptEntries = movie.subtitles.Entries();
                if (ptEntries.empty()) return 23;
                size_t longest = 0;
                for (size_t i = 1; i < ptEntries.size(); ++i)
                    if (ptEntries[i].text.size() > ptEntries[longest].text.size()) longest = i;
                std::vector<size_t> samples{0, longest, ptEntries.size() - 1};
                for (size_t i = 1; i < ptEntries.size(); ++i)
                    if (ptEntries[i].startSeconds < ptEntries[i - 1].endSeconds) samples.push_back(i);
                std::sort(samples.begin(), samples.end());
                samples.erase(std::unique(samples.begin(), samples.end()), samples.end());
                for (int language = 0; language < NumLanguages; ++language) {
                    g_customText.SetLanguage((GameLanguage)language);
                    movie.LoadSubtitles(path.string().c_str());
                    const auto& entries = movie.subtitles.Entries();
                    if (entries.size() != ptEntries.size() || !CheckGlyphs(entries, "pc/fonts/YIKES!__.TTF")) return 24;
                    g_textManager->PushState();
                    g_textManager->SetFontByName("Menu");
                    g_textManager->SetScale(SCREEN_SCALE_Y(0.35f));
                    g_textManager->SetLineSpacing((s32)SCREEN_SCALE_Y(2.0f));
                    const float wrap = SCREEN_SCALE_X(DEFAULT_SCREEN_WIDTH * 0.88f);
                    g_textManager->SetWrapWidth(wrap);
                    g_textManager->SetPromptsEnabled(false);
                    for (size_t i = 0; i < entries.size(); ++i) {
                        const auto& cue = entries[i];
                        const auto bounds = g_textManager->MeasureString(Utf8TextView(cue.text));
                        metrics << width << '\t' << height << '\t' << g_customText.GetLanguageCode() << '\t' << name << '\t'
                            << i + 1 << '\t' << cue.startSeconds << '\t' << cue.endSeconds << '\t' << bounds.width
                            << '\t' << bounds.height << '\t' << bounds.lineCount << '\n';
                        if (bounds.width > wrap + 1 || bounds.height > SCREEN_HEIGHT * 0.3f) return 25;
                        ++checked;
                        if (i && cue.startSeconds < entries[i - 1].endSeconds) {
                            const double time = (cue.startSeconds + std::min(cue.endSeconds, entries[i - 1].endSeconds)) * 0.5;
                            const auto joint = g_textManager->MeasureString(Utf8TextView(movie.subtitles.TextAt(time)));
                            if (joint.width > wrap + 1 || joint.height > SCREEN_HEIGHT * 0.3f) return 37;
                            ++overlaps;
                        }
                    }
                    g_textManager->PopState();
                }
                for (size_t index : samples) {
                    const double seconds = (ptEntries[index].startSeconds + ptEntries[index].endSeconds) * 0.5;
                    const u32 wanted = (u32)std::floor(seconds / movie.frameInterval) + 1;
                    while (movie.framesDecoded < wanted && !movie.finished) movie.AdvanceFrame();
                    if (movie.finished) {
                        std::printf("Movie ended before sample: %s cue=%zu time=%.3f frames=%u wanted=%u interval=%.6f sectors=%u/%u\n",
                            name, index + 1, seconds, movie.framesDecoded, wanted, movie.frameInterval,
                            movie.currentSector, movie.sectorCount);
                        return 26;
                    }
                    for (int language = 0; language < NumLanguages; ++language) {
                        if (language != LangPortugueseBrazil && (index != longest || std::string(name) != "PROLOG")) continue;
                        g_customText.SetLanguage((GameLanguage)language);
                        movie.LoadSubtitles(path.string().c_str());
                        const auto stem = std::string(name) + "-" + g_customText.GetLanguageCode() + "-cue"
                            + std::to_string(index + 1) + "-" + std::to_string(height);
                        std::vector<unsigned char> oldPixels, newPixels, disabledPixels;
                        if (!Draw(movie, display, seconds, true, oldPixels, width, height)
                            || !Draw(movie, display, seconds, false, newPixels, width, height)) return 27;
                        if (!Save(fs::path(outputDirectory) / (stem + "-A.png"), oldPixels, width, height)
                            || !Save(fs::path(outputDirectory) / (stem + "-B.png"), newPixels, width, height)) return 28;
                        captures += 2;
                        // Disabled subtitles must leave the framebuffer untouched.
                        display.BeginFrame();
                        g_settings.subtitlesEnabled = false;
                        movie.Render();
                        display.GetView().EndRender();
                        p3d::context->EndFrame();
                        int w = 0, h = 0;
                        if (!p3d::display->ReadBackBuffer(disabledPixels, w, h)) return 29;
                        movie.RenderSubtitles(seconds);
                        std::vector<unsigned char> after;
                        if (!p3d::display->ReadBackBuffer(after, w, h) || after != disabledPixels) return 30;
                        p3d::display->SwapBuffers();
                        g_settings.subtitlesEnabled = true;
                        if (newPixels == disabledPixels) return 31;
                        if (!fallbackChecked) {
                            TextManager fallback;
                            fallback.Init();
                            if (!fallback.LoadFont({"Legal", "pc/fonts/Roboto-Regular.ttf", 48})) return 35;
                            TextManager* primary = g_textManager;
                            g_textManager = &fallback;
                            std::vector<unsigned char> fallbackPixels;
                            const bool drawn = Draw(movie, display, seconds, false, fallbackPixels, width, height);
                            g_textManager = primary;
                            if (!drawn || fallbackPixels == disabledPixels) return 36;
                            fallbackChecked = true;
                        }
                    }
                }
                movie.Close();
                if (!movie.subtitles.Entries().empty() || movie.IsOpen()) return 32;
                metrics.flush();
                std::printf("PASS %dx%d %s: six-language metrics, paired captures, state and disabled-subtitle checks\n", width, height, name);
                std::fflush(stdout);
            }
        }
        g_settings.subtitlesEnabled = savedEnabled;
        g_customText.SetLanguage(savedLanguage);
        p3d::display->SetFullscreen(false);
        std::printf("PASS native movie subtitle rendering: %d cue layouts, %d captures, %d overlapping entries, two resolutions\n",
            checked, captures, overlaps);
        std::puts("PASS missing Menu font falls back to the existing Legal font");
        return 0;
    }
};

int AutoTest_RenderMovieSubtitles(const char* movieDirectory, const char* outputDirectory) {
    return MovieSubtitleVisualTest::Run(movieDirectory, outputDirectory);
}
