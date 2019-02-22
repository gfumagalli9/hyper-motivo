//
// Created by steven on 1/19/19.
//

#ifndef MOTIVO_THREADSAFECACHE_H
#define MOTIVO_THREADSAFECACHE_H

#include <map> //FIXME: What should we use?
#include <mutex>
#include <shared_mutex>

template <typename K, typename V> class ThreadSafeCache
{
private:
    std::map<K, V> map;
    std::shared_mutex mutex;

    const V invalid_value;

public:
    explicit ThreadSafeCache(V invalid_value) : invalid_value(invalid_value)
    {}

    V get(const K &key)
    {
        std::shared_lock lock(mutex);
        if(const auto it = map.find(key); it!=map.cend())
            return *it;

        return invalid_value;
    }

    void cache(const K &key, const V &value)
    {
        std::unique_lock lock(mutex);
        map[key]=value;
    }

    template <typename ComputeFnc> V get_or_compute_and_cache(const K &key, ComputeFnc fnc)
    {
        mutex.lock_shared();
        if(const auto it = map.find(key); it!=map.cend())
            return *it;

        mutex.unlock(); //ComputeFnc might be slow
        const &V value = fnc();

        mutex.lock();
        map[key] = value;
        mutex.unlock();
        return value;
    }
};


#endif //MOTIVO_THREADSAFECACHE_H
