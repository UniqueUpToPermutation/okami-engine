#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// Timestamp — high-resolution wall-clock time with nanosecond precision.
//
// Stored in the asset graph YAML cache as an ISO 8601 string, e.g.
//   "2026-03-19T14:05:32.001234567Z"
// ---------------------------------------------------------------------------

using Timestamp = std::chrono::time_point<std::chrono::system_clock,
                                          std::chrono::nanoseconds>;

// Sentinel representing "never built"; always considered stale.
inline const Timestamp kUnbuiltTimestamp{};

// Convert a std::filesystem::file_time_type to a Timestamp.
//
// Portable across C++17 implementations where the file clock epoch is
// implementation-defined (clock_cast is C++20 only).  Maps through the
// live wall-clock offset between the two clocks.
inline Timestamp FileTimeToTimestamp(std::filesystem::file_time_type ft) {
    using namespace std::chrono;
    namespace fs = std::filesystem;

    auto scNow = system_clock::now();
    auto ftNow = fs::file_time_type::clock::now();

    long long ftNowNs = duration_cast<nanoseconds>(ftNow.time_since_epoch()).count();
    long long scNowNs = duration_cast<nanoseconds>(scNow.time_since_epoch()).count();
    long long fileNs  = duration_cast<nanoseconds>(ft.time_since_epoch()).count();

    return Timestamp(nanoseconds(fileNs + (scNowNs - ftNowNs)));
}

// ---------------------------------------------------------------------------
// FileSystem — abstract interface for all filesystem I/O used by ResourceGraph.
//
// Inject a MockFileSystem in tests; use RealFileSystem (the default) in
// production builds.  The interface covers exactly the operations that
// ResourceGraph::LoadCache, SaveCache, Build, and RebuildNode perform.
// ---------------------------------------------------------------------------

class FileSystem {
public:
    virtual ~FileSystem() = default;

    // Returns true if 'path' refers to an existing file or directory.
    virtual bool Exists(const std::filesystem::path& path) const = 0;

    // Returns the last write time of 'path' as a Timestamp.
    // Behaviour is undefined (may throw) if the path does not exist.
    virtual Timestamp LastWriteTime(const std::filesystem::path& path) const = 0;

    // Creates 'path' and all missing intermediate directories.
    // A no-op if the directory already exists.
    virtual void CreateDirectories(const std::filesystem::path& path) = 0;

    // Writes 'text' to 'path', replacing any existing file contents.
    virtual void WriteText(const std::filesystem::path& path,
                           std::string_view text) = 0;

    // Reads and returns the entire contents of 'path' as a string.
    // Throws std::runtime_error if the file cannot be opened.
    virtual std::string ReadText(const std::filesystem::path& path) const = 0;

    // Returns the current wall-clock time as a Timestamp.
    // Tests override this to return a controlled, monotonically-increasing value
    // so that rebuild decisions are deterministic and timestamp-order is clear.
    virtual Timestamp Now() const {
        return std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now());
    }
};

// ---------------------------------------------------------------------------
// RealFileSystem — production implementation backed by std::filesystem.
// ---------------------------------------------------------------------------

class RealFileSystem : public FileSystem {
public:
    bool Exists(const std::filesystem::path& path) const override {
        return std::filesystem::exists(path);
    }

    Timestamp LastWriteTime(const std::filesystem::path& path) const override {
        return FileTimeToTimestamp(std::filesystem::last_write_time(path));
    }

    void CreateDirectories(const std::filesystem::path& path) override {
        std::filesystem::create_directories(path);
    }

    void WriteText(const std::filesystem::path& path,
                   std::string_view text) override {
        std::ofstream f(path);
        if (!f)
            throw std::runtime_error("Cannot write file: " + path.string());
        f << text;
    }

    std::string ReadText(const std::filesystem::path& path) const override {
        std::ifstream f(path);
        if (!f)
            throw std::runtime_error("Cannot read file: " + path.string());
        return std::string(std::istreambuf_iterator<char>(f), {});
    }

    // Returns the single shared instance (default used by ResourceGraph).
    static RealFileSystem& Instance() {
        static RealFileSystem inst;
        return inst;
    }
};
