/*
 * Copyright 2014 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDED_AERON_COMMON_BACKOFF_IDLE_STRATEGY__
#define INCLUDED_AERON_COMMON_BACKOFF_IDLE_STRATEGY__

#include <util/Exceptions.h>
#include <iostream>
#include <thread>
#include <mintomic/mintomic.h>


namespace aeron { namespace common { namespace common {

class BusySpinIdleStrategy
{
public:
    BusySpinIdleStrategy()
    {
    }

    inline void idle(int workCount)
    {
        if (workCount > 0)
        {
            return;
        }

        pause();
    }

    inline static void pause()
    {
#if MINT_CPU_X86 || MINT_CPU_X64
# ifdef _MSC_VER
		__asm  { pause };
# else
        asm volatile("pause");
# endif
#else
        #error Unsupported platform!
#endif
    }

private:
};

}}}

#endif