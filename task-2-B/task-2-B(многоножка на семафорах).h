#pragma once

// Задача "Шагающий робот" на семафорах для n ног.
// Провилков Иван. гр.593.
//

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

// Класс семафора, который разрешает одновременно выполняться только
// одному потоку.
class Semaphore {
 public:

  Semaphore()
      : counter_(false) {};
  // Если есть жетон, то сразу берем его и заходим, если нет то ждем
  // пока появится.
  void Wait() {
    std::unique_lock<std::mutex> locker(semaphore_mutex_);
    flow_queue_.wait(locker, [this] {return counter_;});
    counter_ = false;
  }

  // Сигнализируем, что можно зайти и будим один из потоков в очереди.
  void Signal() {
    std::unique_lock<std::mutex> locker(semaphore_mutex_);
    counter_ = true;
    flow_queue_.notify_one();
  }

 private:
  // true - можно зайти, false - нельзя.
  bool counter_;
  std::condition_variable flow_queue_;
  std::mutex semaphore_mutex_;
};

// Робот поочередно делающий два шага, начиная с первой ноги.
// Реализован на n семафорах, которые поочередно передают жетон друг другу.
class Robot {
 public:

  explicit Robot(const int foots_count)
      : legs_(foots_count) {
    legs_.front().Signal();
  }

  // Команда делающая n-й шаг.
  void Step(const int foot_number) {
    legs_[foot_number].Wait();
    std::cout << "foot " << foot_number << std::endl;
    legs_[(foot_number + 1) % legs_.size()].Signal();
  }

 private:
  // По симафору на каждую ногу. Нулевая нога в нуле.
  std::vector<Semaphore> legs_;
};
