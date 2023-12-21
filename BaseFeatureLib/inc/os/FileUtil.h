#ifndef __FileUtil_h__
#define __FileUtil_h__
#include "File.h"
#include <stdio.h>
#include <string>
#include <vector>

//---------------------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__

    //------------------------------------------------------------------------------------//
    bool isDirectory(const char* path);

    bool isFileExist(const char* filePath);

    uint64 getFileBytes(const char* filePath);

    uint64 getFileSize(const FILE* file);

    uint64 readFile0(const char* filePath, std::string& buffer);

    uint64 readFile1(const File* file, std::string& buffer);

    void* readFile2(const char* filePath, uint64* size);

    bool saveFile(const char* filePath, const void* data, uint32 size);

    bool getAllFiles(const char* dirPath, std::vector<std::string>& files, char* type);

    void deleteFile(const char* filePath);

__CExternEnd__
__END__

#endif // __FileUtil_h__
