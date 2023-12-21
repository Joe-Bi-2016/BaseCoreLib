#ifndef __File_h__
#define __File_h__
#include "../base/Macro.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <string>
#include <memory>

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
        class API_EXPORTS File
        {
        public:
            explicit File(void);
            explicit File(const FILE* file);

            explicit File(const File& file);
            explicit File(File && file);
            explicit File(const std::string& path, const std::string& mode = "rw");

            File& operator=(File&& file);
            File& operator=(const File& file);

            virtual ~File(void);

        #if defined(__ANDROID__)
            void setAssetManager(const void* assetManager);
        #endif
        
            virtual bool fopen(const std::string& path, const std::string& mode = "rw");
            virtual void fclose(void);

            virtual size_t fread(void* buffer, long size, long count);
            virtual size_t fwrite(const void* buffer, long size, long count);

            virtual char* fgets(char* buffer, int bufferSize);

            virtual void fprintf(const char* format, ...);

            virtual void syncfile(void);

            virtual long size(void);

            virtual bool eof(void);
            virtual void resetRead(void);
            virtual void resetWrite(void);
            virtual std::string& getPath(void);

            bool getOneLine(char** buf, size_t* len);

        private:
            void release(void);

        protected:
            std::string    mPath;
            std::string    mMode;
            void*           mFile;
            bool            mNeedClosed;
            long            mReadPos;
            long            mWritePos;
            void*           mAssetManager;
    };

    class API_EXPORTS FileUtil
    {
        public:
            static bool isDirectory(const char* dir);

            static bool isFileExist(const char* filePath);
            
            static int64 getFileSize(const char* filePath);
            
            static uint64 getFileSize(FILE* file);
            
            static uint64 readFile(const char* filePath, std::string& buffer);
            
            static uint64 readFile(File* file, std::string& buffer);
            
            static void *readFile(const char* filePath, uint64* size);
            
            static bool saveFile(const char* filePath, void* data, uint64 size);
            
            static bool getAllFiles(const char* dir, std::vector<std::string>& files, char* type = nullptr);
            
            static void delFile(const char* filePath);
    };

__END__

#endif // __File_h__
