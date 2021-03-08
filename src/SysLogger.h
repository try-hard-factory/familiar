#ifndef SYS_LOGGER_H
#define SYS_LOGGER_H

#include <string>

class SysLogger
{
public:
    static void log(const std::string& msg);
};

#endif