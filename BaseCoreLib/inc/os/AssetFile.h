/*****************************************************************************
* FileName    : AssetFile.h
* Description : Android system access file function definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __AssetFile_h__
#define __AssetFile_h__
#include "File.h"

//---------------------------------------------------------------------------//
__BEGIN__

    #if defined(__ANDROID__)
        class API_EXPORTS AssetFile : public File
        {
        public:
            AssetFile();
            AssetFile(const char* path, const char* mode);
            ~AssetFile();
            
        public:
            bool fopen(const std::string& path, const std::string& mode);
            void fclose();
            uint64 fread(void* buffer, uint32 size, uint32 count);
            uint64 fwrite(const void* pVoid, uint32 size, uint32 count);
            char* fgets(char* buffer, int bufferSize);
            void fprintf(const char* format, ...);
            uint64 size();
            bool eof(void);
            void resetRead(void);
            void resetWrite(void);
            std::string& getPath();
        
        public:
            static void setAssetManager(void* manager);
            static void* getAssetManager(void);

        private:
            uint64 mSize;
        };
    #else
         #define AssetFile  File
    #endif

__END__

#endif //__AssetFile_h__
