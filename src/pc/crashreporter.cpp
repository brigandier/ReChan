#include "pc/crashreporter.h"

#include "extra/version.h"
#include "pc/log.h"
#include <atomic>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

static constexpr const char* kIssueUrl =
"https://github.com/SilverwireGames/ReChan/issues/new";

static std::atomic<int> s_gameState{-1}, s_level{-1}, s_region{-1};
static std::atomic<const char*> s_operation{"startup"};
void CrashReporter::SetGameContext(int state, int level, int region) {
    s_gameState.store(state); s_level.store(level); s_region.store(region);
}
void CrashReporter::SetOperation(const char* operation) {
    s_operation.store(operation ? operation : "unknown");
    LOG_ALWAYS("[Operation] %s", operation ? operation : "unknown");
}

#if defined(RC_PLATFORM_WINDOWS)

#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>


static volatile LONG s_handlingCrash = 0;
static wchar_t s_reportPath[32768]{}, s_dumpPath[32768]{}, s_logPath[32768]{};
static char s_logUtf8[32768]{};
static bool s_suppressDialog = false;

static void WriteHandle(HANDLE file, const char* text) {
    if (file == INVALID_HANDLE_VALUE || !text) return;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

static void AppendLogTail(HANDLE output) {
    HANDLE input = CreateFileW(s_logPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{}, offset{};
    if (GetFileSizeEx(input, &size) && size.QuadPart > 65536) {
        offset.QuadPart = size.QuadPart - 65536;
        SetFilePointerEx(input, offset, nullptr, FILE_BEGIN);
    }
    char buffer[4096];
    DWORD read = 0;
    for (int blocks = 0; blocks < 16 && ReadFile(input, buffer, sizeof(buffer), &read, nullptr) && read; ++blocks) {
        DWORD written = 0;
        WriteFile(output, buffer, read, &written, nullptr);
    }
    CloseHandle(input);
}

static void WriteCrashReport(EXCEPTION_POINTERS* exceptionInfo, const char* reason) {
    // Paths and diagnostic context are prepared before faults. No logger mutex,
    // symbol loading, directory traversal or game-object access is allowed here.
    HANDLE report = CreateFileW(s_reportPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (report != INVALID_HANDLE_VALUE) {
        char buffer[2048];
        std::snprintf(buffer, sizeof(buffer),
            "ReChan fatal report\r\nVersion: %s\r\nPID: %lu\r\nThread: %lu\r\nReason: %s\r\n"
            "Game state: %d\r\nLevel: %d\r\nRegion: %d\r\nLast operation: %s\r\n",
            GAME_VERSION, GetCurrentProcessId(), GetCurrentThreadId(), reason,
            s_gameState.load(), s_level.load(), s_region.load(), s_operation.load());
        WriteHandle(report, buffer);
        WriteHandle(report, "Session log: "); WriteHandle(report, s_logUtf8); WriteHandle(report, "\r\n");
        if (exceptionInfo && exceptionInfo->ExceptionRecord) {
            const auto* record = exceptionInfo->ExceptionRecord;
            std::snprintf(buffer, sizeof(buffer), "Exception code: 0x%08lX\r\nFault address: %p\r\n",
                record->ExceptionCode, record->ExceptionAddress);
            WriteHandle(report, buffer);
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
                std::snprintf(buffer, sizeof(buffer), "Access type: %llu\r\nReferenced address: %p\r\n",
                    (unsigned long long)record->ExceptionInformation[0], (void*)record->ExceptionInformation[1]);
                WriteHandle(report, buffer);
            }
        }
#if defined(_M_X64) || defined(__x86_64__)
        if (exceptionInfo && exceptionInfo->ContextRecord) {
            std::snprintf(buffer, sizeof(buffer), "Fault context RIP: 0x%llX RSP: 0x%llX\r\n",
                (unsigned long long)exceptionInfo->ContextRecord->Rip, (unsigned long long)exceptionInfo->ContextRecord->Rsp);
            WriteHandle(report, buffer);
        }
#endif
        WriteHandle(report, "Handler stack addresses (resolve the fault stack offline from the dump):\r\n");
        void* frames[32]{};
        const USHORT count = CaptureStackBackTrace(0, 32, frames, nullptr);
        for (USHORT i = 0; i < count; ++i) {
            std::snprintf(buffer, sizeof(buffer), "  #%u %p\r\n", i, frames[i]);
            WriteHandle(report, buffer);
        }
        WriteHandle(report, "\r\nSession log tail (up to 64 KiB):\r\n");
        AppendLogTail(report);
        FlushFileBuffers(report);
        CloseHandle(report);
    }
    HANDLE dump = CreateFileW(s_dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = GetCurrentThreadId(); info.ExceptionPointers = exceptionInfo; info.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump,
        (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo),
        exceptionInfo ? &info : nullptr, nullptr, nullptr);
    FlushFileBuffers(dump);
    CloseHandle(dump);
}

static void NotifyUser() {
#if !defined(RC_PLATFORM_NULL)
    if (!s_suppressDialog) MessageBoxW(nullptr,
        L"ReChan stopped unexpectedly. Session log and crash files are in the logs folder.\n"
        L"Keep the matching log, crash report and dump for diagnosis. Nothing is uploaded automatically.",
        L"ReChan", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#endif
}

static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (InterlockedCompareExchange(&s_handlingCrash, 1, 0) == 0) {
        WriteCrashReport(exceptionInfo, "Unhandled structured exception");
        NotifyUser();
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] static void TerminateHandler() {
    if (InterlockedCompareExchange(&s_handlingCrash, 1, 0) == 0) {
        WriteCrashReport(nullptr, "std::terminate"); NotifyUser();
    }
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

static void CrtSignalHandler(int signalNumber) {
    if (InterlockedCompareExchange(&s_handlingCrash, 1, 0) == 0) {
        char reason[80];
        std::snprintf(reason, sizeof(reason), "C runtime fatal signal %d", signalNumber);
        WriteCrashReport(nullptr, reason); NotifyUser();
    }
    TerminateProcess(GetCurrentProcess(), (UINT)(128 + signalNumber));
}

void CrashReporter::Install() {
    const auto& stem = Log::Get().GetSessionStem();
    const auto& log = Log::Get().GetSessionPath();
    const auto cache = [](const std::string& path, wchar_t* target, int capacity) {
        return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, target, capacity) > 0;
    };
    cache(stem + "-crash.txt", s_reportPath, (int)std::size(s_reportPath));
    cache(stem + "-crash.dmp", s_dumpPath, (int)std::size(s_dumpPath));
    cache(log, s_logPath, (int)std::size(s_logPath));
    std::snprintf(s_logUtf8, sizeof(s_logUtf8), "%s", log.c_str());
    s_suppressDialog = std::getenv("RECHAN_CRASH_REPORTER_NO_DIALOG") != nullptr;
    ULONG guarantee = 64 * 1024;
    SetThreadStackGuarantee(&guarantee);
    SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    std::set_terminate(TerminateHandler);
    std::signal(SIGABRT, CrtSignalHandler);
    LOG_ALWAYS("[CrashReporter] Session paths prepared; native dump enabled");
}

void CrashReporter::ShowFatalMessage(const char* title, const char* message) {
    LOG_ALWAYS("[Session] handled-error: %s", message ? message : "unknown");
    std::fprintf(stderr, "%s: %s\n", title, message);
#if !defined(RC_PLATFORM_NULL)
    if (!s_suppressDialog) MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR | MB_TASKMODAL);
#endif
}

#elif defined(RC_PLATFORM_SWITCH)

// Deliberately minimal no-op for this toolchain-validation milestone: no
// <execinfo.h> (glibc-only, absent from newlib), no fork()/execlp() (no
// process model on Horizon OS), no sigaction-based fault handling (Switch
// has no equivalent POSIX signal delivery for hardware faults -- real crash
// reporting here would hook libnx's own fatal-error/diagFatal path instead,
// left for a later pass once there's an actual Switch build to crash).
void CrashReporter::Install() {}

void CrashReporter::ShowFatalMessage(const char* title, const char* message) {
    std::printf("%s: %s\n", title, message);
}

#else

#include <cerrno>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t s_handlingCrash = 0;
static char s_reportPath[PATH_MAX] = "rechan-crash.txt";
static bool s_suppressDialog = false;

static bool EnsureMinidumpDirectory() {
    if (mkdir("minidumps", 0755) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info = {};
    return stat("minidumps", &info) == 0 && S_ISDIR(info.st_mode);
}

static void WriteFd(int fd, const char* text) {
    if (fd < 0 || !text) return;
    const size_t length = std::strlen(text);
    size_t offset = 0;
    while (offset < length) {
        const ssize_t written = write(fd, text + offset, length - offset);
        if (written <= 0) break;
        offset += static_cast<size_t>(written);
    }
}

static void AppendFileToFd(int output, const char* path) {
    const int input = open(path, O_RDONLY);
    if (input < 0) return;
    char buffer[4096];
    ssize_t count = 0;
    while ((count = read(input, buffer, sizeof(buffer))) > 0) {
        size_t offset = 0;
        while (offset < static_cast<size_t>(count)) {
            const ssize_t written = write(output, buffer + offset,
                                          static_cast<size_t>(count) - offset);
            if (written <= 0) break;
            offset += static_cast<size_t>(written);
        }
    }
    close(input);
}

static const char* SignalName(int signalNumber) {
    switch (signalNumber) {
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGSEGV: return "SIGSEGV";
        default:      return "unknown signal";
    }
}

static void NotifyUser() {
    char message[1024];
    std::snprintf(message, sizeof(message),
                  "ReChan crashed and created %s.\n\n"
                  "Please open an issue and attach that report:\n%s",
                  s_reportPath, kIssueUrl);
    WriteFd(STDERR_FILENO, "\n");
    WriteFd(STDERR_FILENO, message);
    WriteFd(STDERR_FILENO, "\n");

#if defined(RC_PLATFORM_NULL)
    // Headless/null-hardware build: no display server to show a dialog on.
    // The stderr write above is the only notification.
#else
    if (s_suppressDialog) return;

    const pid_t child = fork();
    if (child == 0) {
        execlp("zenity", "zenity", "--error", "--title=ReChan crashed",
               "--width=520", "--text", message, static_cast<char*>(nullptr));
        execlp("xmessage", "xmessage", "-center", message, static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
}

static void FatalSignalHandler(int signalNumber, siginfo_t* signalInfo, void*) {
    if (s_handlingCrash) {
        _exit(128 + signalNumber);
    }
    s_handlingCrash = 1;

    // Create the minidumps directory only now, at crash time, so nothing is
    // written to the filesystem on a clean run.
    const bool hasDirectory = EnsureMinidumpDirectory();
    std::snprintf(s_reportPath, sizeof(s_reportPath),
                  hasDirectory ? "minidumps/rechan-crash-%ld.txt" : "rechan-crash-%ld.txt",
                  static_cast<long>(getpid()));

    const int report = open(s_reportPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (report >= 0) {
        char buffer[2048];
        std::snprintf(buffer, sizeof(buffer),
                      "ReChan crash report\n"
                      "===================\n"
                      "Version: %s\n"
                      "Platform: Linux x86_64\n"
                      "Process ID: %ld\n"
                      "Signal: %s (%d)\n"
                      "Signal code: %d\n"
                      "Fault address: %p\n\n"
                      "Native stack trace:\n",
                      GAME_VERSION, static_cast<long>(getpid()), SignalName(signalNumber),
                      signalNumber, signalInfo ? signalInfo->si_code : 0,
                      signalInfo ? signalInfo->si_addr : nullptr);
        WriteFd(report, buffer);

        void* frames[64] = {};
        const int frameCount = backtrace(frames, 64);
        backtrace_symbols_fd(frames, frameCount, report);

        WriteFd(report, "\nProcess memory map:\n");
        AppendFileToFd(report, "/proc/self/maps");
        WriteFd(report, "\nOperating system:\n");
        AppendFileToFd(report, "/etc/os-release");
        WriteFd(report,
                "\nPlease open an issue at:\n"
                "https://github.com/SilverwireGames/ReChan/issues/new\n"
                "Attach this crash report.\n");
        WriteFd(report,
                "\nrechan.log\n"
                "==========\n");
        AppendFileToFd(report, "rechan.log");
        fsync(report);
        close(report);
    }

    NotifyUser();

    struct sigaction action = {};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(signalNumber, &action, nullptr);

    sigset_t unblockSet;
    sigemptyset(&unblockSet);
    sigaddset(&unblockSet, signalNumber);
    sigprocmask(SIG_UNBLOCK, &unblockSet, nullptr);
    kill(getpid(), signalNumber);
    _exit(128 + signalNumber);
}

[[noreturn]] static void TerminateHandler() {
    raise(SIGABRT);
    _exit(134);
}

void CrashReporter::ShowFatalMessage(const char* title, const char* message) {
    std::fprintf(stderr, "%s: %s\n", title, message);
#if !defined(RC_PLATFORM_NULL)
    if (std::getenv("RECHAN_CRASH_REPORTER_NO_DIALOG")) return;

    const pid_t child = fork();
    if (child == 0) {
        execlp("zenity", "zenity", "--error", "--title", title,
               "--width=520", "--text", message, static_cast<char*>(nullptr));
        execlp("xmessage", "xmessage", "-center", message, static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
}

void CrashReporter::Install() {
    s_suppressDialog = std::getenv("RECHAN_CRASH_REPORTER_NO_DIALOG") != nullptr;

    struct sigaction action = {};
    action.sa_sigaction = FatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;

    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    std::set_terminate(TerminateHandler);
}

#endif

[[noreturn]] void CrashReporter::TriggerTestCrash() {
    std::raise(SIGABRT);
    std::abort();
}
