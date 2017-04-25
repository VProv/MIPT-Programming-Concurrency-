#pragma once

// Оптимистичный список.
// Провилков Иван.

#include "arena_allocator.h"

#include <atomic>
#include <limits>
#include <mutex>

///////////////////////////////////////////////////////////////////////

template <typename T>
struct ElementTraits {
  static T Min() {
    return std::numeric_limits<T>::min();
  }
  static T Max() {
    return std::numeric_limits<T>::max();
  }
};

///////////////////////////////////////////////////////////////////////

// Test-and-test-and-set spinlock.
class SpinLock {
 public:
  explicit SpinLock()
      : observer_(false) {}

  void Lock() {
    while (true) {
      while(observer_) {
        std::this_thread::yield();
      }
      if (!observer_.exchange(true)) {
        return;
      }
    }
  }

  void Unlock() {
    observer_.store(false);
  }

  // adapters for BasicLockable concept

  void lock() {
    Lock();
  }

  void unlock() {
    Unlock();
  }

 private:
  std::atomic<bool> observer_;
};

///////////////////////////////////////////////////////////////////////

template <typename T>
class OptimisticLinkedSet {
 private:
  struct Node {
    T element_;
    std::atomic<Node*> next_;
    SpinLock lock_{};
    std::atomic<bool> marked_{false};

    Node(const T& element, Node* next = nullptr)
        : element_(element),
          next_(next) {
    }
  };

  struct Edge {
    Node* pred_;
    Node* curr_;

    Edge(Node* pred, Node* curr)
        : pred_(pred),
          curr_(curr) {
    }
  };

 public:
  explicit OptimisticLinkedSet(ArenaAllocator& allocator)
      : allocator_(allocator), size_(0) {
    CreateEmptyList();
  }

  bool Insert(const T& element) {
    while(true) {
      Edge position = Locate(element);
      std::unique_lock <SpinLock> previous_locker(position.pred_->lock_);
      std::unique_lock <SpinLock> current_locker(position.curr_->lock_);
      if (Validate(position)) {
        if (position.curr_->element_ == element) {
          return false;
        } else {
          Node *new_node = allocator_.New<Node>(element);
          new_node->next_ = position.curr_;
          position.pred_->next_ = new_node;
          ++size_;
          return true;
        }
      }
    }
    return false;
  }

  bool Remove(const T& element) {
    while (true) {
      Edge position = Locate(element);
      std::unique_lock<SpinLock> previous_locker(position.pred_->lock_);
      std::unique_lock<SpinLock> current_locker(position.curr_->lock_);
      if (Validate(position)) {
        if (position.curr_->element_ != element) {
          return false;
        } else {
          position.curr_->marked_ = true;
          position.pred_->next_.store(position.curr_->next_.load());
          --size_;
          return true;
        }
      }
    }
    return false;
  }

  bool Contains(const T& element) const {
    Node* current = head_;
    while (current->element_ < element) {
      current = current->next_;
    }
    return current->element_ == element && !current->marked_;
  }

  size_t Size() const {
    return size_;
  }

 private:
  void CreateEmptyList() {
    head_ = allocator_.New<Node>(ElementTraits<T>::Min());
    head_->next_ = allocator_.New<Node>(ElementTraits<T>::Max());
  }

  Edge Locate(const T&  element) const {
    Node* previous = this->head_;
    Node* current = head_->next_;
    while(current->element_ < element) {
      previous = current;
      current = current->next_;
    }
    return Edge{previous, current};
  }

  bool Validate(const Edge& edge) const {
    return !edge.curr_->marked_ && !edge.pred_->marked_ &&
        edge.pred_->next_ == edge.curr_;
  }

 private:
  ArenaAllocator& allocator_;
  Node* head_{nullptr};
  std::atomic<size_t> size_;
};

template <typename T> using ConcurrentSet = OptimisticLinkedSet<T>;