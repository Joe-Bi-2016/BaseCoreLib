#ifndef __Macro_h__
#define __Macro_h__
#include <stdio.h>

//--------------------------------------------------------------------------------------//
#ifdef __cplusplus
    #define MyNameSpace_Begin(name) namespace name{
    #define MyNameSpace_end }
    #define Using_MyNameSpace(name) using namespace name;
    #define MyNameSpace_Multi_Begin(base, name) MyNameSpace_Begin(base) namespace name{
    #define MyNameSpace_Multi_End }}
    #define __CExternBegin__ extern "C" {
    #define __CExternEnd__ }
#else
    #define MyNameSpace_Begin(name)
    #define MyNameSpace_end
    #define Using_MyNameSpace(name)
    #define MyNameSpace_Multi_Begin(base, name)
    #define MyNameSpace_Multi_End
    #define __CExternBegin__
    #define __CExternEnd__
#endif

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    #ifdef API_CONDUCT
        #ifdef __GNUC__
            #define API_EXPORTS __attribute__ ((dllexport))
        #else
            #define API_EXPORTS __declspec(dllexport)
        #endif
    #else
        #ifdef __GNUC__
            #define API_EXPORTS __attribute__ ((dllimport))
        #else
            #if defined(NDEBUG)
                #define API_EXPORTS
            #elif defined(RELEASE)
                #define API_EXPORTS __declspec(dllexport)
            #else
                #define API_EXPORTS __declspec(dllimport)
            #endif
        #endif
    #endif
#else
    #if __GNUC__ >= 4
        #define API_EXPORTS __attribute__ ((visibility ("default")))
        #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
    #else
        #define API_EXPORTS
        #define DLL_LOCAL
    #endif
#endif

//define Usage patterns
#define NameSpace_RootName         Root  /*main name space*/
#define NameSpace_ModuleName    Core

#define __BEGIN__      MyNameSpace_Multi_Begin(NameSpace_RootName, NameSpace_ModuleName)
#define __END__         MyNameSpace_Multi_End

// define string
#define STR(x)  #x

// define memory macro
#ifdef __cplusplus
    #define newObject(ptr, Class, ...)  ptr = new Class(__VA_ARGS__)
    #define newArray(T, n)  ((T*)(new T[n]))
    #define deleteArray(T, ptr)\
     if(ptr)\
     {\
         delete [] (T*)ptr;\
         ptr = NULL;\
     }
#else
    #define newArray(T, n)  ((T*)(malloc(sizeof(T) * n)))
    #define deleteArray(T, ptr)\
     if(ptr)\
     {\
         free(ptr);\
         ptr = NULL;\
     }
#endif

// define type
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    typedef __int64                     int64;
    typedef unsigned __int64     uint64;
    typedef unsigned int            uint32;
#else
    typedef int64_t                     int64;
    typedef uint64_t                   uint64;
    typedef uint32_t                   uint32;
#endif

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    #define SNPRINTF         _snprintf_s
#else
    #define SNPRINTF         snprintf
#endif

__BEGIN__

template<typename T>
struct deleter
{
    void operator()(T* p) {
//        printf("T type is : %s\n", typeid(T).name());
        if (p) { delete p;  p = nullptr; }
    }
};

__END__

#endif // __Macro_h__

