#pragma once
using namespace std;

#include <string>
#include <mutex>
#include <fstream>

class Logger {
public:
    explicit Logger(const string& logPath);
    ~Logger();

    void line(const string& msg);// plain
    void event(const string& tag, const string& msg); // [tag] msg

private:
    mutex mu_;
    ofstream out_;
};

