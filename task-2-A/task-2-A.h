#pragma once

// Барьер для потоков.
// Провилков Иван. гр.593.
//

#include <condition_variable>
#include <mutex>

// Класс циклического барьера для потоков.
template <class ConditionVariable = std::condition_variable>
class CyclicBarrier {
  // Идея, будем поддерживать два барьера. На первый потоки будут приходить
  // и собираться в группу необходимого размера, а потом будут проходить на
  // второй барьер. После сбора на втором барьере они будут продолжать
  // выполнение. Таким образом мы избавимся от проблем с потоками,
  // только покинувшими циклический барьер(совокупность двух барьеров),
  // которые могут тут же подойти к нему снова,
  // пока остальные потоки его еще не покинули.
 public:
  explicit CyclicBarrier(int64_t num_threads)
      : num_threads_(num_threads),
        barrier_enter_size_(0),
        barrier_exit_size_(0) {}

  void Pass() {
    std::unique_lock<std::mutex> locker(barrier_mutex_);

    // Заходим в первый барьер, если мы последний недостающий поток,
    // то будим всех, иначе ждем.
    GoThroughBarrier(barrier_enter_size_, observer_enter_, locker);

    // Переходим ко второму барьеру, он аналогичен первому.
    GoThroughBarrier(barrier_exit_size_, observer_exit_, locker);
  }

 private:
  // Прохождение через указанный барьер.
  void GoThroughBarrier(int64_t& current_barrier_size,
     ConditionVariable& current_observer,
     std::unique_lock<std::mutex>& locker) {
    if (current_barrier_size == num_threads_ - 1) {
      current_barrier_size = 0;
      current_observer.notify_all();
    } else {
      ++current_barrier_size;
      // Помним, что notify_all только сигнализирует, что нужно проверить
      // предикат, так же предикат защищает от spurious wakeup.
      current_observer.wait(locker, [&] { return current_barrier_size == 0;});
    }
  }
  int64_t num_threads_;
  // Текущее количество потоках на барьерах
  int64_t barrier_enter_size_, barrier_exit_size_;
  std::mutex barrier_mutex_;
  ConditionVariable observer_enter_, observer_exit_;
};

