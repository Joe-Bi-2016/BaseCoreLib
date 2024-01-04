/*****************************************************************************
* FileName    : CAssetFile.cpp
* Description : Interface to access assetFile by C-file implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/os/CAssetFile.h"
#include "../../inc/os/AssetFile.h"

//---------------------------------------------------------------------------//
__BEGIN__
    __CExternBegin__

        #if defined(__ANDROID__)

            //---------------------------------------------------------------//
            AssetFile* mFile = nullptr;
            
            //---------------------------------------------------------------//
            bool hadSetAssetManager(void)
            {
                bool ret = ((AssetFile::getAssetManager() != nullptr) ? true : false);
                return ret;
            }
            
            //---------------------------------------------------------------//
            void setAssetManager(void* assetManager)
            {
                AssetFile::setAssetManager(assetManager);
            }
            
            //---------------------------------------------------------------//
            bool fileOpen(const char* path, const char* mode)
            {
                if(mFile == nullptr)
                    mFile = new AssetFile();
                else
                    mFile->fclose();    
            
                return mFile->fopen(path, mode);    
            }
            
            //---------------------------------------------------------------//
            void fileClose(void)
            {
                if(mFile)
                {
                    mFile->fclose();
                    delete mFile;
                    mFile = nullptr;
                }
            }
            
            //---------------------------------------------------------------//
            uint64 fileFread(void *buffer, uint32 size, uint32 count)
            {
                if(mFile)
                    return mFile->fread(buffer, size, count);
            
                return 0;
            }
            
            //---------------------------------------------------------------//
            char* fileGets(char *buffer, int bufferSize)
            {
                if(mFile)
                    return mFile->fgets(buffer, bufferSize);
            
                return nullptr;
            }
            
            //---------------------------------------------------------------//
            bool getFileOneLine(char** buf, int* len)
            {
                if(mFile)
                    return mFile->getOneLine(buf, len);
            
                return false;
            }
            
            //---------------------------------------------------------------//
            uint64 size(void)
            {
                if(mFile)
                    return mFile->size();
            
                return 0;
            }

        #endif

    __CExternEnd__
__END__