#include "Consumer.h"
#include "QueueManager.h"

#include <sstream>
#include <iostream>
#include <random>

using CustomConsumerWrapper = CustomConsumer<uint32_t, std::string>;
using QueueManagerWrapper = QueueManager<uint32_t, std::string>;

constexpr uint32_t N_PRODUCERS = 10;
constexpr uint32_t N_CONSUMERS = 5;
constexpr uint32_t VOLUME_MULTIPL = 20;

uint32_t InProgress = 0;
uint32_t MaxProcces = 0;

void producer_thread(uint32_t id, uint32_t key, uint32_t volume, QueueManagerWrapper& processor, std::mutex& ref_mx) noexcept
{
    try
    {
        {
            std::lock_guard<std::mutex> progress_lock(ref_mx);
            std::cout << "Start for {KEY,Producer,Volume} {" << key << "," << id << "," << volume << "}" << std::endl;
            MaxProcces += volume;
        }

        std::default_random_engine  random_engine(static_cast<uint32_t>(std::time(nullptr)));

        std::uniform_int_distribution<uint32_t> time_for_sleep(0, 20);
        for(uint32_t value = 0; value < volume; ++value)
        {
            std::ostringstream value_oss;
            value_oss << "pid: " << id << ", key: " << key << ", value: " << value;

            processor.enqueue(key, value_oss.str());

            {
                std::lock_guard<std::mutex> local_lock(ref_mx);
                std::cout << "Thread No " << (++InProgress) << " of " << MaxProcces << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(time_for_sleep(random_engine)));
        }
    }
    catch(...)
    {

    }
}

int main()
{
    std::default_random_engine  random_engine(static_cast<uint32_t>(std::time(nullptr)));
    std::mutex mx_progress;

    QueueManagerWrapper processor;
    processor.start();

    //subscribe
    std::array<CustomConsumerWrapper, N_CONSUMERS> consumers;
    for(uint32_t consumer_key = 0; consumer_key < N_CONSUMERS; ++consumer_key)
        processor.subscribe(consumer_key, &consumers[consumer_key]);

    //run threads
    std::array<std::thread, N_PRODUCERS> producers;
    std::uniform_int_distribution<uint32_t>  producer_volume_random_distribution(1, 5);
    for(uint32_t producer = 0; producer < N_PRODUCERS; ++producer)
    {
        uint32_t producer_key    = producer % N_CONSUMERS;
        uint32_t producer_volume = VOLUME_MULTIPL * producer_volume_random_distribution(random_engine);
        if(producer_key < consumers.size())
            consumers[producer_key].at_expected.fetch_add(producer_volume, std::memory_order_relaxed);
        producers[producer] = std::thread(&producer_thread, producer, producer_key, producer_volume, std::ref(processor), std::ref(mx_progress));
    }

    //wait threads
    for(auto & producer : producers)
        if(producer.joinable())
            producer.join();

    //unsubscribe
    for(uint32_t consumer_key = 0; consumer_key < N_CONSUMERS; ++consumer_key)
        processor.unsubscribe(consumer_key);

    processor.stop();

    std::cout << std::endl << std::endl;

    for(uint32_t consumer_key = 0; consumer_key < N_CONSUMERS; ++consumer_key)
    {
        auto & consumer = consumers[consumer_key];
        std::cout << "Consumer No " << consumer_key << " received " << consumer.at_received << " of " << consumer.at_expected << std::endl;
    }

    std::cout << std::endl;
}



