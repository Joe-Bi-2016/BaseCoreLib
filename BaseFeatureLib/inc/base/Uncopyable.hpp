#ifndef __Uncopyable_h__
#define __Uncopyable_h__
#include "Macro.h"

//-------------------------------------------------------------------------------------//
__BEGIN__

    class API_EXPORTS Uncopyable
    {
        protected:
            Uncopyable(void) { }
            virtual ~Uncopyable(void) { }

        private:
            Uncopyable(const Uncopyable& other) { }
            Uncopyable& operator=(const Uncopyable& other) { return *this; } 
    };

__END__

#endif // __Uncopyable_h__
