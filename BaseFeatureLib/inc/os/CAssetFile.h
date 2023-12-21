#ifndef __CAssetFile_h__
#define __CAssetFile_h__
#include "../base/Macro.h"

//---------------------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__

        #if defined(__ANDROID__)
            bool hadSetAssetManager(void);
            void setAssetManager(void* assetManager);
            bool fileOpen(const char* path, const char* mode);
            void fileClose(void);
            uint64 fileFread(void* buffer, uint32 size, uint32 count);
            char* fileGets(char* buffer, int bufferSize);
            bool getFileOneLine(char** buf, int* len);
            uint64 size(void);
        #endif

    __CExternEnd__
__END__

#endif // __CAssetFile_h__
