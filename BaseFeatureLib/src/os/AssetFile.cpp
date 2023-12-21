#include "../../inc/os/AssetFile.h"
#include <stdio.h>
#include <errno.h>

//---------------------------------------------------------------------------------------//
__BEGIN__

    #ifdef __ANDROID__
        #include <android/asset_manager.h>

        //---------------------------------------------------------------------------------//
        static AAssetManager *g_android_asset_manager = nullptr;
        
        //---------------------------------------------------------------------------------//
        static int android_read(void *cookie, char *buf, int size) {
            return AAsset_read((AAsset *) cookie, buf, size);
        }

        //---------------------------------------------------------------------------------//
        static int android_write(void *cookie, const char *buf, int size) {
            return EACCES; // can't provide write access to the apk
        }

        //---------------------------------------------------------------------------------//
        static fpos_t android_seek(void *cookie, fpos_t offset, int whence) {
            return AAsset_seek((AAsset *) cookie, offset, whence);
        }

        //---------------------------------------------------------------------------------//
        static int android_close(void *cookie) {
            AAsset_close((AAsset *) cookie);
            return 0;
        }

        //---------------------------------------------------------------------------------//
        static FILE *android_fopen(const char *fname, char *mode, uint64* size) {
            if (mode[0] == 'w') 
                return nullptr;

            AAsset *asset = AAssetManager_open(g_android_asset_manager, fname, 0);
            if (!asset) 
                return nullptr;

            if(size)
                *size = AAsset_getLength(asset);

            return funopen(asset, android_read, android_write, android_seek, android_close);
        }

        //---------------------------------------------------------------------------------//
        AssetFile::AssetFile()
                : mSize(0){
        }

        //---------------------------------------------------------------------------------//
        AssetFile::AssetFile(const char *path, const char *mode)
                : mSize(0){
            fopen(path, mode);
        }

        //---------------------------------------------------------------------------------//
        AssetFile::~AssetFile() {
            fclose();
        }

        //---------------------------------------------------------------------------------//
        bool AssetFile::fopen(const std::string &path, const std::string &mode) {
            if (!mFile) {
                mFile = (void*)android_fopen(path.c_str(), (char *) mode.c_str(), &mSize);
            }
            mPath = path;
            return mFile != nullptr;
        }

        //---------------------------------------------------------------------------------//
        void AssetFile::fclose() {
            if (mFile) {
                ::fclose((FILE*)mFile);
                mFile = nullptr;
            }
        }

        //---------------------------------------------------------------------------------//
        uint64 AssetFile::fread(void *buffer, uint32 size, uint32 count) {
            uint64 r = 0;

            if (mFile) {
                ::fseek((FILE*)mFile, mReadPos, SEEK_SET);
                r = ::fread(buffer, size, count, (FILE*)mFile);
                mReadPos = ::ftell((FILE*)mFile);
            }

            return r;
        }

        //---------------------------------------------------------------------------------//
        uint64 AssetFile::fwrite(const void *pVoid, uint32 size, uint32 count) {
            return 0;
        }

        //---------------------------------------------------------------------------------//
        char *AssetFile::fgets(char *buffer, int bufferSize) {
            char *r = nullptr;

            if (mFile) {
                ::fseek((FILE*)mFile, mReadPos, SEEK_SET);
                r = ::fgets(buffer, bufferSize, (FILE*)mFile);
                mReadPos = ::ftell((FILE*)mFile);
            }
            return r;
        }

        //---------------------------------------------------------------------------------//
        void AssetFile::fprintf(const char *format, ...) {
        }

        //---------------------------------------------------------------------------------//
        uint64 AssetFile::size() {
            return mSize;
        }

        //---------------------------------------------------------------------------------//
        bool AssetFile::eof() {
            if (mFile) {
                return ::feof((FILE*)mFile);
            }
            return true;
        }

        //---------------------------------------------------------------------------------//
        void AssetFile::resetRead() {
        }

        //---------------------------------------------------------------------------------//
        void AssetFile::resetWrite() {
        }

        //---------------------------------------------------------------------------------//
        std::string &AssetFile::getPath() {
            return mPath;
        }

        //---------------------------------------------------------------------------------//
        void AssetFile::setAssetManager(void *manager) {
            g_android_asset_manager = (AAssetManager *) manager;
        }

        //---------------------------------------------------------------------------------//
        void* AssetFile::getAssetManager(void) {
            return g_android_asset_manager;
        }

    #endif

__END__
