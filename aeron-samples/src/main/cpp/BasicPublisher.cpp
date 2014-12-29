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

#include <util/CommandOptionParser.h>
#include <thread>
#include "Configuration.h"
#include <Aeron.h>
#include <array>
#include <atomic>
#include <csignal>
#include <memory>
#include <cstdio>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

using namespace aeron::common::util;
using namespace aeron;

std::atomic<bool> running (true);

void sigIntHandler (int param)
{
    running = false;
}

static const char optHelp     = 'h';
static const char optChannel  = 'c';
static const char optStreamId = 's';
static const char optMessages = 'm';
static const char optLinger   = 'l';

struct Settings
{
    std::string channel = samples::configuration::DEFAULT_CHANNEL;
    std::int32_t streamId = samples::configuration::DEFAULT_STREAM_ID;
    int numberOfMessages = samples::configuration::DEFAULT_NUMBER_OF_MESSAGES;
    int lingerTimeoutMs = samples::configuration::DEFAULT_LINGER_TIMEOUT_MS;
};

typedef std::array<std::uint8_t, 256> buffer_t;

Settings parseCmdLine(CommandOptionParser& cp, int argc, char** argv)
{
    cp.parse(argc, argv);
    if (cp.getOption(optHelp).isPresent())
    {
        cp.displayOptionsHelp(std::cout);
        exit(0);
    }

    Settings s;

    s.channel = cp.getOption(optChannel).getParam(0, s.channel);
    s.streamId = cp.getOption(optStreamId).getParamAsInt(0, 1, INT32_MAX, s.streamId);
    s.numberOfMessages = cp.getOption(optMessages).getParamAsInt(0, 0, INT32_MAX, s.numberOfMessages);
    s.lingerTimeoutMs = cp.getOption(optLinger).getParamAsInt(0, 0, 60 * 60 * 1000, s.lingerTimeoutMs);

    return s;
}

int main (int argc, char** argv)
{
    CommandOptionParser cp;
    cp.addOption(CommandOption (optHelp,     0, 0, "                Displays help information."));
    cp.addOption(CommandOption (optChannel,  1, 1, "channel         Channel."));
    cp.addOption(CommandOption (optStreamId, 1, 1, "streamId        Stream ID."));
    cp.addOption(CommandOption (optMessages, 1, 1, "number          Number of Messages."));
    cp.addOption(CommandOption (optLinger,   1, 1, "milliseconds    Linger timeout in milliseconds."));

    signal (SIGINT, sigIntHandler);

    try
    {
        Settings settings = parseCmdLine(cp, argc, argv);

        std::cout << "Publishing to channel " << settings.channel << " on Stream ID " << settings.streamId << std::endl;

        aeron::Context context;
        Aeron aeron(context.useSharedMemoryOnLinux());
        // shared so that it will be deleted when going out of scope
        std::shared_ptr<Publication> publication(aeron.addPublication(settings.channel, settings.streamId));

        MINT_DECL_ALIGNED(buffer_t buffer, 16);
        concurrent::AtomicBuffer srcBuffer(&buffer[0], buffer.size());
        char message[256];

        for (int i = 0; i < settings.numberOfMessages && running; i++)
        {
            const int messageLen = snprintf(message, sizeof(message), "Hello World! %d", i);

            srcBuffer.putBytes(0, reinterpret_cast<std::uint8_t *>(message), messageLen);

            std::cout << "offering " << i << "/" << settings.numberOfMessages;
            std::cout.flush();

            const bool result = publication->offer(srcBuffer, 0, messageLen);

            if (!result)
            {
                std::cout << " ah?!" << std::endl;
            }
            else
            {
                std::cout << " yay!" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Done sending." << std::endl;

        if (settings.lingerTimeoutMs > 0)
        {
            std::cout << "Lingering for " << settings.lingerTimeoutMs << " milliseconds." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(settings.lingerTimeoutMs));
        }
    }
    catch (CommandOptionException& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        cp.displayOptionsHelp(std::cerr);
        return -1;
    }
    catch (SourcedException& e)
    {
        std::cerr << "FAILED: " << e.what() << " : " << e.where() << std::endl;
        return -1;
    }
    catch (std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << " : " << std::endl;
        return -1;
    }

    return 0;
}
