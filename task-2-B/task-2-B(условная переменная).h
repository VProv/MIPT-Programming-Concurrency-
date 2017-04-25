// Задача "Шагающий робот" на условных переменных.
// Провилков Иван. гр.593.
//
#include <condition_variable>
#include <iostream>
#include <mutex>

// Класс робота, умеющего делать два шага.
class Robot {
 public:
  explicit Robot(const bool start_flag = true)
      : flag_(start_flag) {}
  void StepLeft() {
    std::unique_lock<std::mutex> locker(robot_mutex_);

    // Если можем сделать шаг, то делаем, а затем будим поток правого шага,
    // если он на ожидании. Если не можем сделать шаг, то ждем пока сможем и
    // делаем то же самое.
    controller_.wait(locker, [this] {return flag_;});
    DoLeftStep();
  }

  void StepRight() {
    std::unique_lock<std::mutex> locker(robot_mutex_);

    // Если можем сделать шаг, то делаем, а затем будим поток левого шага,
    // если он на ожидании. Если не можем сделать шаг, то ждем пока сможем и
    // делаем то же самое.
    controller_.wait(locker, [this] {return !flag_;});
    DoRightStep();
  }
 private:
  // Делает шаг, эта функция вызывается, когда мы знаем, что шаг сделать можно.
  void DoLeftStep() {
    std::cout << "left" << std::endl;
    flag_ = false;
    controller_.notify_all();
  }

  void DoRightStep() {
    std::cout << "right" << std::endl;
    flag_ = true;
    controller_.notify_one();
  }

  // true - время идти левой ноге, false - время идти правой ноге.
  bool flag_;
  std::mutex robot_mutex_;
  std::condition_variable controller_;
};
