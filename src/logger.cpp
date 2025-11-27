using namespace std;
#include "logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

//quick helper to stamp logs with local time.
static inline string nowStamp() {
    using clock = chrono::system_clock;
    auto t = clock::to_time_t(clock::now());
    std::tm tm = *std::localtime(&t);   // not thread-safe by itself
    ostringstream os;
    os << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return os.str();
}

Logger::Logger(const string& path) : out_(path, ios::app) {
    if (!out_) throw runtime_error("Logger: cannot open " + path);
}
Logger::~Logger(){
    out_.flush();
}

void Logger::line(const string& msg) {
    lock_guard<std::mutex> lk(mu_);
    out_ << nowStamp() << " " << msg << "\n";
    out_.flush();
}

void Logger::event(const string& tag, const string& msg) {
    lock_guard<std::mutex> lk(mu_);
    out_ << nowStamp() << " [" << tag << "] " << msg << "\n";
    out_.flush();
}
