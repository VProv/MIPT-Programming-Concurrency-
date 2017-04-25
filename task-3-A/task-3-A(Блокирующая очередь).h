#pragma once
// Многопоточная блокирующая очередь.
// Провилков Иван. группа 593.

#include <condition_variable>
#include <deque>
#include <exception>
#include <future>
#include <iostream>
#include <mutex>
#include <string>

class BlockingQueueException : public std::exception {
 public:
  explicit BlockingQueueException(const std::string& log)
      : log_(log) {}
  std::string log_;
};

template <class T, class Container = std::deque<T> >
class BlockingQueue {
  // Блокирующая очередь фиксированной вместимости.
 public:
  explicit BlockingQueue(const size_t& capacity)
      : capacity_(capacity), queue_is_working_(true) {}
  void Put(T&& element) {
    std::unique_lock<std::mutex> locker(mutex_);
    put_observer_.wait(locker, [this]() {return !queue_is_working_ ||
        queue_.size() < capacity_;});
    if (!queue_is_working_) {
      throw BlockingQueueException("Try put to disabled queue");
    }
    queue_.push_back(std::move(element));
    get_observer_.notify_one();
  }
  bool Get(T& result) {
    std::unique_lock<std::mutex> lock(mutex_);
    get_observer_.wait(lock, [this]() {return queue_.size() != 0 ||
        !queue_is_working_;});
    if (!queue_is_working_ && queue_.size() == 0) {
      return false;
    }
    result = std::move(queue_.front());
    queue_.pop_front();
    put_observer_.notify_one();
    return true;
  }
  void Shutdown() {
    // Захватываем мьютекс, так как иначе может оказаться, что мы сделали
    // Shutdown, после того как поток проверил что очередь еще работает, но до
    // того как он встал в wait в функции get, и тогда он так и будет спать.
    std::unique_lock<std::mutex> lock(mutex_);
    queue_is_working_ = false;
    put_observer_.notify_all();
    get_observer_.notify_all();
  }
 private:
  std::mutex mutex_;
  size_t capacity_;
  Container queue_;
  std::condition_variable put_observer_, get_observer_;
  bool queue_is_working_;
};
