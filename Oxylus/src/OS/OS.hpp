#pragma once

namespace ox {
namespace os {
    //  ── MEMORY ──────────────────────────────────────────────────────────
    auto mem_page_size() -> uint64;
    auto mem_reserve(uint64 size) -> void *;
    auto mem_release(void *data, uint64 size = 0) -> void;
    auto mem_commit(void *data, uint64 size) -> bool;
    auto mem_decommit(void *data, uint64 size) -> void;
} // namespace os
}
