#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ox {
class Command {
public:
  virtual ~Command() = default;
  virtual auto execute() -> void = 0;
  virtual auto undo() -> void = 0;
  virtual auto get_id() const -> u64 = 0;
  virtual auto can_merge(const Command& other) const -> bool { return false; }
  virtual auto merge(std::unique_ptr<Command> other) -> std::unique_ptr<Command> { return nullptr; }
};

class LambdaCommand : public Command {
public:
  LambdaCommand(std::function<void()> execute,
                std::function<void()> undo,
                const usize loc = std::source_location::current().line())
      : _execute_func(std::move(execute)),
        _undo_func(std::move(undo)),
        _id(loc) {}

  auto execute() -> void override { _execute_func(); }
  auto undo() -> void override { _undo_func(); }
  auto get_id() const -> u64 override { return _id; }

private:
  std::function<void()> _execute_func;
  std::function<void()> _undo_func;
  u64 _id;
};

class CommandGroup : public Command {
public:
  explicit CommandGroup(const usize loc = std::source_location::current().line()) : _id(loc) {}

  template <typename T, typename... Args>
  auto add_command(Args&&... args) -> void {
    _commands.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  auto add_command(std::unique_ptr<Command> command) -> void { _commands.emplace_back(std::move(command)); }

  auto execute() -> void override {
    for (auto& cmd : _commands) {
      cmd->execute();
    }
  }

  auto undo() -> void override {
    for (auto it = _commands.rbegin(); it != _commands.rend(); ++it) {
      (*it)->undo();
    }
  }

  auto get_id() const -> u64 override { return _id; }

  auto empty() const -> bool { return _commands.empty(); }
  auto size() const -> usize { return _commands.size(); }

private:
  std::vector<std::unique_ptr<Command>> _commands;
  u64 _id;
};

class UndoRedoSystem {
public:
  explicit UndoRedoSystem(size_t max_history = 100,
                          bool merge_enabled = true,
                          std::chrono::milliseconds merge_timeout = std::chrono::milliseconds(500))
      : _max_history_size(max_history),
        _merge_enabled(merge_enabled),
        _merge_timeout(merge_timeout),
        _last_command_time(std::chrono::steady_clock::now()) {}

  template <typename T, typename... Args>
  auto execute_command(this UndoRedoSystem& self, Args&&... args) -> UndoRedoSystem& {
    auto command = std::make_unique<T>(std::forward<Args>(args)...);
    self.execute_command(std::move(command));
    return self;
  }

  auto execute_command(this UndoRedoSystem& self, std::unique_ptr<Command> command) -> UndoRedoSystem& {
    if (self._merge_enabled && !self._undo_stack.empty()) {
      auto now = std::chrono::steady_clock::now();
      if (now - self._last_command_time < self._merge_timeout && self._undo_stack.back()->can_merge(*command)) {
        if (auto merged = self._undo_stack.back()->merge(std::move(command))) {
          self._undo_stack.back() = std::move(merged);
          self._last_command_time = now;
          return self;
        }
      }
    }

    OX_CHECK_NULL(command);

    command->execute();

    self._undo_stack.emplace_back(std::move(command));

    self._redo_stack.clear();

    if (self._undo_stack.size() > self._max_history_size) {
      self._undo_stack.erase(self._undo_stack.begin());
    }

    self._last_command_time = std::chrono::steady_clock::now();

    return self;
  }

  auto execute_lambda(this UndoRedoSystem& self,
                      std::function<void()> execute,
                      std::function<void()> undo,
                      const usize loc = std::source_location::current().line()) -> UndoRedoSystem& {
    self.execute_command(std::make_unique<LambdaCommand>(std::move(execute), std::move(undo), loc));
    return self;
  }

  auto begin_group(this UndoRedoSystem&, const usize loc = std::source_location::current().line())
      -> std::unique_ptr<CommandGroup> {
    return std::make_unique<CommandGroup>(loc);
  }

  auto execute_group(this UndoRedoSystem& self, std::unique_ptr<CommandGroup> group) -> UndoRedoSystem& {
    if (!group->empty()) {
      self.execute_command(std::move(group));
    }
    return self;
  }

  auto undo(this UndoRedoSystem& self) -> bool {
    if (self._undo_stack.empty())
      return false;

    auto command = std::move(self._undo_stack.back());
    self._undo_stack.pop_back();

    command->undo();
    self._redo_stack.emplace_back(std::move(command));

    return true;
  }

  auto redo(this UndoRedoSystem& self) -> bool {
    if (self._redo_stack.empty())
      return false;

    auto command = std::move(self._redo_stack.back());
    self._redo_stack.pop_back();

    command->execute();
    self._undo_stack.emplace_back(std::move(command));

    return true;
  }

  auto can_undo(this const UndoRedoSystem& self) -> bool { return !self._undo_stack.empty(); }
  auto can_redo(this const UndoRedoSystem& self) -> bool { return !self._redo_stack.empty(); }

  auto get_undo_count(this const UndoRedoSystem& self) -> usize { return self._undo_stack.size(); }
  auto get_redo_count(this const UndoRedoSystem& self) -> usize { return self._redo_stack.size(); }

  auto clear(this UndoRedoSystem& self) -> void {
    self._undo_stack.clear();
    self._redo_stack.clear();
  }

  auto get_merge_timeout(this const UndoRedoSystem& self) -> std::chrono::milliseconds { return self._merge_timeout; }
  auto set_merge_enabled(this UndoRedoSystem& self, bool enabled) -> UndoRedoSystem& {
    self._merge_enabled = enabled;
    return self;
  }
  auto set_merge_timeout(this UndoRedoSystem& self, std::chrono::milliseconds timeout) -> UndoRedoSystem& {
    self._merge_timeout = timeout;
    return self;
  }
  auto set_max_history_size(this UndoRedoSystem& self, usize size) -> UndoRedoSystem& {
    self._max_history_size = size;
    while (self._undo_stack.size() > self._max_history_size) {
      self._undo_stack.erase(self._undo_stack.begin());
    }
    return self;
  }

private:
  std::vector<std::unique_ptr<Command>> _undo_stack;
  std::vector<std::unique_ptr<Command>> _redo_stack;
  usize _max_history_size;
  bool _merge_enabled;
  std::chrono::milliseconds _merge_timeout;
  std::chrono::steady_clock::time_point _last_command_time;
};

template <typename T>
class PropertyChangeCommand : public Command {
public:
  PropertyChangeCommand(T* target, T old_val, T new_val, const usize loc = std::source_location::current().line())
      : _target(target),
        _old_value(old_val),
        _new_value(new_val),
        _id(loc) {}

  auto execute() -> void override { *_target = _new_value; }

  auto undo() -> void override { *_target = _old_value; }

  auto get_id() const -> u64 override { return _id; }

  auto can_merge(const Command& other) const -> bool override {
    if (auto* other_cmd = dynamic_cast<const PropertyChangeCommand<T>*>(&other)) {
      return _target == other_cmd->_target && _id == other_cmd->_id;
    }
    return false;
  }

  auto merge(std::unique_ptr<Command> other) -> std::unique_ptr<Command> override {
    if (auto other_cmd = dynamic_cast<PropertyChangeCommand<T>*>(other.get())) {
      auto merged = std::make_unique<PropertyChangeCommand<T>>(_target, _old_value, other_cmd->_new_value, _id);
      other.release();
      return merged;
    }
    return nullptr;
  }

private:
  T* _target;
  T _old_value;
  T _new_value;
  u64 _id;
};
} // namespace ox
