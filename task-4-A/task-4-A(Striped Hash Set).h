#pragma once
// Многопоточная хэш-таблица.
// Провилков Иван

#include <algorithm>
#include <atomic>
#include <forward_list>
#include <functional>
#include <mutex>
#include <vector>

template <typename T, class Hash = std::hash<T>>
class StripedHashSet {
 public:
  explicit StripedHashSet(const size_t concurrency_level,
                          const size_t growth_factor = 3,
                          const double max_load_factor = 0.75)
      : growth_factor_(growth_factor), max_load_factor_(max_load_factor),
        stripes_(concurrency_level), buckets_(concurrency_level * 3),
        size_(0) {}
  bool Insert(const T& element) {
    size_t hash_value = hash_function_(element);
    std::unique_lock<std::mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    if (find_element(hash_value, element)) {
      // Элемент уже был в контейнере.
      return false;
    } else {
      // Вставляем элемент, а затем делаем проверку на рехэш.
      buckets_[GetBucketIndex(hash_value)].push_front(element);
      ++size_;
      if (TimeToRehash()) {
        Rehash(locker);
      }
      return true;
    }
  }

  bool Remove(const T& element) {
    size_t hash_value = hash_function_(element);
    std::unique_lock<std::mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    if (find_element(hash_value, element)) {
      buckets_[GetBucketIndex(hash_value)].remove(element);
      --size_;
      return true;
    } else {
      return false;
    }
  }
  bool Contains(const T& element) {
    size_t hash_value = hash_function_(element);
    std::unique_lock<std::mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    if (find_element(hash_value, element)) {
      return true;
    } else {
      return false;
    }
  }

  size_t Size()const {
    return size_;
  }
 private:
  size_t GetBucketIndex(const size_t hash_value)const {
    return hash_value % buckets_.size();
  }
  size_t GetStripeIndex(const size_t hash_value)const {
    return hash_value % stripes_.size();
  }
  bool TimeToRehash()const {
    return size_ >= max_load_factor_ * buckets_.size();
  }
  bool find_element(const size_t hash_value, const T& element)const {
    return std::find(buckets_[GetBucketIndex(hash_value)].begin(),
                     buckets_[GetBucketIndex(hash_value)].end(), element) !=
        buckets_[GetBucketIndex(hash_value)].end();
  }
  void Rehash(std::unique_lock<std::mutex>& current_lock) {
    current_lock.unlock();
    std::vector<std::unique_lock<std::mutex>> lockers;
    lockers.emplace_back(std::unique_lock<std::mutex>(stripes_[0]));
    if (!TimeToRehash())
      return;
    for (size_t i = 1; i < stripes_.size(); ++i) {
      lockers.emplace_back(std::unique_lock<std::mutex>(stripes_[i]));
    }
    std::vector<std::forward_list<T>> past_buckets(buckets_.size() *
        growth_factor_);
    past_buckets.swap(buckets_);
    for (auto bucket: past_buckets) {
      for (auto element: bucket) {
        size_t hash_value = hash_function_(element);
        buckets_[GetBucketIndex(hash_value)].push_front(element);
      }
    }
    return;
  }
  size_t growth_factor_;
  double max_load_factor_;
  std::vector<std::mutex> stripes_;
  std::vector<std::forward_list<T> > buckets_;
  // Количество элементов в множестве.
  std::atomic<size_t> size_;
  Hash hash_function_;
};

template <typename T> using ConcurrentSet = StripedHashSet<T>;