#include "log.h"
#include "extra/version.h"
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdlib>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#ifndef RECHAN_BUILD_COMMIT
#define RECHAN_BUILD_COMMIT "unavailable"
#endif

Log& Log::Get() {
    static Log instance;
    return instance;
}

Log::Log()
    : m_File() {}

Log::~Log() {
    Shutdown();
}

void Log::Init(const char* directory) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_File.is_open())
        return;

    namespace fs = std::filesystem;
    std::error_code ec;
    auto working = fs::current_path(ec);
    bool checkout = false;
    for (auto parent = working; !parent.empty(); parent = parent.parent_path()) {
        if (fs::exists(parent / ".git", ec)) { checkout = true; break; }
        if (parent == parent.root_path()) break;
    }
    auto target = directory ? fs::u8path(directory) : working / "logs";
    auto now = std::chrono::system_clock::now();
    const auto stamp = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &stamp);
    const unsigned long pid = GetCurrentProcessId();
#else
    localtime_r(&stamp, &local);
    const unsigned long pid = (unsigned long)getpid();
#endif
    char date[48];
    std::strftime(date, sizeof(date), "%Y-%m-%d_%H-%M-%S", &local);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    auto openSession = [&](const fs::path& folder) {
        fs::create_directories(folder, ec);
        if (ec) return false;
        for (int attempt = 0; attempt < 32; ++attempt) {
            char name[96];
            std::snprintf(name, sizeof(name), "%s_%03lld_%lu_%02d", date, milliseconds, pid, attempt);
            const auto stem = fs::absolute(folder / name, ec);
            if (ec) return false;
            const auto path = fs::path(stem.native() + fs::path(".log").native());
#if defined(_WIN32)
            HANDLE reserved = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (reserved == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_FILE_EXISTS) continue;
                return false;
            }
            CloseHandle(reserved);
#else
            const int reserved = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (reserved < 0) continue;
            close(reserved);
#endif
            m_File.open(path, std::ios::out | std::ios::app);
            if (!m_File) { m_File.clear(); return false; }
            const auto utf8Path = path.u8string(), utf8Stem = stem.u8string();
            m_SessionPath.assign((const char*)utf8Path.data(), utf8Path.size());
            m_SessionStem.assign((const char*)utf8Stem.data(), utf8Stem.size());
            return true;
        }
        return false;
    };
    bool opened = (!checkout || directory) && openSession(target);
    if (!opened) {
#if defined(_WIN32)
        wchar_t appData[32768]{};
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", appData, (DWORD)std::size(appData));
        if (length && length < std::size(appData)) opened = openSession(fs::path(appData) / "ReChan/logs");
#else
        const char* state = std::getenv("XDG_STATE_HOME");
        const char* home = std::getenv("HOME");
        if (state) opened = openSession(fs::path(state) / "rechan/logs");
        else if (home) opened = openSession(fs::path(home) / ".local/state/rechan/logs");
#endif
    }
    char header[512];
    std::snprintf(header, sizeof(header), "[Session] started=%s local pid=%lu version=%s commit=%s\n",
        date, pid, GAME_VERSION, RECHAN_BUILD_COMMIT);
    if (opened) {
        m_File << header << "[Session] log=" << m_SessionPath << '\n';
        m_File.flush();
    }
    std::fprintf(stderr, "%s[Session] log=%s\n", header, opened ? m_SessionPath.c_str() : "unavailable (no writable diagnostic directory)");
}

void Log::Shutdown(const char* outcome, int exitCode) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_File.is_open()) {
        m_File << "[Session] end outcome=" << outcome << " exit=" << exitCode << '\n';
        m_File.flush();
        m_File.close();
    }
}

void Log::LogMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessageV(fmt, args);
    va_end(args);
}

void Log::LogMessageV(const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    char messageBuffer[2048];
    std::vsnprintf(messageBuffer, sizeof(messageBuffer), fmt, args);

    static const auto started = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    char finalBuffer[2100];
    std::snprintf(finalBuffer, sizeof(finalBuffer), "[+%lldms] %s\n", elapsed, messageBuffer);

    if (m_File.is_open()) {
        m_File << finalBuffer;
        m_File.flush();
    }

    WriteToConsole(finalBuffer);


    if (m_MessageStack.size() > 20) {
        m_MessageStack.erase(m_MessageStack.begin());
    }

    m_MessageStack.push_back(finalBuffer);
}

void Log::WriteToConsole(const char* text) {
    std::printf("%s", text);
}
