#pragma once
// Пул потоков.
// Провилков Иван. группа 593.

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Многопоточная блокирующая очередь.
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

// Пул потоков.
template <class T>
class ThreadPool {
 public:
  ThreadPool(ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  explicit ThreadPool(const size_t& num_threads = DefaultNumWorkers())
      : workers_number_(num_threads), pool_is_working_(true),
        task_queue_(std::numeric_limits<size_t>::max()) {
    for (uint64_t i = 0; i < workers_number_; ++i) {
      // Раздаем задачи потокам.
      threads_.emplace_back(&ThreadPool::EnableWorker, this);
    }
  }

  // Добавляет задачу в конец очереди пула, через future можно получить
  // результат задачи.
  std::future<T> Submit(std::function<T()> task) {
    std::packaged_task<T()> current_task(task);
    std::future<T> future = current_task.get_future();
    // Здесь выкинется исключение, если ранее был сделан Shutdown.
    task_queue_.Put(std::move(current_task));
    return future;
  }

  void Shutdown() {
    if (pool_is_working_) {
      pool_is_working_ = false;
      std::unique_lock<std::mutex> locker(mutex_);
      task_queue_.Shutdown();
      workers_observer_.wait(locker, [this] { return workers_number_ == 0; });
      for (uint64_t i = 0; i < threads_.size(); ++i) {
        threads_[i].join();
      }
    }
  }

  ~ThreadPool() {
    Shutdown();
  }

 private:
  static int64_t DefaultNumWorkers(){
    int default_size = std::thread::hardware_concurrency();
    return  bool(default_size) ? default_size : 4;
  }

  void EnableWorker() {
    while (true) {
      std::packaged_task<T()> task;
      if (task_queue_.Get(task)) {
        task();
      } else {
        --workers_number_;
        if (workers_number_ == 0)
          workers_observer_.notify_one();
        break;
      }
    }
  }

  std::vector<std::thread> threads_;
  std::atomic<size_t> workers_number_;
  std::atomic<bool> pool_is_working_;
  std::mutex mutex_;
  BlockingQueue<std::packaged_task<T()> > task_queue_;
  std::condition_variable workers_observer_;
};
