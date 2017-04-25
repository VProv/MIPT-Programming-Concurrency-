#pragma once

// Задача "Шагающий робот" на семафорах для двух ног.
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

// Робот поочередно делающий два шага, начиная с левой ноги.
// Реализован на двух семафорах, которые поочередно передают жетон друг другу.
class Robot {
 public:

  explicit Robot()
      : legs_(2) {
    // Говорим, что левая нога идет первой.
    legs_[0].Signal();
  }

  void StepLeft() {
    legs_[0].Wait();
    std::cout << "left" << std::endl;
    legs_[1].Signal();
  }

  void StepRight() {
    legs_[1].Wait();
    std::cout << "right" << std::endl;
    legs_[0].Signal();
  }

 private:
  std::vector<Semaphore> legs_;
};
