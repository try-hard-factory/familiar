#include "SysLogger.h"

#include <syslog.h>

void SysLogger::log(const std::string& msg) {
    syslog(LOG_INFO, "%s", msg.c_str());
}