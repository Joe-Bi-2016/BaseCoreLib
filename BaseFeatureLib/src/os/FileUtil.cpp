/*****************************************************************************
* Description: file util implemention.
* Author     : Bi ShengWang(shengwang.bisw@alibaba-inc.com.)
* Date       : 2020.06.11
* Copyright (c) alibaba All rights reserved.
******************************************************************************/
#include "../../inc/os/FileUtil.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <string.h>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <atlstr.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__
   
    //------------------------------------------------------------------------------------//
    bool isDirectory(const char* path)
    {
        int nRet = 0;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        CString str(path);
        str.Replace(_T("/"), _T("\\"));
        struct _stat st = {0};
        nRet = _tstat(str, &st);
#else
        struct stat st;
        nRet = stat(path, &st);
#endif        
        if(nRet != 0)
        {
            printf("error: %s doesn't exist\n", path);
            return false;
        }
        else
        {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))           
            if((st.st_mode & _S_IFDIR) == _S_IFDIR)
#else
            if((st.st_mode & S_IFDIR) == S_IFDIR)
#endif            
                return true;
            else    
                return false;
        }
    }

   //------------------------------------------------------------------------------------//
    bool isFileExist(const char* filePath)
    {
        int nRet = 0;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        CString str(filePath);
        str.Replace(_T("/"), _T("\\"));
        struct _stat st = {0};
        nRet = _tstat(str, &st);
#else
        struct stat st;
        nRet = stat(filePath, &st);
#endif        
        if(nRet != 0)
        {
            printf("error: %s doesn't exist\n", filePath);
            return false;
        }
        else
        {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))        
            if((st.st_mode & _S_IFREG) == _S_IFREG)
#else
            if((st.st_mode & S_IFREG) == S_IFREG)
#endif            
                return true;
            else    
                return false;
        }   
    }

   //------------------------------------------------------------------------------------//
    uint64 getFileBytes(const char* filePath)
    {
        int nRet = 0;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        CString str(filePath);
        str.Replace(_T("/"), _T("\\"));
        struct _stat st = {0};
        nRet = _tstat(str, &st);
#else
        struct stat st;
        nRet = stat(filePath, &st);
#endif        
        if(nRet != 0)
        {
            printf("error: %s doesn't exist\n", filePath);
            return 0;
        }
        else
        {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))            
            if((st.st_mode & _S_IFREG) == _S_IFREG)
#else
            if((st.st_mode & S_IFREG) == S_IFREG)
#endif            
                return st.st_size;
            else    
                return 0;
        }  
    }

   //------------------------------------------------------------------------------------//
    uint64 getFileSize(const FILE* file)
    {
        if(file)
        {
            fpos_t pos;
            ::fgetpos(const_cast<FILE*>(file), &pos);
            ::fseek(const_cast<FILE*>(file), 0, SEEK_END);
            uint64 size = ::ftell(const_cast<FILE*>(file));
            ::fsetpos(const_cast<FILE*>(file), &pos);
            return size;
        }

        return 0;
    }

   //------------------------------------------------------------------------------------//
    uint64 readFile0(const char* filePath, std::string& buffer)
    {
        if(!filePath)
            return 0;

        std::ifstream rf(filePath);
        if(rf.fail())
        {
            printf("error, open file: %s failed\n", filePath);
            return 0;
        }

        rf.seekg(0, std::ios::end);
        size_t size = rf.tellg();
        buffer.reserve(size);
        rf.seekg(0, std::ios::beg);

        buffer.assign((std::istreambuf_iterator<char>(rf)), std::istreambuf_iterator<char>()); 
        rf.close();

        return size;
    }

   //------------------------------------------------------------------------------------//
    uint64 readFile1(const File* file, std::string& buffer)
    {
        if(!file)
            return 0;

        if(!const_cast<File*>(file)->eof())
        {
            long size = const_cast<File*>(file)->size();
            buffer.reserve(size);
            const_cast<File*>(file)->fread((void*)(buffer.data()), 1, size);
            return size;
        }
        else
            return 0;
    }

   //------------------------------------------------------------------------------------//
    void* readFile2(const char* filePath, uint64* size)
    {
        if(!filePath)
            return 0;

        std::ifstream rf(filePath);
        if(rf.fail())
        {
            printf("error, open file: %s failed\n", filePath);
            return nullptr;
        }

        rf.seekg(0, std::ios::end);
        *size = rf.tellg();
        char* buf = (char*)malloc(*size);
        rf.seekg(0, std::ios::beg);
        rf.read(buf, *size);
        rf.close();

        return buf;
    }

   //------------------------------------------------------------------------------------//
    bool saveFile(const char* filePath, const void* data, uint32 size)
    {
        if(!filePath)
            return false;

        std::ofstream wf(filePath);
        if(wf.fail())
        {
            printf("error, open file: %s failed\n", filePath);
            return false;
        }

        wf.write((char*)data, size);
        wf.close();
        return true;
    }

   //------------------------------------------------------------------------------------//
    bool getAllFiles(const char* dirPath, std::vector<std::string>& files, char* type)
    {
        if (!dirPath)
            return false;

        size_t typeSize = type ? ::strlen(type) : 0;

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        intptr_t hFile = 0;
        struct _finddata_t fileinfo;
        std::string p;

        if ((hFile = _findfirst(p.assign(dirPath).append((std::string("\\*") + type)).c_str(), &fileinfo)) != -1)
        {
            do
            {
                files.push_back(fileinfo.name);
            } while (_findnext(hFile, &fileinfo) == 0);

            _findclose(hFile);

            return true;
        }

        return false;
#else
        DIR* dp = opendir(dirPath);
        if(dp == nullptr)
        {
            printf("error, %s doesn't exist\n", dirPath);
            return false;
        }

        struct dirent* ptr;
        char absolutePath[512] = { 0 };
        while ((ptr = readdir(dp)) != nullptr)
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

                        files.push_back(absolutePath);
                    }
                }
            }
            else if (ptr->d_type == 4)
            { //dir
                memset(absolutePath, '\0', sizeof(absolutePath));
                strcpy(absolutePath, dirPath);
                strcat(absolutePath, "/");
                strcat(absolutePath, ptr->d_name);
                getAllFiles(absolutePath, files, type);
            }
        }
        
        closedir(dp);
        return true;
#endif        
    }

   //------------------------------------------------------------------------------------//
    void deleteFile(const char* filePath)
    {
        if(filePath)
            remove(filePath);
    }

    __CExternEnd__
__END__