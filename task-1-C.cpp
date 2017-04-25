// Реализация метода try_lock для ticket спинлока.
// Провилков Иван. гр.593.

#include <atomic>

class ticket_spinlock {
private:
    using ticket = std::size_t;
public:
    ticket_spinlock()
            : owner_ticket(0)
            , next_free_ticket(0)
    {}

    // Семантика: если мы можем захватить lock без ожидания, то мы захватываем
    // его и возвращаем true, иначе false.
    // Объяснение.
    // Атомарная функция f.compare_exchange_strong(A, B) сравнивает то,
    // что записано в f с A, если они совпадают пишет B в f и возвращает true,
    // иначе возвращает false.
    // Таким образом, мы считываем текущее значение owner_ticket в temp и затем
    // используем функцию compare_exchange_strong, и если next_free_ticket
    // == temp (что эквивалентно взятию lock без ожидания),
    // то мы забираем текущий  next_free_ticket, добавляем на его
    // место temp + 1 и возвращаем true(берем lock),
    // иначе просто возвращаем false.
    // Так как compare_exchange_strong атомарна, то неудачный вызов try_lock
    // не влияет на другие потоки.
    // Таким образом мы реализовали корректный try_lock().
    bool try_lock() {
        temp = owner_ticket.load();
        return next_free_ticket.compare_exchange_strong(temp, temp + 1);
    }

    void lock() {
        ticket this_thread_ticket = next_free_ticket.fetch_add(1);
        while (owner_ticket.load() != this_thread_ticket) {
            // wait
        }
    }

    void unlock() {
        owner_ticket.store(owner_ticket.load() + 1);
    }

private:
    std::atomic<ticket_t> owner_ticket;
    std::atomic<ticket_t> next_free_ticket;
};

