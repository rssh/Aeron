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

#ifndef INCLUDED_AERON_CONCURRENT_LOGBUFFER_LOG_BUFFER__
#define INCLUDED_AERON_CONCURRENT_LOGBUFFER_LOG_BUFFER__

#include <util/Index.h>
#include <concurrent/AtomicBuffer.h>
#include <algorithm>
#include "LogBufferDescriptor.h"


namespace aeron { namespace common { namespace concurrent { namespace logbuffer {

class LogBuffer
{
public:

    inline AtomicBuffer& logBuffer() const
    {
        return m_logBuffer;
    }

    inline AtomicBuffer& stateBuffer() const
    {
        return m_stateBuffer;
    }

    inline util::index_t capacity() const
    {
        return m_capacity;
    }

    inline void clean()
    {
        m_logBuffer.setMemory(0, m_logBuffer.getCapacity(), 0);
        m_stateBuffer.setMemory(0, m_stateBuffer.getCapacity(), 0);
        statusOrdered(LogBufferDescriptor::CLEAN);
    }

    inline int status() const
    {
        return m_stateBuffer.getInt32Ordered(LogBufferDescriptor::STATUS_OFFSET);
    }

    inline bool compareAndSetStatus(std::int32_t expectedStatus, std::int32_t updateStatus)
    {
        return m_stateBuffer.compareAndSetInt32(LogBufferDescriptor::STATUS_OFFSET, expectedStatus, updateStatus);
    }

    inline void statusOrdered(std::int32_t status)
    {
        m_stateBuffer.putInt32Ordered(LogBufferDescriptor::STATUS_OFFSET, status);
    }

    inline std::int32_t tailVolatile()
    {
        return std::min(m_stateBuffer.getInt32Ordered(LogBufferDescriptor::TAIL_COUNTER_OFFSET), m_capacity);
    }

    inline std::int32_t highWaterMarkVolatile()
    {
        return m_stateBuffer.getInt32Ordered(LogBufferDescriptor::HIGH_WATER_MARK_OFFSET);
    }

    inline std::int32_t tail()
    {
        return std::min(m_stateBuffer.getInt32(LogBufferDescriptor::TAIL_COUNTER_OFFSET), m_capacity);
    }

    inline std::int32_t highWaterMark()
    {
        return m_stateBuffer.getInt32(LogBufferDescriptor::HIGH_WATER_MARK_OFFSET);
    }

protected:
    inline LogBuffer(AtomicBuffer& logBuffer, AtomicBuffer& stateBuffer)
    : m_logBuffer(logBuffer), m_stateBuffer(stateBuffer)
    {
        LogBufferDescriptor::checkLogBuffer(logBuffer);
        LogBufferDescriptor::checkStateBuffer(stateBuffer);

        m_capacity = logBuffer.getCapacity();
    }

private:
    AtomicBuffer& m_logBuffer;
    AtomicBuffer& m_stateBuffer;
    util::index_t m_capacity;
};

}}}}

#endif
