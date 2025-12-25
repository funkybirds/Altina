#include "TestHarness.h"

#include "Container/ThreadSafeQueue.h"
#include "Container/ThreadSafeStack.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace AltinaEngine::Core::Container;

TEST_CASE("TThreadSafeQueue concurrent producers/consumer")
{
    TThreadSafeQueue<int> Q;
    const int Producers = 4;
    const int ItemsPerProducer = 2000;
    const int Total = Producers * ItemsPerProducer;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    // Start consumer
    std::thread consumer([&]() {
        while (consumed.load() < Total) {
            if (!Q.IsEmpty()) {
                int v = Q.Front();
                Q.Pop();
                consumed.fetch_add(1);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });

    // Start producers
    std::vector<std::thread> workers;
    for (int p = 0; p < Producers; ++p) {
        workers.emplace_back([&]() {
            for (int i = 0; i < ItemsPerProducer; ++i) {
                Q.Push(i);
                produced.fetch_add(1);
            }
        });
    }

    for (auto &t : workers) t.join();
    // wait for consumer
    consumer.join();

    REQUIRE_EQ(produced.load(), Total);
    REQUIRE_EQ(consumed.load(), Total);
}

TEST_CASE("TThreadSafeStack concurrent producers/consumer")
{
    TThreadSafeStack<int> S;
    const int Producers = 4;
    const int ItemsPerProducer = 2000;
    const int Total = Producers * ItemsPerProducer;

    std::atomic<int> produced{0};
    std::atomic<int> popped{0};

    // Start popper
    std::thread popper([&]() {
        while (popped.load() < Total) {
            if (!S.IsEmpty()) {
                int v = S.Top();
                S.Pop();
                popped.fetch_add(1);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });

    // Producers
    std::vector<std::thread> workers;
    for (int p = 0; p < Producers; ++p) {
        workers.emplace_back([&]() {
            for (int i = 0; i < ItemsPerProducer; ++i) {
                S.Push(i);
                produced.fetch_add(1);
            }
        });
    }

    for (auto &t : workers) t.join();
    popper.join();

    REQUIRE_EQ(produced.load(), Total);
    REQUIRE_EQ(popped.load(), Total);
}
