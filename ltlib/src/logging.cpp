/** ==========================================================================
 * 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#include <ltlib/logging.h>

#include <cassert>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>

#include <ltlib/times.h>

namespace {

const std::string file_name_time_formatted = "%Y%m%d-%H%M%S";
const std::string time_formatted = "%H:%M:%S.%f3";
const std::string date_formatted = "%Y/%m/%d";
const std::string kFractionalIdentier = "%f";
//const size_t kFractionalIdentierSize = 2;

std::string header(const std::string& headerFormat) {
    std::ostringstream ss_entry;
    //  Day Month Date Time Year: is written as "%a %b %d %H:%M:%S %Y" and formatted output as : Wed
    //  Sep 19 08:28:16 2012
    auto now = std::chrono::system_clock::now();
    ss_entry << "\t\tltlib::LogSink created log at: "
             << g3::localtime_formatted(now, "%a %b %d %H:%M:%S %Y") << "\n";
    ss_entry << headerFormat;
    return ss_entry.str();
}

bool isValidFilename(const std::string& prefix_filename) {
    std::string illegal_characters("/,|<>:#$%{}[]\'\"^!?+* ");
    size_t pos = prefix_filename.find_first_of(illegal_characters, 0);
    if (pos != std::string::npos) {
        std::cerr << "Illegal character [" << prefix_filename.at(pos) << "] in logname prefix: "
                  << "[" << prefix_filename << "]" << std::endl;
        return false;
    }
    else if (prefix_filename.empty()) {
        std::cerr << "Empty filename prefix is not allowed" << std::endl;
        return false;
    }

    return true;
}

std::string prefixSanityFix(std::string prefix) {
    prefix.erase(std::remove_if(prefix.begin(), prefix.end(), ::isspace), prefix.end());
    prefix.erase(std::remove(prefix.begin(), prefix.end(), '/'), prefix.end());
    prefix.erase(std::remove(prefix.begin(), prefix.end(), '\\'), prefix.end());
    prefix.erase(std::remove(prefix.begin(), prefix.end(), '.'), prefix.end());
    prefix.erase(std::remove(prefix.begin(), prefix.end(), ':'), prefix.end());
    if (!isValidFilename(prefix)) {
        return {};
    }
    return prefix;
}

std::string createLogFileName(const std::string& verified_prefix) {
    std::stringstream oss_name;
    oss_name << verified_prefix << ".";
    auto now = std::chrono::system_clock::now();
    oss_name << g3::localtime_formatted(now, file_name_time_formatted);
    oss_name << ".log";
    return oss_name.str();
}

std::string pathSanityFix(std::string path, std::string file_name) {
    // Unify the delimeters,. maybe sketchy solution but it seems to work
    // on at least win7 + ubuntu. All bets are off for older windows
    std::replace(path.begin(), path.end(), '\\', '/');

    // clean up in case of multiples
    auto contains_end = [&](std::string& in) -> bool {
        size_t size = in.size();
        if (!size)
            return false;
        char end = in[size - 1];
        return (end == '/' || end == ' ');
    };

    while (contains_end(path)) {
        path.erase(path.size() - 1);
    }

    if (!path.empty()) {
        path.insert(path.end(), '/');
    }

    path.insert(path.size(), file_name);
    return path;
}

bool openLogFile(const std::string& complete_file_with_path, std::ofstream& outstream) {
    std::ios_base::openmode mode =
        std::ios_base::out; // for clarity: it's really overkill since it's an ofstream
    mode |= std::ios_base::trunc;
    outstream.open(complete_file_with_path, mode);
    if (!outstream.is_open()) {
        std::ostringstream ss_error;
        ss_error << "FILE ERROR:  could not open log file:[" << complete_file_with_path << "]";
        ss_error << "\n\t\t std::ios_base state = " << outstream.rdstate();
        std::cerr << ss_error.str().c_str() << std::endl;
        outstream.close();
        return false;
    }
    return true;
}

std::unique_ptr<std::ofstream> createLogFile(const std::string& file_with_full_path) {
    std::unique_ptr<std::ofstream> out(new std::ofstream);
    std::ofstream& stream(*(out.get()));
    bool success_with_open_file = openLogFile(file_with_full_path, stream);
    if (false == success_with_open_file) {
        out.reset();
    }
    return out;
}

tm localtime(const std::time_t& ts) {
    struct tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
    localtime_s(&tm_snapshot, &ts); // windsows
#else
    localtime_r(&ts, &tm_snapshot); // POSIX
#endif
    return tm_snapshot;
}

/*
std::string localtime_formatted_fractions(const g3::system_time_point& ts, std::string
format_buffer)
{
    // iterating through every "%f" instance in the format string
    auto identifierExtraSize = 0;
    for (size_t pos = 0;
         (pos = format_buffer.find(kFractionalIdentier, pos)) != std::string::npos;
         pos += kFractionalIdentierSize + identifierExtraSize) {
        // figuring out whether this is nano, micro or milli identifier
        auto type = g3::internal::getFractional(format_buffer, pos);
        auto value = g3::internal::to_string(ts, type);
        auto padding = 0;
        if (type != g3::internal::Fractional::NanosecondDefault) {
            padding = 1;
        }

        // replacing "%f[3|6|9]" with sec fractional part value
        format_buffer.replace(pos, kFractionalIdentier.size() + padding, value);
    }
    return format_buffer;
}
*/

std::string logDetailsToString(const g3::LogMessage& msg) {
    std::stringstream ss;
    ss << '[' << msg.timestamp({date_formatted + " " + time_formatted}) << "][" << msg.threadID()
       << "][" << msg.level() << "][" << msg.file() << ':' << msg.line() << "] ";
    return ss.str();
}

} // namespace

namespace ltlib {

LogSink::LogSink(const std::string& log_prefix, const std::string& log_directory,
                 size_t write_to_log_every_x_message)
    : _log_directory(log_directory)
    , _log_prefix_backup(log_prefix)
    , _outptr(new std::ofstream)
    , _header("\t\tLOG format: [YYYY/MM/DD hh:mm:ss.uuu][THREAD][LEVEL][FILE:LINE] message\n\n")
    , _firstEntry(true)
    , _write_counter(0)
    , _write_to_log_every_x_message(write_to_log_every_x_message) {
    auto time_point = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm t = localtime(time_point);
    _last_mday = t.tm_mday + 1;

    _log_prefix_backup = prefixSanityFix(log_prefix);
    if (!isValidFilename(_log_prefix_backup)) {
        std::cerr << "g3log: forced abort due to illegal log prefix [" << log_prefix << "]"
                  << std::endl;
        abort();
    }

    std::string file_name = createLogFileName(_log_prefix_backup);
    _log_file_with_path = pathSanityFix(_log_directory, file_name);
    _outptr = createLogFile(_log_file_with_path);

    if (!_outptr) {
        std::cerr << "Cannot write log file to location, attempting current directory" << std::endl;
        _log_file_with_path = "./" + file_name;
        _outptr = createLogFile(_log_file_with_path);
    }
    assert(_outptr && "cannot open log file at startup");
}

LogSink::~LogSink() {
    std::string exit_msg = {"\ng3log with ltlib::LogSink shutdown at: "};
    auto now = std::chrono::system_clock::now();
    exit_msg.append(g3::localtime_formatted(now, time_formatted)).append("\n");

    // write anything buffered up and then end with the exit msg
    filestream() << _write_buffer << exit_msg << std::flush;

    exit_msg.append("Log file at: [").append(_log_file_with_path).append("]\n");
    std::cerr << exit_msg << std::flush;
}

void LogSink::fileWrite(g3::LogMessageMover message) {
    if (_firstEntry) {
        addLogFileHeader();
        _firstEntry = false;
        tryRemoveOldLogs();
    }
    if (isTimeToRoll()) {
        changeLogFile();
        tryRemoveOldLogs();
    }

    auto data = message.get().toString(&logDetailsToString);

    _write_buffer.append(data);
    if (++_write_counter % _write_to_log_every_x_message == 0) {
        filestream() << _write_buffer << std::flush;
        _write_buffer.clear();
    }
}

std::string LogSink::changeLogFile() {
    std::string file_name = createLogFileName(_log_prefix_backup);
    auto log_file_with_path = pathSanityFix(_log_directory, file_name);
    auto new_outptr = createLogFile(log_file_with_path);
    if (!new_outptr) {
        filestream() << "Cannot write log file to location, attempting current directory\n";
        log_file_with_path = "./" + file_name;
        new_outptr = createLogFile(log_file_with_path);
    }
    _log_file_with_path = log_file_with_path;
    _outptr = std::move(new_outptr);
    addLogFileHeader();
    return _log_file_with_path;
}

bool LogSink::isTimeToRoll() {
    auto time_point = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm t = localtime(time_point);
    if (t.tm_mday + 1 != _last_mday) {
        _last_mday = t.tm_mday + 1;
        return true;
    }
    else {
        return false;
    }
}

void LogSink::tryRemoveOldLogs() {
    auto expire_tp = std::chrono::system_clock::now() - std::chrono::hours{24 * 7};
    auto expire_time_t = std::chrono::system_clock::to_time_t(expire_tp);
    struct tm expire_tm = localtime(expire_time_t);
    expire_tm.tm_year += 1900;
    expire_tm.tm_mday += 1;
    int expire_date = expire_tm.tm_year * 10000 + (expire_tm.tm_mon + 1) * 100 + expire_tm.tm_mday;
    std::regex pattern{".+?([0-9]+?)-.+?"};
    std::filesystem::path directory{_log_directory};
    for (const auto& file : std::filesystem::directory_iterator{directory}) {

        std::smatch sm;
        std::string filename = file.path().string();
        if (!std::regex_match(filename, sm, pattern)) {
            continue;
        }
        int file_date = atoi(sm[1].str().c_str());
        if (file_date < expire_date) {
            remove(filename.c_str());
        }
    }
}

std::string LogSink::fileName() {
    return _log_file_with_path;
}

void LogSink::addLogFileHeader() {
    filestream() << header(_header);
}

} // namespace ltlib
