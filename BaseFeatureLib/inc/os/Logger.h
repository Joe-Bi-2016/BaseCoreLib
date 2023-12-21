#ifndef __Logger_h__
#define __Logger_h__
#if defined(__ANDROID__)
#include <android/log.h>
#endif
#include "../../inc/base/Macro.h"

//---------------------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__

    //------------------------------------------------------------------------------------//
    typedef enum
    {
        LOG_LEVEL_NONE = 0,
        LOG_LEVEL_FATAL = 1,
        LOG_LEVEL_ERR = 2,
        LOG_LEVEL_WARNING = 3,
        LOG_LEVEL_INFO = 4,
        LOG_LEVEL_DEBUG = 5
    } LogPriority;
    
    // Note: you should define LOG_TAG before using follow macros
    #define LOG_TAGD(LOG_TAG)    STR(D:)##STR(LOG_TAG)
    #define LOG_TAGI(LOG_TAG)      STR(I:)##STR(LOG_TAG)
    #define LOG_TAGW(LOG_TAG)   STR(W:)##STR(LOG_TAG)
    #define LOG_TAGE(LOG_TAG)     STR(E:)##STR(LOG_TAG)
    #define LOG_TAGF(LOG_TAG)     STR(F:)##STR(LOG_TAG)
    
    void logPrint(LogPriority prio, const char* tag, const char* fmt, ...);
    
    #ifndef LOG
        #define LOG(priority, tag, ...) \
        logPrint(priority, tag, __VA_ARGS__)
    #endif
    
    #if defined(__ANDROID__)
    #define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAGD, __VA_ARGS__ )
    #define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  LOG_TAGI, __VA_ARGS__ )
    #define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,  LOG_TAGW, __VA_ARGS__ )
    #define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAGE, __VA_ARGS__)
    #define  LOGF(...)  __android_log_print(ANDROID_LOG_FATAL, LOG_TAGF, __VA_ARGS__)
    #else
    #define  LOGD(...)  ((void)LOG(LOG_LEVEL_DEBUG, LOG_TAGD(LOG_TAG), __VA_ARGS__ ))
    #define  LOGI(...)  ((void)LOG(LOG_LEVEL_INFO,  LOG_TAGI(LOG_TAG), __VA_ARGS__ ))
    #define  LOGW(...)  ((void)LOG(LOG_LEVEL_WARNING,  LOG_TAGW(LOG_TAG), __VA_ARGS__ ))
    #define  LOGE(...)  ((void)LOG(LOG_LEVEL_ERR, LOG_TAGE(LOG_TAG), __VA_ARGS__))
    #define  LOGF(...)  ((void)LOG(LOG_LEVEL_FATAL, LOG_TAGF(LOG_TAG), __VA_ARGS__))
    #endif

    __CExternEnd__
__END__

#endif // __Logger_h__
