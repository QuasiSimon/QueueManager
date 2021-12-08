#pragma once
#define _CONSUMER_H_

#include "QueueManager.h"

#include <atomic>

template<typename TKey, typename TValue>
class CustomConsumer : public IConsumer
{
public:

    CustomConsumer() = default;

    CustomConsumer(CustomConsumer const &) = delete;

    CustomConsumer(CustomConsumer &&) = delete;

    ~CustomConsumer() = default;

    CustomConsumer& operator = (CustomConsumer const &) = delete;

    CustomConsumer& operator = (CustomConsumer&&) = delete;

    virtual void consume() noexcept override
    {
        try
        {
            at_received.fetch_add(1, std::memory_order_relaxed);
        }
        catch(...)
        {

        }
    }

    std::atomic<uint32_t>   at_expected {0};
    std::atomic<uint32_t>   at_received {0};
};



