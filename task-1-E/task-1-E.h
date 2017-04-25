#pragma once

// Tournament Tree Mutex.
// Провилков Иван. гр.593.
// Методы названы по кодстайлу, а не для фрэймворка контеста.

#include <atomic>
#include <iostream>
#include <stack>
#include <thread>
#include <vector>

// Мьютекс Петерсона. Взят из лекции.
class PetersonMutex {
 public:

  PetersonMutex()
      : victim_(0) {
    // Initialization list нет для array<atomic<bool>, 2>
    want_[0].store(false);
    want_[1].store(false);
  }

  void Lock(const int thread_index) {
    want_[thread_index].store(true);
    victim_.store(thread_index);
    while (want_[1 - thread_index].load() && victim_.load() == thread_index) {
      std::this_thread::yield();
    }
  }

  void Unlock(const int thread_index) {
    want_[thread_index].store(false);
  }

 private:
  std::array<std::atomic<bool>, 2> want_;
  std::atomic<int> victim_;
};

// Tournament tree mutex.
class TreeMutex {
 public:

  // Количество мьютексов равно количеству потоков, если оно нечетное, и
  // количеству потоков - 1,
  // если их число четное(легко выводится если просто просуммировать,
  // я беру 2 запасных ячейки).
  // Поэтому нет необходимости выделять полное дерево размера 2^n.
  TreeMutex(const int n_threads)
      : tree_mutex_(n_threads + 2) {
    int64_t thred_num_ = Pow2(n_threads);
    vertices_count_.store(thred_num_ - 1);
  }

  // Рассматриваем поданный поток как лист и поднимаемся по его родителям к
  // корню, делая Lock() во всех мьютексах на пути.
  void Lock(const int threadIndex) {
    int thread_position = threadIndex + vertices_count_.load();
    while (thread_position != 0) {
      int previous = Parent(thread_position);
      tree_mutex_[previous].Lock(GetOrient(thread_position));
      thread_position = previous;
    }
  }

  // Ради экономии памяти мы не записываем все возможные пути от листьев
  // к корню изначально, а каждый раз ищем путь по новой,
  // чтобы сделать Unlock корректно, делаем это рекурсивно.
  void Unlock(const int threadIndex) {
    int thread_position = threadIndex + vertices_count_.load();
    RecursionForUnlock(thread_position);
  }

 private:
  std::vector<PetersonMutex> tree_mutex_;
  std::atomic<int> vertices_count_;

  // Функция делающая рекурсивный Unlock, от указанного мьютекса.
  void RecursionForUnlock(const int index) {
    int previous = Parent(index);
    if (index != 0) {
      RecursionForUnlock(previous);
      tree_mutex_[previous].Unlock(GetOrient(index));
    }
  }

  static int Parent(const int t) {
    return t == 0 ? 0 : (t - 1) / 2;
  }

  static int Left(const int i) {
    return 2 * i + 1;
  }

  static int Right(const int i)  {
    return 2 * i + 2;
  }

  // Возвращает 1, если vertex в правой ветке дерева и 0 если в левой.
  static int GetOrient(const int vertex) {
    return vertex % 2 == 0 ? 1 : 0;
  }

  // Функция подсчета степени двойки, не меньшей чем данное число.
  static int64_t Pow2(const int64_t number) {
    int64_t power = 1;
    while (power < number) {
      power *= 2;
    }
    return power;
  }
};
