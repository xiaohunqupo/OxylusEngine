#include "Stack.hpp"

#include "OS/OS.hpp"

namespace ox::memory {
ThreadStack::ThreadStack() {
  OX_SCOPED_ZONE;

  constexpr static usize stack_size = ox::mib_to_bytes(32);
  ptr = static_cast<u8*>(os::mem_reserve(stack_size));
  os::mem_commit(ptr, stack_size);
}

ThreadStack::~ThreadStack() {
  OX_SCOPED_ZONE;

  os::mem_release(ptr);
}

ScopedStack::ScopedStack() {
  OX_SCOPED_ZONE;

  auto& stack = get_thread_stack();
  ptr = stack.ptr;
}

ScopedStack::~ScopedStack() {
  OX_SCOPED_ZONE;

  auto& stack = get_thread_stack();
  stack.ptr = ptr;
}

} // namespace ox::memory
