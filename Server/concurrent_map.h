#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <execution>
#include <atomic>

#include "log_duration.h"

using namespace std::string_literals;

template <typename Container, typename Predicate>
std::vector<typename Container::value_type> CopyIfUnordered(const Container& container,
    Predicate predicate) {
    std::vector<typename Container::value_type> result;
    result.resize(container.size());
    std::atomic<size_t> size_ = 0;
    std::for_each(std::execution::par, container.begin(), container.end(), [&predicate, &size_, &result](const auto& value) {
        if (predicate(value)) {

            result[size_++] = value;

        };
        });
    result.resize(size_);
    return result;
}

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket;
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        // ...
        Access(const Key& key, Bucket& bucket) : guard(bucket.mutex_), ref_to_value(bucket.map[key]) {}
    };

    explicit ConcurrentMap(size_t bucket_count) : buckets(bucket_count) {}

    Access operator[](const Key& key) {
        auto& bucket = buckets[static_cast<uint64_t>(key) % buckets.size()];
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex_, map] : buckets) {
            std::lock_guard guard(mutex_);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    auto Erase(const Key& key) {
        uint64_t tmp_key = static_cast<uint64_t>(key) % buckets.size();
        std::lock_guard guard(buckets[tmp_key].mutex_);
        return buckets[tmp_key].map.erase(key);
    }

private:
    // ...
    struct Bucket {
        std::mutex mutex_;
        std::map<Key, Value> map;
    };
    std::vector<Bucket> buckets;
};