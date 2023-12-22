/*****************************************************************************
* FileName      : Logger.cpp
* Description   : Logger implemention, use spdlog in non-android system
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/os/Logger.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../thirdparty/spdlog-1.x/include/spdlog/spdlog.h"
#include "../../thirdparty/spdlog-1.x/include/spdlog/sinks/stdout_color_sinks.h"
#include <assert.h>

//---------------------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__

    static auto console_logger = spdlog::stdout_color_mt("console");
    static bool inited = false;
    static Mutex lock;
    
    //------------------------------------------------------------------------------------//
    void initLogger(void) {
        AutoMutex am(&lock);
        if (inited)
            return;
    
        inited = true;
        console_logger->set_level(spdlog::level::trace);
    }
    
    //------------------------------------------------------------------------------------//
    void logPrint(LogPriority prio, const char* tag, const char* fmt, ...) {
        assert(prio >= LOG_LEVEL_NONE && prio <= LOG_LEVEL_DEBUG);
        assert(tag && fmt);
    
        va_list ap;
        char buf[1024] = { 0 };
        size_t size = strlen(tag);
        memcpy(buf, tag, size);
    
        va_start(ap, fmt);
        vsnprintf(buf + size, (1024 - size), fmt, ap);
        va_end(ap);

        initLogger();
    
        switch (prio) {
        case LOG_LEVEL_DEBUG:
            console_logger->debug(buf);
            break;
    
        case LOG_LEVEL_INFO:
            console_logger->info(buf);
            break;
    
        case LOG_LEVEL_WARNING:
            console_logger->warn(buf);
            break;
    
        case LOG_LEVEL_ERR:
            console_logger->error(buf);
            break;
    
        case LOG_LEVEL_FATAL:
            console_logger->error(buf);
            break;
    
        case LOG_LEVEL_NONE:
            console_logger->trace(buf);

        default:
            printf("%s\n", buf);
            break;
        }
    
    }

    __CExternEnd__
__END__
