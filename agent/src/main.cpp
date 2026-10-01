#include <iostream>
#include <thread>
#include "ThreadSafeQueue.hpp"

int main() {
    ThreadSafeQueue<int> q;

    std::thread consumer([&q] {
        while (auto v = q.waitPop()) {
            std::cout << "got " << *v << "\n";
        }
        std::cout << "consumer done\n";
    });

    for (int i = 1; i <= 5; ++i) q.push(i);
    q.stop();
    consumer.join();
}
