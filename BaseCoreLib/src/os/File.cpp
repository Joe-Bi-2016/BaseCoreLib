/*****************************************************************************
* FileName    : File.cpp
* Description : File implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/os/File.h"
#include "../../inc/os/FileUtil.h"
#include "../../inc/os/Logger.h"
#include <errno.h>
#include <stdio.h>
#if defined(__ANDROID__)
#include <unistd.h>
#include <android/asset_manager.h>
#endif
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <shlwapi.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif
#include <stdarg.h>
#include <assert.h>

//------------------------------------------------------------------------------------//
# pragma warning (disable:4819)

//---------------------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (FileUtils):

    __CExternBegin__
    //------------------------------------------------------------------------------------//
    #ifdef _MSC_VER
        #define _CRT_SECURE_NO_WARNINGS 1
        #define restrict __restrict
    #else
        #define restrict __restrict__
    #endif

    int64_t my_getline(char** restrict line, size_t* restrict len, FILE *restrict fp) 
    {
        // Check if either line, len or fp are nullptr pointers
        if (line == nullptr || len == nullptr || fp == nullptr)
        {
            errno = EINVAL;
            return -1;
        }

        // Use a chunk array of 128 bytes as parameter for fgets
        char chunk[128];
        // Allocate a block of memory for *line if it is nullptr or smaller than the chunk array
        if (*line == nullptr || *len < sizeof(chunk))
        {
            *len = sizeof(chunk);
            if ((*line = (char*)malloc(*len)) == nullptr)
            {
                errno = ENOMEM;
                return -1;
            }
        }

        // "Empty" the string
        (*line)[0] = '\0';

        while (fgets(chunk, sizeof(chunk), fp) != nullptr)
        {
            // Resize the line buffer if necessary
            size_t len_used = strlen(*line);
            size_t chunk_used = strlen(chunk);

            if (*len - len_used < chunk_used)
            {
                // Check for overflow
                if (*len > SIZE_MAX / 2)
                {
                    errno = EOVERFLOW;
                    return -1;
                }
                else
                    *len *= 2;

                if ((*line = (char*)realloc(*line, *len)) == nullptr)
                {
                    errno = ENOMEM;
                    return -1;
                }
            }

            // Copy the chunk to the end of the line buffer
            memcpy(*line + len_used, chunk, chunk_used);
            len_used += chunk_used;
            (*line)[len_used] = '\0';

            // Check if *line contains '\n', if yes, return the *line length
            if ((*line)[len_used - 1] == '\n')
                return len_used;
        }

        return -1;
    }
    __CExternEnd__

   //------------------------------------------------------------------------------------//
    File::File(void)
    : mPath("")
    , mMode("rw")
    , mFile(nullptr)
    , mNeedClosed(true)
    , mReadPos(0)
    , mWritePos(0)
    , mAssetManager(nullptr)
    { }

   //------------------------------------------------------------------------------------//
    File::File(const FILE* file)
    : mPath("")
    , mMode("rw")
    , mFile(const_cast<FILE*>(file))
    , mNeedClosed(false)
    , mReadPos(0)
    , mWritePos(0)
    , mAssetManager(nullptr)
    { }

   //------------------------------------------------------------------------------------//
    File::File(const File& file)
    : mPath(file.mPath)
    , mMode(file.mMode)
    , mFile(file.mFile)
    , mNeedClosed(file.mNeedClosed)
    , mReadPos(file.mReadPos)
    , mWritePos(file.mWritePos)
    , mAssetManager(file.mAssetManager)
    {
    }

   //------------------------------------------------------------------------------------//
    File::File(File&& file)
    : mPath(std::move(file.mPath))
    , mMode(std::move(file.mMode))
    , mFile(file.mFile)
    , mNeedClosed(file.mNeedClosed)
    , mReadPos(file.mReadPos)
    , mWritePos(file.mWritePos)
    , mAssetManager(file.mAssetManager)
    { 
        file.mPath = "";
        file.mMode = "rw";
        file.mFile = nullptr;
        file.mNeedClosed = false;
        file.mReadPos = 0;
        file.mWritePos = 0;
        file.mAssetManager = nullptr;
    }

   //------------------------------------------------------------------------------------//
    File::File(const std::string& path, const std::string& mode /* = "rw" */)
    : mPath(path)
    , mMode(mode)
    , mFile(nullptr)
    , mNeedClosed(true)
    , mReadPos(0)
    , mWritePos(0)
    , mAssetManager(nullptr) 
    {
        fopen(path, mode);
    }

   //------------------------------------------------------------------------------------//
    File& File::operator=(const File& file)
    {
        if (this != &file)
        {
            release();
            mPath = file.mPath;
            mMode = file.mMode;
            mFile = file.mFile;
            mNeedClosed = file.mNeedClosed;
            mReadPos = file.mReadPos;
            mWritePos = file.mWritePos;
            mAssetManager = file.mAssetManager;
        }

        return *this;
    }

   //------------------------------------------------------------------------------------//
    File& File::operator=(File&& file)
    {
        if(this != &file)
        {
            release();
            mPath = std::move(file.mPath);
            mMode = std::move(file.mMode);
            mFile = file.mFile;
            mNeedClosed = file.mNeedClosed;
            mReadPos = file.mReadPos;
            mWritePos = file.mWritePos;
            mAssetManager = file.mAssetManager;

            file.mPath = "";
            file.mMode = "";
            file.mFile = nullptr;
            file.mNeedClosed = false;
            file.mReadPos = 0;
            file.mWritePos = 0;
            file.mAssetManager = nullptr;
        }

        return *this;
    }

   //------------------------------------------------------------------------------------//
    File::~File(void)
    {
        release();        
    }

   //------------------------------------------------------------------------------------//
#if defined(__ANDROID__)
    void File::setAssetManager(const void* assetManager)
    {
        mAssetManager = (void*)(assetManager);
    }
#endif

   //------------------------------------------------------------------------------------//
    bool File::fopen(const std::string& path, const std::string& mode /* = "rw" */)
    {
        if (path.length() == 0 || mode.length() == 0)
        {
            LOGE("%s", "file path is null");
            return false;
        }
        if (mFile)
        {
            LOGW("%s", "file had been opened");
            return true;
        }

#if defined(__ANDROID__)
        if (mAssetManager) 
        {
            if (mode[0] == 'w')
                return false;

            mFile = AAssetManager_open((AAssetManager*)mAssetManager, path.c_str(), AASSET_MODE_UNKNOWN);
        } 
        else
#endif
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            fopen_s((FILE**) & mFile, path.c_str(), mode.c_str());
#else
            mFile = ::fopen(path.c_str(), mode.c_str());
#endif
        mPath = path;
        mMode = mode;

        return mFile ? true : false;
    }

   //------------------------------------------------------------------------------------//
    void File::fclose(void)
    {
        if(mFile && mNeedClosed)
        {
#if defined(__ANDROID__) 
            if (mAssetManager) 
                AAsset_close((AAsset*)mFile);
            else
#endif             
                ::fclose((FILE*)mFile);

            mFile = nullptr;
            mReadPos = 0;
            mWritePos = 0;
            mAssetManager = nullptr;
        }
    }

   //------------------------------------------------------------------------------------//
    size_t File::fread(void* buffer, long size, long count)
    {
        size_t r = 0;
        if(mFile && buffer && size > 0 && count > 0)
        {
#if defined(__ANDROID__)
            if(mAssetManager)
            {
                AAsset *pAAsset = (AAsset *)mFile;
                AAsset_seek(pAAsset, mReadPos, 0);
                r = AAsset_read(pAAsset, buffer, (size * count));
                mReadPos += (r > 0 ? r : 0);
            }
            else
            {
#endif
                ::fseek((FILE*)mFile, mReadPos, SEEK_SET);
                r = ::fread(buffer, size, count, (FILE*)mFile);
                mReadPos = ::ftell((FILE*)mFile);
#if defined(__ANDROID__)            
            }
#endif            
        }

        return r;
    }

   //------------------------------------------------------------------------------------//
    size_t File::fwrite(const void* buffer, long size, long count)
    {
#if defined(__ANDROID__)
        if (mAssetManager)
            return EACCES;
#endif       
        size_t w = 0;
        if(mFile && buffer && size > 0 && count > 0)
        {
            ::fseek((FILE*)mFile, mWritePos, SEEK_SET);
            w = ::fwrite(buffer, size, count, (FILE*)mFile);
            fflush((FILE*)mFile);
            mWritePos = ftell((FILE*)mFile);
        }

        return w;
    }

   //------------------------------------------------------------------------------------//
    char* File::fgets(char* buffer, int bufferSize)
    {
        assert(buffer && bufferSize > 0);

#if defined(__ANDROID__)
        if(mAssetManager)
        {
            AAsset *pAAsset = (AAsset *)mFile;
            int r = AAsset_getRemainingLength(pAAsset);
            AAsset_seek(pAAsset, mReadPos, 0);
            int realReadCnt = (r > bufferSize ? bufferSize : r);
            AAsset_read(pAAsset, buffer, realReadCnt);
            mReadPos += realReadCnt;
            return buffer;
        }
#endif         
        char* r = 0;
        if(mFile && buffer)
        {
            ::fseek((FILE*)mFile, mReadPos, SEEK_SET);
            r = ::fgets(buffer, bufferSize, (FILE*)mFile);
            mReadPos = ::ftell((FILE*)mFile);
        }

        return r;
    }

   //------------------------------------------------------------------------------------//
    void File::fprintf(const char* format, ...)
    {
#if defined(__ANDROID__)
        if(mAssetManager)
            return;
#endif  

        if(!mFile)
            return;
        
        va_list v;
        va_start(v, format);
        ::fseek((FILE*)mFile, mWritePos, SEEK_SET);
        vfprintf((FILE*)mFile, format, v);
        mWritePos = ::ftell((FILE*)mFile);
        va_end(v);
    }

   //------------------------------------------------------------------------------------//
    void File::syncfile(void)
    {
        if(mFile)
        {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            int fd = _fileno((FILE*)mFile);
            HANDLE h = (HANDLE)_get_osfhandle(fd);
            FlushFileBuffers(h);
#else      
            int fd = fileno((FILE*)mFile);
            fsync(fd);
#endif 
        }
    }

   //------------------------------------------------------------------------------------//
    long File::size(void)
    {
        if(mFile)
        {
#if defined(__ANDROID__)
            if(mAssetManager)
            {
                AAsset *pAAsset = (AAsset *)mFile;
                AAsset_seek(pAAsset, 0, 0);
                int64 size = AAsset_getLength64(pAAsset);
                AAsset_seek(pAAsset, mReadPos, 0);
                return size;
            }
#endif            
            fpos_t pos;
            ::fgetpos((FILE*)mFile, &pos);
            ::fseek((FILE*)mFile, 0, SEEK_END);
            long size = ::ftell((FILE*)mFile);
            ::fsetpos((FILE*)mFile, &pos);
            return size;
        }
        else
            return 0;    
    }

   //------------------------------------------------------------------------------------//
    bool File::eof(void)
    {
        if(mFile)
        {
#if defined(__ANDROID__)
            if(mAssetManager)
                return (AAsset_getRemainingLength64((AAsset*)mFile) == 0);
#endif
            return (feof((FILE*)mFile) != 0);
        }

        return true;    
    }

   //------------------------------------------------------------------------------------//
    void File::resetRead(void)
    {
        mReadPos = 0;
    }

   //------------------------------------------------------------------------------------//
    void File::resetWrite(void)
    {
        mWritePos = 0;
    }

   //------------------------------------------------------------------------------------//
    std::string& File::getPath(void)
    {
        return mPath;
    }

   //------------------------------------------------------------------------------------//
    bool File::getOneLine(char** buf, size_t* len)
    {
        if(buf == nullptr || mFile == nullptr)
            return false;

        size_t L = 0;
        int64 readnum = 0;
        bool ret = false;
        if ((readnum = my_getline(buf, &L, (FILE*)mFile)) != -1)
        {
            if(len)
                *len = L;
            ret = true;
        }

        return ret;
    }

   //------------------------------------------------------------------------------------//
    void File::release(void)
    {
        if(mNeedClosed)
            fclose();
        
        mPath = "";
        mMode = "";
        mFile = nullptr;
        mNeedClosed = false;
        mReadPos = 0;
        mWritePos = 0;
        mAssetManager = nullptr;
    }

   //------------------------------------------------------------------------------------//
   bool FileUtil::isDirectory(const char *dir)
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        return PathIsDirectoryA(dir) ? true : false;
#else
        DIR *pdir = ::opendir(dir);
        if (pdir)
        {
            ::closedir(pdir);
            return true;
        }
        return false;
#endif
    }

   //------------------------------------------------------------------------------------//
    bool FileUtil::isFileExist(const char* filePath)
    {

        FILE *file = nullptr;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        fopen_s(&file, filePath, "rb");
#else
        file = ::fopen(filePath, "rb");
#endif
        if (file)
        {
            ::fclose(file);
            return true;
        }
        return false;
    }

    //------------------------------------------------------------------------------------//
    int64 FileUtil::getFileSize(const char* filePath)
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        struct _stat st; // see : https://msdn.microsoft.com/en-us/library/14h5k7ff.aspx
        if (_stat(filePath, &st) != 0)
            return -1;
        return st.st_size;
#else
        struct stat st;

        if (::stat(filePath, &st) != 0)
        {
            return -1;
        }

        return (int64)st.st_size;
#endif
    }

    //------------------------------------------------------------------------------------//
    uint64 FileUtil::getFileSize(FILE* file)
    {
        if (file)
        {
            ::fseek(file, 0, SEEK_SET);
            uint64 size = ::ftell(file);
            ::fseek(file, 0, SEEK_SET);

            return size;
        }
        return -1;
    }

    //------------------------------------------------------------------------------------//
    uint64 FileUtil::readFile(File* file, std::string& buffer)
    {
        if (!file)
        {
            return 0;
        }

        const int PER_READ_SIZE = 1024;
        uint64 total = 0;
        char data[PER_READ_SIZE + 1];

        while (!file->eof())
        {
            uint64 size = file->fread(data, 1, PER_READ_SIZE);

            if (size > 0)
            {
                data[size] = '\0';
                total += size;
                buffer.append(data, size);
            }

            if (size < PER_READ_SIZE)
            {
                break;
            }
        }

        return total;
    }

    //------------------------------------------------------------------------------------//
    uint64 FileUtil::readFile(const char* filePath, std::string& buffer)
    {
        FILE* file = nullptr;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        fopen_s(&file, filePath, "rb");
#else
        file = ::fopen(filePath, "rb");
#endif

        if (!file)
        {
            return 0;
        }

        const int PER_READ_SIZE = 1024;
        uint64 total = 0;
        char data[PER_READ_SIZE + 1];

        while (!feof(file))
        {
            size_t size = ::fread(data, 1, PER_READ_SIZE, file);

            if (size > 0)
            {
                data[size] = '\0';
                total += size;
                buffer.append(data, size);
            }

            if (size < PER_READ_SIZE)
            {
                break;
            }
        }
        fclose(file);

        return total;
    }

    //------------------------------------------------------------------------------------//
    void *FileUtil::readFile(const char* filePath, uint64* size)
    {
        if (!size)
        {
            return nullptr;
        }

        uint64 len = FileUtil::getFileSize(filePath);

        if (len > 0)
        {
            FILE* file = nullptr;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            fopen_s(&file, filePath, "rb");
#else
            file = ::fopen(filePath, "rb");
#endif
            if (!file)
            {
                return nullptr;
            }
            void *data = malloc(len + 1);

            if (data)
            {
                if (len == ::fread(data, 1, len + 1, file))
                {
                    char *tmp = (char *)data;
                    tmp[len] = '\0';
                    *size = len;
                }
                else
                {
                    free(data);
                }
            }
            ::fclose(file);
            return data;
        }
        return nullptr;
    }

    //------------------------------------------------------------------------------------//
    bool FileUtil::saveFile(const char* filePath, void* data, uint64 size)
    {
        bool ret = false;

        if (data && size > 0)
        {
            FILE* file = nullptr;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            fopen_s(&file, filePath, "wb+");
#else
            file = ::fopen(filePath, "wb+");
#endif
            if (file)
            {
                fseek(file, 0, SEEK_SET);
                ret = size == fwrite(data, size, 1, file);
                fclose(file);
            }
        }
        return ret;
    }

    //------------------------------------------------------------------------------------//
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    bool FileUtil::getAllFiles(const char* dirPath, std::vector<std::string>& files, char* type /*=nullptr*/)
    {
        if (!dirPath)
        {
            return false;
        }
        size_t typeSize = ::strlen(type);
        long long hFile = 0;
        struct _finddata_t fileinfo;
        std::string p;
        std::string ap = std::string("\\*") + type;

        if ((hFile = _findfirst(p.assign(dirPath).append(ap).c_str(), &fileinfo)) != -1)
        {
            do
            {
                files.push_back(fileinfo.name);
            } while (_findnext(hFile, &fileinfo) == 0); //寻找下一个，成功返回0，否则-1
            _findclose(hFile);
        }
        return true;
    }
#else
    bool FileUtil::getAllFiles(const char* dirPath, std::vector<std::string>& files, char* type /*=nullptr*/)
    {
        if (!dirPath)
        {
            return false;
        }
        DIR *dir;
        struct dirent *ptr;
        char absolutePath[500];

        if ((dir = opendir(dirPath)) == nullptr)
        {
            OS_TRACE_ERROR("no exit: %s\n", dirPath);
            return false;
        }
        int typeSize = type ? ::strlen(type) : 0, len;

        while ((ptr = readdir(dir)) != nullptr)
        {
            if (ptr->d_type == 10 //link file
                || strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
            {
                continue;
            }
            else if (ptr->d_type == 8)
            { //file
                if (!type)
                {
                }
                else
                {
                    len = ::strlen(ptr->d_name);
                    if ((len > typeSize) && (0 == ::strcmp(type, ptr->d_name + (len - typeSize))))
                    {

                        memset(absolutePath, '\0', sizeof(absolutePath));
                        strcpy(absolutePath, dirPath);
                        strcat(absolutePath, "/");
                        strcat(absolutePath, ptr->d_name);

                        //                            files.push_back(ptr->d_name);
                        files.push_back(absolutePath);
                        //                            OS_TRACE_DEBUG("file = %s\n", ptr->d_name);
                        OS_TRACE_DEBUG("push_back, ptr->d_name = %s,file absolutePath = %s",
                                       ptr->d_name, absolutePath);
                    }
                }
            }
            else if (ptr->d_type == 4)
            { //dir
                memset(absolutePath, '\0', sizeof(absolutePath));
                strcpy(absolutePath, dirPath);
                strcat(absolutePath, "/");
                strcat(absolutePath, ptr->d_name);
                OS_TRACE_DEBUG("ptr->d_name = %s,dir absolutePath = %s", ptr->d_name, absolutePath);
                getAllFiles(absolutePath, files, type);
            }
        }
        closedir(dir);
        return true;
    }
#endif

    //------------------------------------------------------------------------------------//
    void FileUtil::delFile(const char* filePath)
    {
        if (filePath)
        {
            remove((const char *)filePath);
        }
    }

__END__

