#include <iostream>
#include <atomic>
#include <string>
#include <vector>
#include "aare/ProducerConsumerQueue.hpp"

struct Tracker {
    static std::atomic<int> ctors;
    static std::atomic<int> dtors;
    static std::atomic<int> moves;
    static std::atomic<int> live;

    std::string tag;
    std::vector<int> buf;

    Tracker() = delete;
    explicit Tracker(int id)
        : tag("T" + std::to_string(id)), buf(1 << 18, id) 
    {
        ++ctors; ++live;
    }
    
    Tracker(Tracker&& other) noexcept
        : tag(std::move(other.tag)), buf(std::move(other.buf)) 
    {
        ++moves; 
        ++ctors; 
        ++live;
    }

    Tracker& operator=(Tracker&&) = delete;
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    ~Tracker() 
    {
        ++dtors; --live;
    }
};

std::atomic<int> Tracker::ctors{0};
std::atomic<int> Tracker::dtors{0};
std::atomic<int> Tracker::moves{0};
std::atomic<int> Tracker::live{0};

int main() {
    using Queue = aare::ProducerConsumerQueue<Tracker>;

    // Scope make sure destructors have ran before we check the counters.
    {
        Queue q1(8);
        Queue q2(8);

        for (int i = 0; i < 3; ++i) q2.write(Tracker(100 + i));
        for (int i = 0; i < 5; ++i) q1.write(Tracker(200 + i));

        q2 = std::move(q1);

        Tracker tmp(9999);
        if (auto* p = q2.frontPtr()) 
        {
            (void)p;
        }
    }

    std::cout << "ctors=" << Tracker::ctors.load()
            << " dtors=" << Tracker::dtors.load()
            << " moves=" << Tracker::moves.load()
            << " live="  << Tracker::live.load()
            << "\n";

    bool ok = (Tracker::ctors.load() == Tracker::dtors.load()) && (Tracker::live.load() == 0);

    if (!ok) 
    {
        std::cerr << "Leak or skipped destructors detected (move-assignment bug)\n";
        return 1;
    }

    std::cout << "No leaks; move-assignment cleans up correctly\n";
    
    return 0;
}