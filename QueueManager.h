#pragma once
#define _QUEUEMANAGER_H_

#include <bitset>
#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <utility>


class IConsumer
{
public:
    virtual ~IConsumer() = default;

    virtual void consume() noexcept = 0;
};

template<typename TKey, typename TValue>
class QueueManager
{
public:

    QueueManager() = default;

    QueueManager(QueueManager const &) = delete;

    QueueManager(QueueManager &&) = delete;

    ~QueueManager() = default;

    QueueManager & operator = (QueueManager const &) = delete;

    QueueManager & operator = (QueueManager &&) = delete;

    void enqueue(TKey const & key, TValue const & value)
    {
        {
            std::lock_guard input_queue_lock(mx_input_queue);
            lt_pairs_input.emplace_back(key, value);
        }
        raise(signal::review_input_queue);
    }

    void enqueue(TKey const & key, TValue && value)
    {
        {
            std::lock_guard input_queue_lock(mx_input_queue);
            lt_pairs_input.emplace_back(TKey(key), std::move(value));
        }
        raise(signal::review_input_queue);
    }

    void enqueue(TKey&& key, TValue&& value)
    {
        {
            std::lock_guard input_queue_lock(mx_input_queue);
            lt_pairs_input.emplace_back(std::move(key), std::move(value));
        }
        raise(signal::review_input_queue);
    }

    void start()
    {
        if(!qm_thread.joinable())
            qm_thread = std::thread(&QueueManager::processor_thread, this);
    }

    void stop() noexcept
    {
        raise(signal::stop);
        if(qm_thread.joinable())
            qm_thread.join();
    }

    void subscribe(TKey const & key, IConsumer* consumer)
    {
        {
            std::lock_guard subscriptions_lock(mx_subscriptions);
            um_subscriptions.insert_or_assign(key, consumer);
        }
        raise(signal::review_pending_queue);
    }

    void unsubscribe(TKey const & key) noexcept
    {
        std::lock_guard subscriptions_lock(mx_subscriptions);
        um_subscriptions.erase(key);
    }

private:

    struct signal
    {
        enum : std::size_t
        {
            stop                 = 0,
            review_pending_queue = 1,
            review_input_queue   = 2,
        };
    };

    void processor_thread() noexcept
    {
        std::unique_lock<std::mutex> processor_thread_lock(mx_thread);
        sl_thread.reset(signal::stop);
        while (true)
        {
            if(sl_thread.test(signal::stop))
                return;

            if(sl_thread.test(signal::review_pending_queue))
            {
                sl_thread.reset(signal::review_pending_queue);
                processor_thread_lock.unlock();

                for(auto key_value_itr = lt_pairs_pending.begin(), key_value_end_itr = lt_pairs_pending.end(); key_value_itr != key_value_end_itr;)
                    consume(lt_pairs_pending, key_value_itr);

                processor_thread_lock.lock();
                continue;
            }

            if(sl_thread.test(signal::review_input_queue))
            {
                sl_thread.reset(signal::review_input_queue);
                processor_thread_lock.unlock();

                auto input_queue = capture_input_queue();

                for(auto key_value_itr = input_queue.begin(), key_value_end_itr = input_queue.end(); key_value_itr != key_value_end_itr;)
                    consume(input_queue, key_value_itr);

                if(!input_queue.empty())
                    lt_pairs_pending.splice(lt_pairs_pending.end(), input_queue);

                processor_thread_lock.lock();
                continue;
            }

            cv_thread.wait(processor_thread_lock);
        }
    }

    void raise(std::size_t signal)
    {
        std::lock_guard processor_thread_lock(mx_thread);
        sl_thread.set(signal);
        cv_thread.notify_all();
    }

    std::list<std::pair<TKey, TValue> >  capture_input_queue()
    {
        std::lock_guard input_queue_lock(mx_input_queue);
        return std::move(lt_pairs_input); // move for list move cntr
    }

    void consume(std::list<std::pair<TKey, TValue> > & queue, typename std::list<std::pair<TKey, TValue> >::iterator& key_value_itr)
    {
        IConsumer* consumer = get_consumer(key_value_itr->first);

        if(consumer)
        {
            consumer->consume();
            key_value_itr = queue.erase(key_value_itr);
            return;
        }

        ++key_value_itr;//increase outside cycle counter by list iterators
    }

    IConsumer* get_consumer(TKey const & key)
    {
        std::lock_guard subscriptions_lock(mx_subscriptions);
        auto subscription_itr = um_subscriptions.find(key);
        return subscription_itr != um_subscriptions.end() ? subscription_itr->second : nullptr;
    }

    std::unordered_map<TKey, IConsumer*>   um_subscriptions;

    std::list<std::pair<TKey, TValue> >    lt_pairs_input;
    std::list<std::pair<TKey, TValue> >    lt_pairs_pending;

    std::mutex                             mx_thread;
    std::mutex                             mx_input_queue;
    std::mutex                             mx_subscriptions;

    std::condition_variable                cv_thread;
    std::bitset<3>                         sl_thread;
    std::thread                            qm_thread;
};



