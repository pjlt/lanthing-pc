/** ==========================================================================
 * 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#pragma once

#include <inttypes.h>

#include <memory>
#include <string>
#include <vector>

#include <g3log/g3log.hpp>
#include <g3log/logmessage.hpp>

const LEVELS ERR{WARNING.value + 100, "ERROR"};

#define LOG_2(level, file, line, func)                                                             \
    if (!g3::logLevel(level)) {                                                                    \
    }                                                                                              \
    else                                                                                           \
        LogCapture(file, line, func, level).stream()

namespace ltlib {
class LogSink {
public:
    LogSink(const std::string& log_prefix, const std::string& log_directory,
            size_t write_to_log_every_x_message = 30);
    virtual ~LogSink();

    void fileWrite(g3::LogMessageMover message);
    std::string fileName();

private:
    std::string changeLogFile();
    bool isTimeToRoll();
    void tryRemoveOldLogs();

private:
    std::string _log_directory;
    std::string _log_file_with_path;
    std::string _log_prefix_backup; // needed in case of future log file changes of directory
    std::unique_ptr<std::ofstream> _outptr;
    std::string _header;
    bool _firstEntry;
    std::string _write_buffer;
    size_t _write_counter;
    size_t _write_to_log_every_x_message;
    int _last_mday;

    void addLogFileHeader();
    std::ofstream& filestream() { return *(_outptr.get()); }

    LogSink& operator=(const LogSink&) = delete;
    LogSink(const LogSink& other) = delete;
};

} // namespace ltlib