#include "gen/common.h"
#include "customtext.h"
#include "xclib/xclib.h"
#include <cerrno>
#include <limits>

CustomText g_customText;

const char* CustomText::GetLanguageCode() const {
    static constexpr const char* codes[] = { "en", "de", "fr", "it", "es", "pt-BR" };
    static_assert(sizeof(codes) / sizeof(codes[0]) == NumLanguages);
    return codes[(s32)lang >= 0 && (s32)lang < NumLanguages ? (s32)lang : LangEnglish];
}

// PSX: CustomText::Init
void CustomText::Init() {
    LoadLanguage(lang);
}

bool CustomText::LoadLanguage(GameLanguage language) {
    static constexpr const char* paths[] = { "pc/text/english.txt", "pc/text/german.txt",
        "pc/text/french.txt", "pc/text/italian.txt", "pc/text/spanish.txt", "pc/text/portuguese.txt" };
    static_assert(std::size(paths) == NumLanguages);
    std::vector<StringEntry> next;
    if (!LoadTextFile(paths[LangEnglish], next)) return false;
    if (language != LangEnglish) {
        std::vector<StringEntry> translated;
        if (!LoadTextFile(paths[language], translated)) return false;
        for (auto& entry : translated) {
            bool replaced = false;
            for (auto& fallback : next) {
                if (fallback.hash == entry.hash) {
                    fallback.text = std::move(entry.text);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) next.push_back(std::move(entry));
        }
    }
    // Commit only complete reads; failed switches preserve borrowed views and revision.
    strings.swap(next);
    lang = language;
    ++localizationID;
    return true;
}

void CustomText::Shutdown() {
    ClearStrings();
}

void CustomText::SetLanguage(GameLanguage language) {
    if ((s32)language < 0 || (s32)language >= (s32)NumLanguages) {
        language = LangEnglish;
    }

    if (lang == language && !strings.empty()) {
        return; // no change
    }
    if (!LoadLanguage(language))
        LOG_ALWAYS("[CustomText] Language switch to %d failed; retaining language %d", (s32)language, (s32)lang);
}

const char* CustomText::GetString(const char* token) const {
    if (!token) {
        return nullptr;
    }

    u32 hash = xcHash(token);
    return GetStringByHash(hash);
}

Utf8TextView CustomText::GetText(const char* token) const {
    if (!token) {
        return {};
    }

    return GetTextByHash(xcHash(token));
}

const char* CustomText::GetStringByHash(u32 hash) const {
    // Linear search like xcInventory::FindItem (no double hashing)
    for (const auto& entry : strings) {
        if (entry.hash == hash) {
            return entry.text.c_str();

        }
    }

    return nullptr;
}

Utf8TextView CustomText::GetTextByHash(u32 hash) const {
    for (const auto& entry : strings) {
        if (entry.hash == hash) {
            return Utf8TextView(entry.text);
        }
    }

    return {};
}

void CustomText::ClearStrings() {
    strings.clear();
}

bool CustomText::LoadTextFile(const char* path, std::vector<StringEntry>& entries) {
    if (!path) {
        return false;
    }

    u8* rawData = nullptr;
    u32 fileSize = 0;
    if (!xcReadFileLow(path, &rawData, &fileSize)) {
        LOG("[CustomText::LoadTextFile] Failed to open: %s", path);
        return false;
    }

    std::string fileText((const char*)rawData, (size_t)fileSize);
    delete[] rawData;

    if (fileText.size() >= 3
        && (u8)fileText[0] == 0xEF
        && (u8)fileText[1] == 0xBB
        && (u8)fileText[2] == 0xBF) {
        fileText.erase(0, 3);
    }

    // Parse file line by line
    size_t cursor = 0;
    std::string pendingToken;
    u32 pendingHash = 0;
    bool expectingText = false;

    while (cursor < fileText.size()) {
        // Find next line
        size_t lineEnd = fileText.find('\n', cursor);
        if (lineEnd == std::string::npos) {
            lineEnd = fileText.size();
        }

        std::string line = fileText.substr(cursor, lineEnd - cursor);
        cursor = (lineEnd < fileText.size()) ? (lineEnd + 1) : fileText.size();

        // Trim whitespace
        size_t start = 0;
        size_t end = line.size();
        while (start < end && std::isspace((unsigned char)line[start])) start++;
        while (end > start && std::isspace((unsigned char)line[end - 1])) end--;
        line = line.substr(start, end - start);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Check if line is a token declaration: [TOKEN] or [TOKEN] // comment
        size_t closeBracketPos = std::string::npos;
        if (!line.empty() && line[0] == '[') {
            closeBracketPos = line.find(']', 1);
        }

        if (closeBracketPos != std::string::npos) {
            // Validate tail after closing bracket (allow whitespace or // comment)
            std::string tail = line.substr(closeBracketPos + 1);
            size_t tailStart = 0;
            while (tailStart < tail.size() && std::isspace((unsigned char)tail[tailStart])) tailStart++;
            tail = tail.substr(tailStart);

            if (!tail.empty() && !(tail.size() >= 2 && tail[0] == '/' && tail[1] == '/')) {
                // Not a valid token line; treat as non-token text line.
                closeBracketPos = std::string::npos;
            }
        }

        if (closeBracketPos != std::string::npos) {
            // Extract token (between brackets)
            std::string token = line.substr(1, closeBracketPos - 1);

            // Trim token
            size_t tokStart = 0;
            size_t tokEnd = token.size();
            while (tokStart < tokEnd && std::isspace((unsigned char)token[tokStart])) tokStart++;
            while (tokEnd > tokStart && std::isspace((unsigned char)token[tokEnd - 1])) tokEnd--;
            token = token.substr(tokStart, tokEnd - tokStart);

            if (!token.empty()) {
                u32 parsedHash = 0;
                if (TryParseTokenHash(token, &parsedHash)) {
                    pendingToken = token;
                    pendingHash = parsedHash;
                    expectingText = true;
                }
                else {
                    pendingToken.clear();
                    pendingHash = 0;
                    expectingText = false;
                }
            }
        }
        else if (expectingText && !pendingToken.empty()) {
            // This line is the text for pendingHash
            bool replaced = false;
            for (auto& entry : entries) {
                if (entry.hash == pendingHash) {
                    entry.text = line;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                StringEntry entry;
                entry.hash = pendingHash;
                entry.text = line;
                entries.push_back(entry);
            }
            expectingText = false;
            pendingToken.clear();
            pendingHash = 0;
        }
    }

    if (entries.empty() || expectingText) {
        LOG_ALWAYS("[CustomText] Empty or incomplete catalog: %s", path);
        return false;
    }
    LOG("[CustomText::LoadTextFile] Loaded %u strings from %s", (u32)entries.size(), path);
    return true;
}

bool CustomText::TryParseTokenHash(const std::string& token, u32* outHash) {
    if (!outHash || token.empty()) {
        return false;
    }

    // Numeric hash token: [0x1234ABCD] or [305419896]
    const bool isHex = (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'));
    const bool startsWithDigit = std::isdigit((unsigned char)token[0]) != 0;
    if (isHex || startsWithDigit) {
        char* endPtr = nullptr;
        errno = 0;
        unsigned long value = std::strtoul(token.c_str(), &endPtr, 0);
        if (endPtr != token.c_str() && *endPtr == '\0' && errno != ERANGE
            && value <= std::numeric_limits<u32>::max()) {
            *outHash = (u32)value;
            return true;
        }
        return false;
    }

    // String token path: enforce max 16 chars, then hash with xcHash.
    if (token.size() > 16) {
        LOG("[CustomText] Ignoring token '%s' (string tokens must be <= 16 chars)", token.c_str());
        return false;
    }

    *outHash = xcHash(token.c_str());
    return true;
}
