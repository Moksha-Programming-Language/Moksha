#include "../../include/moksha_rt.h"
#include <stdbool.h>
#include <stdint.h>

extern void *moksha_rt_alloc(size_t payload_size, uint32_t type_id);
extern void moksha_rt_panic(const char *message);
extern void *moksha_mem_alloc(size_t size);
extern void *moksha_mem_realloc(void *ptr, size_t new_size);
extern void moksha_mem_free(void *ptr);

// Internal helper functions for array allocation and manipulation

void *moksha_rt_array_data(MokshaSlice *slice) {
  if (!slice)
    return NULL;
  return slice->data;
}

void *moksha_rt_array_alloc(size_t element_size, uint64_t capacity) {
  MokshaSlice *slice =
      (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), MOKSHA_TYPE_ARRAY);
  MokshaHeader *header = ((MokshaHeader *)slice) - 1;
  header->capacity_bytes = capacity * element_size;

  if (capacity > 0) {
    slice->data = moksha_mem_alloc(header->capacity_bytes);
  } else {
    slice->data = NULL;
  }
  slice->length = 0;
  return slice;
}

static void ensure_capacity(MokshaSlice *slice, uint64_t required_elements,
                            size_t element_size) {
  if (!slice)
    return;
  MokshaHeader *header = ((MokshaHeader *)slice) - 1;
  uint64_t required_bytes = required_elements * element_size;

  if (header->capacity_bytes >= required_bytes)
    return;

  uint64_t new_cap_bytes = header->capacity_bytes == 0
                               ? (4 * element_size)
                               : (header->capacity_bytes * 2);
  if (new_cap_bytes < required_bytes)
    new_cap_bytes = required_bytes;

  if (slice->data && header->capacity_bytes > 0) {
    slice->data = moksha_mem_realloc(slice->data, new_cap_bytes);
  } else {
    void *new_data = moksha_mem_alloc(new_cap_bytes);
    if (slice->data && slice->length > 0) {
      __builtin_memcpy(new_data, slice->data, slice->length * element_size);
    }
    slice->data = new_data;
  }
  if (!slice->data)
    moksha_rt_panic("OOM: Failed to resize array buffer");
  header->capacity_bytes = (uint32_t)new_cap_bytes;
}

void *moksha_rt_array_view(void *stack_data, uint64_t length) {
  MokshaSlice *slice =
      (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), MOKSHA_TYPE_ARRAY);
  MokshaHeader *header = ((MokshaHeader *)slice) - 1;
  header->capacity_bytes = 0;
  slice->data = stack_data;
  slice->length = length;
  return slice;
}

// Implementation of array runtime builtins

int32_t moksha_rt_array_length(MokshaSlice *slice) {
  return slice ? (int32_t)slice->length : 0;
}

int32_t moksha_rt_array_capacity(MokshaSlice *slice) {
  if (!slice)
    return 0;
  return (int32_t)(((MokshaHeader *)slice) - 1)->capacity_bytes;
}

void *moksha_rt_array_at(MokshaSlice *slice, int32_t index,
                         size_t element_size) {
  if (!slice || index < 0 || (uint64_t)index >= slice->length) {
    moksha_rt_panic("Array index out of bounds");
  }
  return (uint8_t *)slice->data + (index * element_size);
}

void __moksha_array_copy(void *dest, void *src, uint32_t bytes) {
  if (dest && src && bytes > 0) {
    __builtin_memcpy(dest, src, bytes);
  }
}

bool __moksha_array_eq(void *a_ptr, int32_t a_len, void *b_ptr, int32_t b_len,
                       int32_t elem_size) {
  if (a_len != b_len)
    return false;
  if (a_len == 0)
    return true;
  if (a_ptr == b_ptr)
    return true;
  if (!a_ptr || !b_ptr)
    return false;
  return __builtin_memcmp(a_ptr, b_ptr, (size_t)a_len * (size_t)elem_size) == 0;
}

bool moksha_rt_array_is_empty(MokshaSlice *slice) {
  return !slice || slice->length == 0;
}

void moksha_rt_array_clear(MokshaSlice *slice) {
  if (slice)
    slice->length = 0;
}

void moksha_rt_array_push(MokshaSlice *slice, void *value_ptr,
                          size_t element_size) {
  ensure_capacity(slice, slice->length + 1, element_size);
  uint8_t *dest = (uint8_t *)slice->data + (slice->length * element_size);
  __builtin_memcpy(dest, value_ptr, element_size);
  slice->length++;
}

void *moksha_rt_array_pop(MokshaSlice *slice, size_t element_size) {
  if (!slice || slice->length == 0)
    moksha_rt_panic("Cannot pop from an empty array");
  slice->length--;
  uint8_t *src = (uint8_t *)slice->data + (slice->length * element_size);
  void *copy = moksha_mem_alloc(element_size);
  __builtin_memcpy(copy, src, element_size);
  return copy;
}

void moksha_rt_array_insert(MokshaSlice *slice, int32_t index, void *value_ptr,
                            size_t element_size) {
  if (!slice || index < 0 || (uint64_t)index > slice->length)
    moksha_rt_panic("Insert index out of bounds");
  ensure_capacity(slice, slice->length + 1, element_size);
  uint8_t *raw_data = (uint8_t *)slice->data;
  size_t bytes_to_move = (slice->length - index) * element_size;
  uint8_t *insert_pos = raw_data + (index * element_size);

  if (bytes_to_move > 0) {
    __builtin_memmove(insert_pos + element_size, insert_pos, bytes_to_move);
  }
  __builtin_memcpy(insert_pos, value_ptr, element_size);
  slice->length++;
}

void *moksha_rt_array_remove(MokshaSlice *slice, int32_t index,
                             size_t element_size) {
  if (!slice || index < 0 || (uint64_t)index >= slice->length)
    moksha_rt_panic("Remove index out of bounds");
  uint8_t *raw_data = (uint8_t *)slice->data;
  uint8_t *remove_pos = raw_data + (index * element_size);
  void *copy = moksha_mem_alloc(element_size);
  __builtin_memcpy(copy, remove_pos, element_size);

  size_t bytes_to_move = (slice->length - index - 1) * element_size;
  if (bytes_to_move > 0) {
    __builtin_memmove(remove_pos, remove_pos + element_size, bytes_to_move);
  }
  slice->length--;
  return copy;
}

void moksha_rt_array_extend(MokshaSlice *dest, MokshaSlice *src,
                            size_t element_size) {
  if (!dest || !src || src->length == 0)
    return;
  ensure_capacity(dest, dest->length + src->length, element_size);
  uint8_t *target = (uint8_t *)dest->data + (dest->length * element_size);
  __builtin_memcpy(target, src->data, src->length * element_size);
  dest->length += src->length;
}

void moksha_rt_array_copy(MokshaSlice *dest, MokshaSlice *src,
                          size_t element_size) {
  if (!dest || !src || dest->length == 0 || src->length == 0)
    return;
  uint64_t copy_len = dest->length < src->length ? dest->length : src->length;
  __builtin_memcpy(dest->data, src->data, copy_len * element_size);
}

void *moksha_rt_array_clone(MokshaSlice *src, size_t element_size) {
  if (!src)
    return NULL;
  MokshaSlice *new_slice =
      (MokshaSlice *)moksha_rt_array_alloc(element_size, src->length);
  new_slice->length = src->length;
  if (src->length > 0) {
    __builtin_memcpy(new_slice->data, src->data, src->length * element_size);
  }
  return new_slice;
}

void *moksha_rt_array_slice(MokshaSlice *slice, int32_t start, int32_t end,
                            size_t element_size) {
  if (!slice)
    return NULL;
  if (start < 0)
    start = 0;
  if ((uint64_t)end > slice->length)
    end = slice->length;
  if (start >= end)
    return moksha_rt_array_view(NULL, 0);

  uint8_t *offset_ptr = (uint8_t *)slice->data + (start * element_size);
  return moksha_rt_array_view(offset_ptr, end - start);
}

void moksha_rt_array_sort(MokshaSlice *slice, size_t element_size) {
  if (!slice || slice->length <= 1)
    return;
  uint8_t *base = (uint8_t *)slice->data;
  uint8_t *temp = (uint8_t *)moksha_mem_alloc(element_size);

  for (uint64_t i = 1; i < slice->length; i++) {
    __builtin_memcpy(temp, base + (i * element_size), element_size);
    int64_t j = (int64_t)i - 1;
    while (j >= 0 && __builtin_memcmp(base + (j * element_size), temp,
                                      element_size) > 0) {
      __builtin_memcpy(base + ((j + 1) * element_size),
                       base + (j * element_size), element_size);
      j--;
    }
    __builtin_memcpy(base + ((j + 1) * element_size), temp, element_size);
  }
  moksha_mem_free(temp);
}

void moksha_rt_array_resize(MokshaSlice *slice, int32_t new_length,
                            size_t element_size) {
  if (!slice || new_length < 0)
    return;
  uint64_t old_len = slice->length;
  if ((uint64_t)new_length > old_len) {
    ensure_capacity(slice, new_length, element_size);
    uint8_t *start_ptr = (uint8_t *)slice->data + (old_len * element_size);
    __builtin_memset(start_ptr, 0, (new_length - old_len) * element_size);
  }
  slice->length = new_length;
}

bool moksha_rt_array_contains(MokshaSlice *slice, void *element,
                              size_t element_size) {
  if (!slice || !slice->data || slice->length == 0)
    return false;
  uint8_t *raw = (uint8_t *)slice->data;
  for (uint64_t i = 0; i < slice->length; i++) {
    if (__builtin_memcmp(raw + (i * element_size), element, element_size) == 0)
      return true;
  }
  return false;
}

int32_t moksha_rt_array_index(MokshaSlice *slice, void *element,
                              size_t element_size) {
  if (!slice || !slice->data || slice->length == 0)
    return -1;
  uint8_t *raw = (uint8_t *)slice->data;
  for (uint64_t i = 0; i < slice->length; i++) {
    if (__builtin_memcmp(raw + (i * element_size), element, element_size) == 0)
      return (int32_t)i;
  }
  return -1;
}

void moksha_rt_array_reverse(MokshaSlice *slice, size_t element_size) {
  if (!slice || !slice->data || slice->length <= 1)
    return;
  uint8_t *raw = (uint8_t *)slice->data;
  uint8_t *temp = (uint8_t *)moksha_mem_alloc(element_size);
  uint64_t left = 0;
  uint64_t right = slice->length - 1;

  while (left < right) {
    __builtin_memcpy(temp, raw + (left * element_size), element_size);
    __builtin_memcpy(raw + (left * element_size), raw + (right * element_size),
                     element_size);
    __builtin_memcpy(raw + (right * element_size), temp, element_size);
    left++;
    right--;
  }
  moksha_mem_free(temp);
}

void moksha_rt_array_fill(MokshaSlice *slice, void *value_ptr,
                          size_t element_size) {
  if (!slice || !slice->data || slice->length == 0)
    return;
  uint8_t *raw = (uint8_t *)slice->data;
  for (uint64_t i = 0; i < slice->length; i++) {
    __builtin_memcpy(raw + (i * element_size), value_ptr, element_size);
  }
}
