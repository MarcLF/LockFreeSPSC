#define BOOST_TEST_MODULE QueueTests
#include "../include/LockFreeSPSC.h"
#include <boost/test/included/unit_test.hpp>
#include <thread>

// Stress test with high concurrency
BOOST_AUTO_TEST_CASE(HeavyConcurrencyTest)
{
    // A small queue capacity forces the threads to contend
    // (wait for each other) frequently.
    LockFreeSPSC<int, 16> queue;

    constexpr int item_count = 500000;

    // Variables to track results
    long long producer_sum = 0;
    long long consumer_sum = 0;

    // Producer thread
    std::thread producer([&] {
        for (int i = 0; i < item_count; ++i) {
            // Spin until we succeed in pushing
            while (!queue.push(i)) {
                // If full, yield CPU to let consumer run
                std::this_thread::yield();
            }
            producer_sum += i;
        }
    });

    // Consumer thread
    std::thread consumer([&] {
        int items_received = 0;
        while (items_received < item_count) {
            if (auto val = queue.pop()) {
                consumer_sum += *val;
                items_received++;
            } else {
                // If empty, yield CPU to let producer run
                std::this_thread::yield();
            }
        }
    });

    // Wait for both to finish
    producer.join();
    consumer.join();

    // Verify data integrity
    BOOST_CHECK_EQUAL(producer_sum, consumer_sum);

    // Calculate expected sum (n * (n-1)) / 2
    constexpr long long expected_sum =
      static_cast<long long>(item_count) * (item_count - 1) / 2;
    BOOST_CHECK_EQUAL(consumer_sum, expected_sum);
}