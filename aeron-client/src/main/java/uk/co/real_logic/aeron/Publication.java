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
package uk.co.real_logic.aeron;

import uk.co.real_logic.aeron.common.concurrent.logbuffer.BufferClaim;
import uk.co.real_logic.agrona.DirectBuffer;
import uk.co.real_logic.aeron.common.TermHelper;
import uk.co.real_logic.aeron.common.concurrent.logbuffer.LogAppender;
import uk.co.real_logic.aeron.common.concurrent.logbuffer.LogBufferDescriptor;
import uk.co.real_logic.aeron.common.protocol.DataHeaderFlyweight;
import uk.co.real_logic.agrona.status.PositionIndicator;

import static java.nio.ByteOrder.LITTLE_ENDIAN;
import static uk.co.real_logic.aeron.common.TermHelper.*;

/**
 * Publication end of a channel and stream for publishing messages to subscribers.
 * <p>
 * Note: Publication instances are threadsafe and can be shared between publisher threads.
 */
public class Publication implements AutoCloseable
{
    private final ClientConductor clientConductor;
    private final ManagedBuffer[] managedBuffers;
    private final String channel;
    private final int streamId;
    private final int sessionId;
    private final long registrationId;
    private final LogAppender[] logAppenders;
    private final PositionIndicator limit;
    private final int positionBitsToShift;
    private final int initialTermId;

    private volatile int activeTermId;
    private int refCount = 1;

    Publication(
        final ClientConductor clientConductor,
        final String channel,
        final int streamId,
        final int sessionId,
        final int initialTermId,
        final LogAppender[] logAppenders,
        final PositionIndicator limit,
        final ManagedBuffer[] managedBuffers,
        final long registrationId)
    {
        this.clientConductor = clientConductor;
        this.channel = channel;
        this.streamId = streamId;
        this.sessionId = sessionId;
        this.managedBuffers = managedBuffers;
        this.registrationId = registrationId;
        this.activeTermId = initialTermId;
        this.logAppenders = logAppenders;
        this.limit = limit;

        this.positionBitsToShift = Integer.numberOfTrailingZeros(logAppenders[0].capacity());
        this.initialTermId = initialTermId;
    }

    /**
     * Media address for delivery to the channel.
     *
     * @return Media address for delivery to the channel.
     */
    public String channel()
    {
        return channel;
    }

    /**
     * Stream identity for scoping within the channel media address.
     *
     * @return Stream identity for scoping within the channel media address.
     */
    public int streamId()
    {
        return streamId;
    }

    /**
     * Session under which messages are published.
     *
     * @return the session id for this publication.
     */
    public int sessionId()
    {
        return sessionId;
    }

    public void close()
    {
        synchronized (clientConductor)
        {
            if (--refCount == 0)
            {
                clientConductor.releasePublication(this);

                for (final ManagedBuffer managedBuffer : managedBuffers)
                {
                    managedBuffer.close();
                }
            }
        }
    }

    /**
     * Non-blocking publish of a buffer containing a message.
     *
     * @param buffer containing message.
     * @return true if buffer is sent otherwise false.
     */
    public boolean offer(final DirectBuffer buffer)
    {
        return offer(buffer, 0, buffer.capacity());
    }

    /**
     * Non-blocking publish of a partial buffer containing a message.
     *
     * @param buffer containing message.
     * @param offset offset in the buffer at which the encoded message begins.
     * @param length in bytes of the encoded message.
     * @return true if the message was published otherwise false.
     */
    public boolean offer(final DirectBuffer buffer, final int offset, final int length)
    {
        boolean succeeded = false;
        final int initialTermId = this.initialTermId;
        final int activeTermId = this.activeTermId;
        final int activeIndex = bufferIndex(initialTermId, activeTermId);
        final LogAppender logAppender = logAppenders[activeIndex];
        final int currentTail = logAppender.tailVolatile();

        if (isWithinFlowControlLimit(initialTermId, activeTermId, currentTail))
        {
            switch (logAppender.append(buffer, offset, length))
            {
                case SUCCESS:
                    succeeded = true;
                    break;

                case TRIPPED:
                    nextTerm(activeTermId, activeIndex);
                    break;

                case FAILURE:
                    break;
            }
        }

        return succeeded;
    }

    /**
     * Try to claim a range in the publication log into which a message can be written with zero copy semantics.
     * Once the message has been written then {@link BufferClaim#commit()} should be called thus making it available.
     *
     * <b>Note:</b> This method can only be used for message lengths less than MTU size minus header.
     *
     * <code>
     *     final BufferClaim bufferClaim = new BufferClaim(); // Can be stored and reused to avoid allocation
     *
     *     if (publication.tryClaim(messageLength, bufferClaim))
     *     {
     *         try
     *         {
     *              final MutableDirectBuffer buffer = bufferClaim.buffer();
     *              final int offset = bufferClaim.offset();
     *
     *              // Work with buffer directly or wrap with a flyweight
     *         }
     *         finally
     *         {
     *             bufferClaim.commit();
     *         }
     *     }
     * </code>
     *
     * @param length      of the range to claim.
     * @param bufferClaim to be populate if the claim succeeds.
     * @return true if the claim was successful otherwise false.
     * @throws IllegalArgumentException if the length is greater than max payload size within an MTU.
     * @see uk.co.real_logic.aeron.common.concurrent.logbuffer.BufferClaim#commit()
     */
    public boolean tryClaim(final int length, final BufferClaim bufferClaim)
    {
        boolean succeeded = false;
        final int initialTermId = this.initialTermId;
        final int activeTermId = this.activeTermId;
        final int activeIndex = bufferIndex(initialTermId, activeTermId);
        final LogAppender logAppender = logAppenders[activeIndex];
        final int currentTail = logAppender.tailVolatile();

        if (isWithinFlowControlLimit(initialTermId, activeTermId, currentTail))
        {
            switch (logAppender.claim(length, bufferClaim))
            {
                case SUCCESS:
                    succeeded = true;
                    break;

                case TRIPPED:
                    nextTerm(activeTermId, activeIndex);
                    break;

                case FAILURE:
                    break;
            }
        }

        return succeeded;
    }

    long registrationId()
    {
        return registrationId;
    }

    void incRef()
    {
        synchronized (clientConductor)
        {
            ++refCount;
        }
    }

    private void nextTerm(final int activeTermId, final int activeIndex)
    {
        final int newTermId = activeTermId + 1;
        final int nextIndex = rotateNext(activeIndex);

        logAppenders[nextIndex].defaultHeader().putInt(DataHeaderFlyweight.TERM_ID_FIELD_OFFSET, newTermId, LITTLE_ENDIAN);
        logAppenders[rotatePrevious(activeIndex)].statusOrdered(LogBufferDescriptor.NEEDS_CLEANING);

        this.activeTermId = newTermId;
    }

    private boolean isWithinFlowControlLimit(final int initialTermId, final int activeTermId, final int currentTail)
    {
        return TermHelper.calculatePosition(activeTermId, currentTail, positionBitsToShift, initialTermId) < limit.position();
    }
}
