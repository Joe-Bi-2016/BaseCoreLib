/*****************************************************************************
* FileName      : CAssetFile.h
* Description   : Interface to access assetFile by C-file definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
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
