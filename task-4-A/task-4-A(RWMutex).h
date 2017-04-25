#pragma once
// Многопоточная хэш-таблица с использованием RWMutex с приоритетом у писателей.
// Провилков Иван

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <vector>


// Reader-writer mutex с приоритетом для писателей.
class RWMutex {
 public:
  explicit RWMutex() : writing_(false), readers_(0), writers_(0) {}

  void WriterLock() {
    std::unique_lock<std::mutex> locker(gate_);
    ++writers_;
    writers_observer_.wait(locker, [this] {
                            return !(writing_ || readers_ > 0);
                          });
    writing_ = true;
  }

  void WriterUnlock() {
    std::unique_lock<std::mutex> locker(gate_);
    --writers_;
    writing_ = false;
    if (writers_ > 0) {
      writers_observer_.notify_one();
    } else {
      readers_observer_.notify_all();
    }
  }

  void ReaderLock() {
    std::unique_lock<std::mutex> locker(gate_);
    readers_observer_.wait(locker, [this] {return writers_ == 0;});
    ++readers_;
  }

  void ReaderUnlock() {
    std::unique_lock<std::mutex> locker(gate_);
    --readers_;
    if (readers_ == 0)
      writers_observer_.notify_one();
  }

  // Сделаем соответствующие названия, для корректного вызова lock-ов.
  void lock() {
    WriterLock();
  }

  void unlock() {
    WriterUnlock();
  }

  void lock_shared() {
    ReaderLock();
  }

  void unlock_shared() {
    ReaderUnlock();
  }

 private:
  std::atomic<bool> writing_;
  std::atomic<size_t> readers_;
  std::atomic<size_t> writers_;
  std::mutex gate_;
  std::condition_variable readers_observer_;
  std::condition_variable writers_observer_;
};

template <typename T, class Hash = std::hash<T>>
class StripedHashSet {
 public:
  using mutex = RWMutex;
  explicit StripedHashSet(const size_t concurrency_level,
                          const size_t growth_factor = 3,
                          const double max_load_factor = 0.75)
      : growth_factor_(growth_factor),
        max_load_factor_(max_load_factor),
        stripes_(concurrency_level),
        buckets_(concurrency_level * 3),
        size_(0) {}

  bool Insert(const T& element) {
    size_t hash_value = hash_function_(element);
    std::unique_lock<mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    if (FindElement(hash_value, element)) {
      // Элемент уже был в контейнере.
      return false;
    } else {
      // Вставляем элемент, а затем делаем проверку на рехэш.
      buckets_[GetBucketIndex(hash_value)].push_front(element);
      ++size_;
      if (TimeToRehash()) {
        locker.unlock();
        Rehash();
      }
      return true;
    }
  }

  bool Remove(const T& element) {
    const size_t hash_value = hash_function_(element);
    std::unique_lock<mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    if (FindElement(hash_value, element)) {
      buckets_[GetBucketIndex(hash_value)].remove(element);
      --size_;
      return true;
    } else {
      return false;
    }
  }

  bool Contains(const T& element) {
    size_t hash_value = hash_function_(element);
    std::shared_lock<mutex> locker(stripes_[GetStripeIndex(hash_value)]);
    return FindElement(hash_value, element);
  }

  size_t Size()const {
    return size_;
  }

 private:
  size_t GetBucketIndex(const size_t hash_value) const {
    return hash_value % buckets_.size();
  }

  size_t GetStripeIndex(const size_t hash_value) const {
    return hash_value % stripes_.size();
  }

  bool TimeToRehash() const {
    return size_ >= max_load_factor_ * buckets_.size();
  }

  bool FindElement(const size_t hash_value, const T& element) const {
    return std::find(buckets_[GetBucketIndex(hash_value)].begin(),
                     buckets_[GetBucketIndex(hash_value)].end(), element) !=
        buckets_[GetBucketIndex(hash_value)].end();
  }

  void Rehash() {
    std::vector<std::unique_lock<mutex>> lockers;
    lockers.emplace_back(std::unique_lock<mutex>(stripes_[0]));
    if (!TimeToRehash())
      return;
    for (size_t i = 1; i < stripes_.size(); ++i) {
      lockers.emplace_back(std::unique_lock<mutex>(stripes_[i]));
    }
    std::vector<std::forward_list<T>> past_buckets(buckets_.size() *
                                                    growth_factor_);
    past_buckets.swap(buckets_);
    for (const auto& bucket: past_buckets) {
      for (const auto& element: bucket) {
        const size_t hash_value = hash_function_(element);
        buckets_[GetBucketIndex(hash_value)].push_front(element);
      }
    }
    return;
  }

  const size_t growth_factor_;
  const double max_load_factor_;
  std::vector<mutex> stripes_;
  std::vector<std::forward_list<T> > buckets_;
  std::atomic<size_t> size_;
  Hash hash_function_;
};

template <typename T> using ConcurrentSet = StripedHashSet<T>;
