/*****************************************************************************
* FileName    : AutoMutex.hpp
* Description : Auto mutex lock definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __AutoMutex_h__
#define __AutoMutex_h__
#include "Mutex.hpp"

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
    class API_EXPORTS AutoMutex : private Uncopyable
    {
        public:
            explicit AutoMutex(Mutex* const pMutex)
            : mpMutex(pMutex)
            {
                if (mpMutex)
                    mpMutex->lock();
            }

            ~AutoMutex()
            {
                if (mpMutex)
                    mpMutex->unlock();
            }

        protected:
            Mutex* const    mpMutex;
    };

__END__

#endif // __AutoMutex_h__
