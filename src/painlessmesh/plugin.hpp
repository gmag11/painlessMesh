#ifndef _PAINLESS_MESH_PLUGIN_HPP_
#define _PAINLESS_MESH_PLUGIN_HPP_

#include "Arduino.h"
#include "painlessmesh/configuration.hpp"

#include "painlessmesh/router.hpp"

namespace painlessmesh {
namespace plugin {

class SinglePackage : public protocol::PackageInterface {
 public:
  uint32_t from;
  uint32_t dest;
  router::Type routing;
  int type;
  int noJsonFields = 4;

  SinglePackage(int type) : routing(router::SINGLE), type(type) {}

  SinglePackage(JsonObject jsonObj) {
    from = jsonObj["from"];
    dest = jsonObj["dest"];
    type = jsonObj["type"];
    routing = static_cast<router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj["from"] = from;
    jsonObj["dest"] = dest;
    jsonObj["routing"] = static_cast<int>(routing);
    jsonObj["type"] = type;
    return jsonObj;
  }
};

class BroadcastPackage : public protocol::PackageInterface {
 public:
  uint32_t from;
  router::Type routing;
  int type;
  int noJsonFields = 3;

  BroadcastPackage(int type) : routing(router::BROADCAST), type(type) {}

  BroadcastPackage(JsonObject jsonObj) {
    from = jsonObj["from"];
    type = jsonObj["type"];
    routing = static_cast<router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj["from"] = from;
    jsonObj["routing"] = static_cast<int>(routing);
    jsonObj["type"] = type;
    return jsonObj;
  }
};

/**
 * Handle different plugins
 *
 * Responsible for
 * - having a list of plugin types
 * - the functions defined to handle the different plugin types
 * - tasks?
 */
template <typename T>
class PackageHandler : public layout::Layout<T> {
 public:
  void stop(Scheduler& scheduler) {
    for (auto&& task : taskList) {
      task->disable();
      scheduler.deleteTask(*task);
    }
    taskList.clear();
  }

  ~PackageHandler() {
    if (taskList.size() > 0)
      Log(logger::ERROR,
          "~PackageHandler(): Always call PackageHandler::stop(scheduler) "
          "before calling this destructor");
  }

  bool sendPackage(protocol::PackageInterface* pkg) {
    auto variant = protocol::Variant(pkg);
    // if single or neighbour with direction
    if (variant.routing() == router::SINGLE ||
        (variant.routing() == router::NEIGHBOUR && variant.dest() != 0)) {
      return router::send(variant, (*this));
    }

    // if broadcast or neighbour without direction
    if (variant.routing() == router::BROADCAST ||
        (variant.routing() == router::NEIGHBOUR && variant.dest() == 0)) {
      auto i = router::broadcast(variant, (*this), 0);
      if (i > 0) return true;
      return false;
    }
    return false;
  }

  void onPackage(int type, std::function<bool(protocol::Variant)> function) {
    auto func = [&function](protocol::Variant var, std::shared_ptr<T>,
                            uint32_t) { return function(var); };
    callbackList.onPackage(type, func);
  }

  /**
   * Add a task to the scheduler
   *
   * The task will be stored in a list and a shared_ptr to the task will be
   * returned. If the task is anonymous (i.e. no shared_ptr to it is held
   * anywhere else) and disabled then it will be reused when a new task is
   * added.
   */
  std::shared_ptr<Task> addTask(Scheduler& scheduler, unsigned long aInterval,
                                long aIterations,
                                std::function<void()> aCallback) {
    for (auto && task : taskList) {
      if (task.use_count() == 1 && !task->isEnabled()) {
        task->set(aInterval, aIterations, aCallback, NULL, NULL); 
        task->enable();
        return task;
      }
    }

    std::shared_ptr<Task> task =
        std::make_shared<Task>(aInterval, aIterations, aCallback);
    scheduler.addTask((*task));
    task->enable();
    taskList.push_front(task);
    return task;
  }

  std::shared_ptr<Task> addTask(Scheduler& scheduler,
                                std::function<void()> aCallback) {
    return this->addTask(scheduler, 0, TASK_ONCE, aCallback);
  }

 protected:
  router::MeshCallbackList<T> callbackList;
  std::list<std::shared_ptr<Task> > taskList = {};
};

}  // namespace plugin
}  // namespace painlessmesh
#endif

