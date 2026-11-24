---
name: obs-plugin-reviewing
description: Audit OBS Studio plugins for correctness, performance, memory safety, and thread safety. Covers module registration, callback implementations, resource management, and audio-specific patterns.
---

# OBS Plugin Code Review & Quality Audit

Checklists, rules, and commands to audit OBS Studio plugins for correctness, stability, memory safety, and performance.

## Purpose

Audit OBS Studio plugins for correctness, performance, memory safety, and thread safety. Covers module registration, callback implementations, resource management, and audio-specific patterns.

## When to Use

- Performing code review on a pull request or plugin before release.
- Investigating crashes, segmentation faults, or deadlock issues in OBS.
- Checking for memory leaks (`bzalloc`/`bfree` balance).
- Auditing real-time audio threads for illegal blocking operations or allocations.

## When NOT to Use

- Writing new plugins from scratch &rarr; Use `obs-plugin-developing` or `obs-audio-plugin-writing`
- Cross-compiling or building &rarr; Use `obs-windows-building` or `obs-cross-compiling`
- Comprehensive architecture guidance &rarr; Use `obs-plugin-expert` agent

---

## Severity Checklist

### P0 - CRITICAL (Must Fix Before Release)

| Check | Anti-Pattern | Audit Command |
| :--- | :--- | :--- |
| **Missing module macro** | No `OBS_DECLARE_MODULE()` | `grep -r "OBS_DECLARE_MODULE" src/` |
| **Module load returns false** | `return false` in `obs_module_load` | `grep -A5 "obs_module_load" src/` |
| **Memory leak in destroy** | Missing `bfree()` in destroy callback | Manual check of all context allocs |
| **Blocking in audio callback** | Mutex/sleep/alloc in `filter_audio` | `grep -A20 "filter_audio" src/` |
| **NULL channel dereference** | Missing `if (audio->data[c])` | `grep -B2 -A5 "audio->data\\[" src/` |

### P1 - HIGH (Should Fix)

| Check | Anti-Pattern | Solution |
| :--- | :--- | :--- |
| **Buffer overflow risk** | Ignoring `audio->frames` | Always bound loops to `audio->frames` |
| **Global mutable state** | `static float g_var = ...;` | Encapsulate in plugin instance context struct |
| **Missing error handling** | Unchecked pointer returns | Check for NULL on allocations and lookups |
| **Thread safety issues** | Shared state without atomics | Use `stdatomic.h` or ringbuffers |

### P2 - MEDIUM (Consider Fixing)

| Check | Anti-Pattern | Solution |
| :--- | :--- | :--- |
| **Missing default settings** | No `get_defaults` callback | Implement default property values |
| **Dynamic properties without settings** | Using `get_properties` when controls depend on settings | Migrate to `get_properties2(void *data, obs_data_t *settings)` |
| **Hardcoded strings** | `"My String"` in UI props | Use `obs_module_text("Key")` |
| **Missing localization file** | No `en-US.ini` | Ensure `data/locale/en-US.ini` exists |

### P3 - LOW (Code Quality & Hygiene)

| Check | Anti-Pattern | Solution |
| :--- | :--- | :--- |
| **Missing logging** | No diagnostic output | Use `blog(LOG_INFO, ...)` or `obs_log()` |
| **Inconsistent code style** | Mixed indentation / naming | Run clang-format with OBS formatting rules |

---

## Memory Safety Verification

OBS uses specialized memory management functions rather than standard `malloc`/`free`:

| OBS Function | Standard Equivalent | Memory Characteristic |
| :--- | :--- | :--- |
| `bzalloc(size)` | `calloc(1, size)` | Zero-initialized memory |
| `bmalloc(size)` | `malloc(size)` | Uninitialized memory |
| `brealloc(ptr, size)` | `realloc(ptr, size)` | Resizes memory block |
| `bfree(ptr)` | `free(ptr)` | Frees memory (safe with NULL) |

### Memory Audit Checklist
- [ ] Every `bzalloc`/`bmalloc` in `create` has a corresponding `bfree` in `destroy`.
- [ ] Context struct itself is freed at the very end of `destroy`.
- [ ] NO dynamic memory allocations (`bmalloc`, `bzalloc`, `new`, `malloc`) inside `filter_audio` or video render callbacks.
- [ ] Reference counts (`obs_source_addref` / `obs_source_release`) are strictly paired.

---

## Real-Time Audio Thread Rules

The `filter_audio` callback runs on a high-priority, real-time operating system thread.

```c
/* FORBIDDEN in filter_audio: */
pthread_mutex_lock(&mutex);       /* Can cause priority inversion & audio dropouts */
malloc(1024);                     /* Heap lock & unbounded allocation time */
printf("Processing...\n");        /* System call / I/O blocks the audio clock */
sleep(1);                         /* Audio buffer will starve */

/* REQUIRED in filter_audio: */
for (size_t c = 0; c < channels; c++) {
    if (audio->data[c]) {         /* Check channel exists before indexing */
        /* Process audio frames safely */
    }
}
```

---

## References

- **OBS Crash Debugging**: [https://obsproject.com/docs/debugging.html](https://obsproject.com/docs/debugging.html)
- **libobs Memory API**: [https://docs.obsproject.com/reference-core#memory-management](https://docs.obsproject.com/reference-core#memory-management)
- **Related Skills**: `obs-plugin-developing`, `obs-audio-plugin-writing`
- **Related Agent**: `obs-plugin-expert`
