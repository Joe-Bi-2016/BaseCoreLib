#ifndef __SemSyncLock_h__
#define __SemSyncLock_h__
#include "Semaphore.hpp"
#include <assert.h>

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
    class API_EXPORTS SemSyncLock : private Uncopyable
    {
        public:
            explicit SemSyncLock(Semaphore* const sem)
            : mSem(sem)
            {  
                if(mSem)
                {
                    mSem->wait(-1);
                }
            }

            ~SemSyncLock()
            {
                if(mSem)
                    mSem->sendMessage();
            }

        private:
            Semaphore*  const mSem;
    };

__END__

#endif // __SemSyncLock_h__
