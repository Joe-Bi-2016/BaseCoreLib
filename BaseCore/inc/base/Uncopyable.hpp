/*****************************************************************************
* FileName      : Uncopyable.hpp
* Description   : The base class of uncopy. Avoid compiler to generate
* constructor  destructor and assignement and copy constructor.
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
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
