
#ifndef SRSGNB_BLOCKING_WORKER_H
#define SRSGNB_BLOCKING_WORKER_H

#include "srsgnb/adt/circular_buffer.h"
#include "task_executor.h"

namespace srsgnb {

/// \brief Contrarily to other type of workers, this worker runs in the same thread where run() is called.
/// run() is blocking.
class blocking_worker final : public task_executor
{
public:
  blocking_worker(size_t q_size) : pending_tasks(q_size) {}

  void execute(unique_task task) override { pending_tasks.push_blocking(std::move(task)); }

  void defer(unique_task task) override { execute(std::move(task)); }

  void request_stop()
  {
    execute([this]() {
      if (not pending_tasks.is_stopped()) {
        pending_tasks.stop();
      }
    });
  }

  void run()
  {
    while (true) {
      bool        success;
      unique_task t = pending_tasks.pop_blocking(&success);
      if (not success) {
        break;
      }
      t();
    }
  }

private:
  dyn_blocking_queue<unique_task> pending_tasks;
};

} // namespace srsgnb

#endif // SRSGNB_BLOCKING_WORKER_H
