#include "../../include/moksha_rt.h"
#include "../abi/sys_caps.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* @brief Uncomment the debugger statements when needed */

extern void moksha_rt_panic(const char *message);
extern void *moksha_mem_alloc(size_t size);
extern void moksha_mem_free(void *ptr);

// static volatile long _moksha_live_objects = 0;
// static volatile long _moksha_alloc_ops = 0;

// // --- TRACKER IMPLEMENTATION (Moved outside the baremetal block) ---
// typedef struct TrackerNode {
//   void *payload;
//   uint32_t type_id;
//   struct TrackerNode *next;
//   struct TrackerNode *prev;
// } TrackerNode;

// static TrackerNode *tracker_head = NULL;
// static int tracker_lock = 0;

// static void tracker_add(void *payload, uint32_t type_id) {
//   TrackerNode *node = (TrackerNode *)moksha_mem_alloc(sizeof(TrackerNode));
//   node->payload = payload;
//   node->type_id = type_id;
//   node->prev = NULL;

//   while (__atomic_exchange_n(&tracker_lock, 1, __ATOMIC_ACQUIRE)) {
//     cpu_relax();
//   }
//   node->next = tracker_head;
//   if (tracker_head)
//     tracker_head->prev = node;
//   tracker_head = node;
//   __atomic_store_n(&tracker_lock, 0, __ATOMIC_RELEASE);
// }

// static void tracker_remove(void *payload) {
//   while (__atomic_exchange_n(&tracker_lock, 1, __ATOMIC_ACQUIRE)) {
//     cpu_relax();
//   }
//   TrackerNode *curr = tracker_head;
//   while (curr) {
//     if (curr->payload == payload) {
//       if (curr->prev)
//         curr->prev->next = curr->next;
//       else
//         tracker_head = curr->next;
//       if (curr->next)
//         curr->next->prev = curr->prev;
//       moksha_mem_free(curr);
//       break;
//     }
//     curr = curr->next;
//   }
//   __atomic_store_n(&tracker_lock, 0, __ATOMIC_RELEASE);
// }

// // Call this function at the very end of main() to dump all leaks
// void moksha_rt_dump_leaks(void) {
//   while (__atomic_exchange_n(&tracker_lock, 1, __ATOMIC_ACQUIRE)) {
//     cpu_relax();
//   }
//   printf("\n--- MOKSHA ARC LEAK DUMP ---\n");
//   TrackerNode *curr = tracker_head;
//   int count = 0;
//   while (curr) {
//     printf("Leak %d: Payload Addr: %p | Type ID: %u\n", ++count,
//     curr->payload,
//            curr->type_id);
//     curr = curr->next;
//   }
//   printf("Total Leaked Objects: %d\n----------------------------\n", count);
//   __atomic_store_n(&tracker_lock, 0, __ATOMIC_RELEASE);
// }
// // ------------------------------------------------------------------

#if defined(__MOKSHA_BAREMETAL__)
extern char _sstack[];
extern char _estack[];

bool is_stack_ptr(void *ptr) {
  char *p = (char *)ptr;
  return (p >= _sstack && p <= _estack);
}
#else
bool is_stack_ptr(void *ptr) {
  if (!ptr)
    return false;
  int local_var;
  void *stack_frame = (void *)&local_var;
  size_t p = (size_t)ptr;
  size_t s = (size_t)stack_frame;
  size_t diff = (p > s) ? (p - s) : (s - p);
  return diff < 8 * 1024 * 1024;
}
#endif

void *moksha_rt_alloc(size_t payload_size, uint32_t type_id) {
  MokshaHeader *header =
      (MokshaHeader *)moksha_mem_alloc(sizeof(MokshaHeader) + payload_size);
  if (!header)
    moksha_rt_panic("OOM during ARC allocation");

  header->ref_count = 1;
  header->weak_count = 1;
  header->type_id = type_id;
  header->capacity_bytes = (uint32_t)payload_size;

  void *payload = (void *)(header + 1);
  __builtin_memset(payload, 0, payload_size);

  // __atomic_add_fetch(&_moksha_live_objects, 1, __ATOMIC_RELAXED);
  // long ops = __atomic_add_fetch(&_moksha_alloc_ops, 1, __ATOMIC_RELAXED);
  // if (ops % 1000000 == 0) {
  //   printf("[ARC DEBUG] Live Heap Objects: %ld\n", _moksha_live_objects);
  // }
  // tracker_add(payload, type_id);

  return payload;
}

void moksha_rt_retain(void *ptr) {
  if (!ptr || is_stack_ptr(ptr))
    return;

  if (is_stack_ptr(ptr))
    return;

  MokshaHeader *header = ((MokshaHeader *)ptr) - 1;

  if (sys_get_caps()->has_threads) {
    __atomic_add_fetch(&header->ref_count, 1, __ATOMIC_RELAXED);
  } else {
    header->ref_count += 1;
  }
}

void moksha_rt_release_closure_env(void *env_ptr) {
  if (!env_ptr)
    return;

  void (**dtor_slot)(void *) = (void (**)(void *))env_ptr;
  void (*env_dtor)(void *) = *dtor_slot;

  if (env_dtor) {
    env_dtor(env_ptr);
    *dtor_slot = NULL;
  }

  if (is_stack_ptr(env_ptr))
    return;

  MokshaHeader *header = ((MokshaHeader *)env_ptr) - 1;
  uint32_t new_strong;
  if (sys_get_caps()->has_threads) {
    new_strong = __atomic_sub_fetch(&header->ref_count, 1, __ATOMIC_ACQ_REL);
  } else {
    if (header->ref_count == 0)
      moksha_rt_panic("ARC double free!");
    new_strong = --header->ref_count;
  }

  if (new_strong == 0) {
    uint32_t new_weak;
    if (sys_get_caps()->has_threads) {
      new_weak = __atomic_sub_fetch(&header->weak_count, 1, __ATOMIC_ACQ_REL);
    } else {
      new_weak = --header->weak_count;
    }

    if (new_weak == 0) {
      // __atomic_sub_fetch(&_moksha_live_objects, 1, __ATOMIC_RELAXED);
      // tracker_remove(env_ptr);
      moksha_mem_free(header);
    }
  }
}

void moksha_rt_execute_closure_dtor_only(void *env_ptr) {
  if (!env_ptr)
    return;

  void (**dtor_slot)(void *) = (void (**)(void *))env_ptr;
  void (*env_dtor)(void *) = *dtor_slot;

  if (env_dtor) {
    env_dtor(env_ptr);
    *dtor_slot = NULL;
  }
}

void moksha_rt_release_with_dtor(void *ptr, void (*dtor)(void *)) {
  if (!ptr)
    return;

  if (is_stack_ptr(ptr)) {
    if (dtor)
      dtor(ptr);
    return;
  }

  MokshaHeader *header = ((MokshaHeader *)ptr) - 1;
  uint32_t new_strong;

  if (sys_get_caps()->has_threads) {
    new_strong = __atomic_sub_fetch(&header->ref_count, 1, __ATOMIC_ACQ_REL);
  } else {
    if (header->ref_count == 0)
      moksha_rt_panic("ARC double free!");
    new_strong = --header->ref_count;
  }

  if (new_strong == (uint32_t)-1)
    moksha_rt_panic("ARC underflow!");

  if (new_strong == 0) {
    if (dtor) {
      dtor(ptr);
    }

    if (header->type_id == MOKSHA_TYPE_ARRAY) {
      MokshaSlice *slice = (MokshaSlice *)ptr;
      if (slice->data && header->capacity_bytes > 0) {
        moksha_mem_free(slice->data);
        slice->data = NULL;
      }
    } else if (!dtor) {
      if (header->type_id == MOKSHA_TYPE_PROMISE) {
        typedef struct {
          void *coro_handle;
          bool is_completed;
          void *result_data;
          void *waiting_coro;
          bool is_rejected;
          bool was_awaited;
        } PromiseLayout;

        PromiseLayout *prom = (PromiseLayout *)ptr;
        if (prom->is_rejected && !prom->was_awaited) {
          moksha_rt_panic(
              "Unhandled Promise Rejection: An async function threw "
              "an exception that was never awaited!");
        }
      } else if (header->type_id == MOKSHA_TYPE_TABLE) {
        extern void moksha_rt_map_free_internal(void *map_ptr);
        moksha_rt_map_free_internal(ptr);
      } else if (header->type_id == MOKSHA_TYPE_CLOSURE) {
        MokshaClosure *closure = (MokshaClosure *)ptr;
        if (closure->environment_ptr) {
          moksha_rt_release_closure_env(closure->environment_ptr);
        }
      }
    }

    uint32_t new_weak;
    if (sys_get_caps()->has_threads) {
      new_weak = __atomic_sub_fetch(&header->weak_count, 1, __ATOMIC_ACQ_REL);
    } else {
      new_weak = --header->weak_count;
    }

    if (new_weak == 0) {
      // __atomic_sub_fetch(&_moksha_live_objects, 1, __ATOMIC_RELAXED);
      // tracker_remove(ptr);
      moksha_mem_free(header);
    }
  }
}

void moksha_rt_release(void *ptr) { moksha_rt_release_with_dtor(ptr, NULL); }

void *__moksha_alloc(uint32_t size, uint32_t type_id) {
  return moksha_rt_alloc((size_t)size, type_id);
}

void __moksha_free(void *ptr) { moksha_rt_release(ptr); }

void moksha_rt_store_weak(void **dest, void *obj) {
  if (!dest)
    return;
  if (obj) {
    MokshaHeader *new_header = ((MokshaHeader *)obj) - 1;
    if (sys_get_caps()->has_threads) {
      __atomic_add_fetch(&new_header->weak_count, 1, __ATOMIC_RELAXED);
    } else {
      new_header->weak_count++;
    }
  }
  void *old_obj;
  if (sys_get_caps()->has_threads) {
    old_obj = __atomic_exchange_n(dest, obj, __ATOMIC_SEQ_CST);
  } else {
    old_obj = *dest;
    *dest = obj;
  }
  if (old_obj) {
    MokshaHeader *old_header = ((MokshaHeader *)old_obj) - 1;
    uint32_t remaining_weak;
    if (sys_get_caps()->has_threads) {
      remaining_weak =
          __atomic_sub_fetch(&old_header->weak_count, 1, __ATOMIC_ACQ_REL);
    } else {
      remaining_weak = --old_header->weak_count;
    }
    if (remaining_weak == 0) {
      // __atomic_sub_fetch(&_moksha_live_objects, 1, __ATOMIC_RELAXED);
      // tracker_remove(old_obj);
      moksha_mem_free(old_header);
    }
  }
}

void *moksha_rt_load_weak(void **src) {
  if (!src)
    return NULL;
  while (true) {
    void *obj;
    if (sys_get_caps()->has_threads) {
      obj = __atomic_load_n(src, __ATOMIC_SEQ_CST);
    } else {
      obj = *src;
    }
    if (!obj)
      return NULL;
    MokshaHeader *header = ((MokshaHeader *)obj) - 1;
    uint32_t count;
    if (sys_get_caps()->has_threads) {
      count = __atomic_load_n(&header->ref_count, __ATOMIC_RELAXED);
    } else {
      count = header->ref_count;
    }
    if (count == 0)
      return NULL;
    if (sys_get_caps()->has_threads) {
      if (__atomic_compare_exchange_n(&header->ref_count, &count, count + 1,
                                      false, __ATOMIC_ACQ_REL,
                                      __ATOMIC_RELAXED)) {
        return obj;
      }
    } else {
      header->ref_count++;
      return obj;
    }
  }
}

int32_t __moksha_get_type(void *ptr) {
  if (!ptr)
    return 19;
  MokshaHeader *header = ((MokshaHeader *)ptr) - 1;
  return (int32_t)header->type_id;
}
