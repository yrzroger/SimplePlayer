#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <queue>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T>
class SafeQueue
{
public:
    SafeQueue(void)
        : q()
        , m()
    {}

    ~SafeQueue(void)
    {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        T val = q.front();
        q.pop();
        return val;
    }

    size_t size()
    {
        return q.size();
    }
    
    bool empty()
    {
        return q.empty();
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
};
#endif
