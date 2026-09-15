#include "../../include/moksha_rt.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

extern void *moksha_mem_alloc(size_t size);
extern void *moksha_rt_alloc(size_t payload_size, uint32_t type_id);
extern void moksha_rt_panic(const char *message);
extern void moksha_rt_retain(void *ptr);
extern void moksha_rt_release(void *ptr);
extern void moksha_mem_free(void *ptr);
extern void moksha_rt_release_with_dtor(void *ptr, void (*dtor)(void *));

// Map Structure

#define MAP_INITIAL_CAPACITY 16

typedef struct MapEntry {
  MokshaAny key;
  MokshaAny value;
  struct MapEntry *next;
  struct MapEntry *order_next;
  struct MapEntry *order_prev;
} MapEntry;

typedef struct {
  MapEntry **buckets;
  uint32_t capacity;
  uint32_t size;
  MapEntry *head;
  MapEntry *tail;
  uint32_t iter_cache_idx;
  MapEntry *iter_cache_node;
} MokshaMap;

// Internal map utilities

static int internal_strcmp(const char *s1, const char *s2) {
  return __builtin_strcmp(s1, s2);
}

static uint32_t get_any_type(MokshaAny *key) {
  if (!key || !key->data)
    return 0;
  if (key->vtable)
    return key->vtable->type_id;
  MokshaHeader *hdr =
      (MokshaHeader *)((uint8_t *)key->data - sizeof(MokshaHeader));
  return hdr->type_id;
}

static void consume_any_into(MokshaAny *dest, MokshaAny *src) {
  dest->vtable = src->vtable;
  uint32_t type = get_any_type(src);

  if (type == MOKSHA_TYPE_I32 || type == MOKSHA_TYPE_U32 ||
      type == MOKSHA_TYPE_F32 || type == MOKSHA_TYPE_BOOL) {
    dest->data = moksha_rt_alloc(4, type);
    *(uint32_t *)dest->data = *(uint32_t *)src->data;
  } else if (type == MOKSHA_TYPE_I64 || type == MOKSHA_TYPE_U64 ||
             type == MOKSHA_TYPE_ISIZE || type == MOKSHA_TYPE_USIZE ||
             type == MOKSHA_TYPE_F64) {
    dest->data = moksha_rt_alloc(8, type);
    *(uint64_t *)dest->data = *(uint64_t *)src->data;
  } else {
    dest->data = src->data;
  }
}

static uint32_t hash_any(MokshaAny *key) {
  if (!key || !key->data)
    return 0;
  uint32_t type_id = get_any_type(key);
  uint32_t hash = 2166136261u;

  if (type_id == MOKSHA_TYPE_STRING) {
    const char *str = (const char *)key->data;
    while (*str) {
      hash ^= (uint8_t)(*str);
      hash *= 16777619;
      str++;
    }
    return hash;
  }
  if (type_id == MOKSHA_TYPE_I32 || type_id == MOKSHA_TYPE_U32 ||
      type_id == MOKSHA_TYPE_F32) {
    uint32_t val = *(uint32_t *)key->data;
    hash ^= val;
    hash *= 16777619;
    return hash;
  }
  if (type_id == MOKSHA_TYPE_I64 || type_id == MOKSHA_TYPE_U64 ||
      type_id == MOKSHA_TYPE_ISIZE || type_id == MOKSHA_TYPE_USIZE ||
      type_id == MOKSHA_TYPE_F64) {
    uint64_t val = *(uint64_t *)key->data;
    hash ^= (uint32_t)(val & 0xFFFFFFFF);
    hash *= 16777619;
    hash ^= (uint32_t)(val >> 32);
    hash *= 16777619;
    return hash;
  }
  uint64_t addr = (uint64_t)(uintptr_t)key->data;
  hash ^= (uint32_t)(addr & 0xFFFFFFFF);
  hash *= 16777619;
  hash ^= (uint32_t)(addr >> 32);
  hash *= 16777619;
  return hash;
}

static bool cmp_any(MokshaAny *a, MokshaAny *b) {
  if (!a || !b)
    return false;
  if (a->data == b->data)
    return true;
  if (!a->data || !b->data)
    return false;

  uint32_t type_a = get_any_type(a);
  uint32_t type_b = get_any_type(b);
  if (type_a != type_b)
    return false;

  if (type_a == MOKSHA_TYPE_STRING) {
    return internal_strcmp((const char *)a->data, (const char *)b->data) == 0;
  }
  if (type_a == MOKSHA_TYPE_I32 || type_a == MOKSHA_TYPE_U32 ||
      type_a == MOKSHA_TYPE_F32) {
    return *(uint32_t *)a->data == *(uint32_t *)b->data;
  }
  if (type_a == MOKSHA_TYPE_I64 || type_a == MOKSHA_TYPE_U64 ||
      type_a == MOKSHA_TYPE_ISIZE || type_a == MOKSHA_TYPE_USIZE ||
      type_a == MOKSHA_TYPE_F64) {
    return *(uint64_t *)a->data == *(uint64_t *)b->data;
  }
  return false;
}

static bool map_keys_equal(MokshaAny *k1, MokshaAny *k2) {
  return cmp_any(k1, k2);
}

void *moksha_rt_map_new(void) {
  MokshaMap *map =
      (MokshaMap *)moksha_rt_alloc(sizeof(MokshaMap), MOKSHA_TYPE_TABLE);
  if (!map)
    moksha_rt_panic("OOM: Failed to allocate Map");

  map->capacity = MAP_INITIAL_CAPACITY;
  map->size = 0;
  map->head = NULL;
  map->tail = NULL;
  map->iter_cache_node = NULL;
  map->buckets =
      (MapEntry **)moksha_mem_alloc(sizeof(MapEntry *) * MAP_INITIAL_CAPACITY);

  __builtin_memset(map->buckets, 0, sizeof(MapEntry *) * MAP_INITIAL_CAPACITY);
  return map;
}

void moksha_rt_map_insert(void *map_ptr, MokshaAny *key, MokshaAny *value) {
  if (!map_ptr || !key || !key->data)
    return;
  MokshaMap *map = (MokshaMap *)map_ptr;

  uint32_t index = hash_any(key) % map->capacity;
  MapEntry *entry = map->buckets[index];

  while (entry) {
    if (cmp_any(&entry->key, key)) {
      void (*val_drop)(void *) =
          entry->value.vtable ? entry->value.vtable->drop : NULL;
      moksha_rt_release_with_dtor(entry->value.data, val_drop);
      consume_any_into(&entry->value, value);
      void (*key_drop)(void *) = key->vtable ? key->vtable->drop : NULL;
      moksha_rt_release_with_dtor(key->data, key_drop);
      return;
    }
    entry = entry->next;
  }

  MapEntry *new_entry = (MapEntry *)moksha_mem_alloc(sizeof(MapEntry));
  consume_any_into(&new_entry->key, key);
  consume_any_into(&new_entry->value, value);

  new_entry->next = map->buckets[index];
  new_entry->order_next = NULL;
  new_entry->order_prev = map->tail;

  map->buckets[index] = new_entry;
  map->size++;

  if (map->tail)
    map->tail->order_next = new_entry;
  else
    map->head = new_entry;
  map->tail = new_entry;
}

MokshaAny *moksha_rt_map_get(void *map_ptr, MokshaAny *key) {
  if (!map_ptr || !key || !key->data)
    return NULL;
  MokshaMap *map = (MokshaMap *)map_ptr;
  uint32_t index = hash_any(key) % map->capacity;

  MapEntry *entry = map->buckets[index];
  while (entry) {
    if (cmp_any(&entry->key, key))
      return &entry->value;
    entry = entry->next;
  }
  return NULL;
}

static MapEntry *get_entry_at(MokshaMap *map, int32_t index) {
  if (index < 0 || (uint32_t)index >= map->size)
    return NULL;

  if (map->iter_cache_node) {
    if ((uint32_t)index == map->iter_cache_idx + 1 &&
        map->iter_cache_node->order_next) {
      map->iter_cache_idx++;
      map->iter_cache_node = map->iter_cache_node->order_next;
      return map->iter_cache_node;
    }
    if ((uint32_t)index == map->iter_cache_idx) {
      return map->iter_cache_node;
    }
  }

  int32_t count = 0;
  MapEntry *curr = map->head;
  while (curr) {
    if (count == index) {
      map->iter_cache_idx = index;
      map->iter_cache_node = curr;
      return curr;
    }
    count++;
    curr = curr->order_next;
  }
  return NULL;
}

MokshaAny *moksha_rt_map_get_key_at(void *map_ptr, int32_t index) {
  MapEntry *entry = get_entry_at((MokshaMap *)map_ptr, index);
  return entry ? &entry->key : NULL;
}

MokshaAny *moksha_rt_map_get_val_at(void *map_ptr, int32_t index) {
  MapEntry *entry = get_entry_at((MokshaMap *)map_ptr, index);
  return entry ? &entry->value : NULL;
}

void *moksha_rt_map_get_val_ptr_at(void *map_ptr, int32_t index) {
  MapEntry *entry = get_entry_at((MokshaMap *)map_ptr, index);
  return entry ? entry->value.data : NULL;
}

void moksha_rt_map_free_internal(void *map_ptr) {
  if (!map_ptr)
    return;
  MokshaMap *map = (MokshaMap *)map_ptr;

  for (uint32_t i = 0; i < map->capacity; i++) {
    MapEntry *entry = map->buckets[i];
    while (entry) {
      MapEntry *next = entry->next;
      void (*key_drop)(void *) =
          entry->key.vtable ? entry->key.vtable->drop : NULL;
      moksha_rt_release_with_dtor(entry->key.data, key_drop);

      void (*val_drop)(void *) =
          entry->value.vtable ? entry->value.vtable->drop : NULL;
      moksha_rt_release_with_dtor(entry->value.data, val_drop);

      moksha_mem_free(entry);
      entry = next;
    }
  }
  if (map->buckets)
    moksha_mem_free(map->buckets);
}

int32_t moksha_rt_map_len(void *map_ptr) {
  if (!map_ptr)
    return 0;
  return (int32_t)((MokshaMap *)map_ptr)->size;
}

bool moksha_rt_map_has(void *map_ptr, MokshaAny *key) {
  return moksha_rt_map_get(map_ptr, key) != NULL;
}

int32_t moksha_rt_map_length(void *map_ptr) {
  return moksha_rt_map_len(map_ptr);
}

// Implementation of Map runtime builtins

void moksha_rt_map_clear(void *map_ptr) {
  if (!map_ptr)
    return;
  MokshaMap *map = (MokshaMap *)map_ptr;
  MapEntry *curr = map->head;
  while (curr) {
    MapEntry *next = curr->order_next;
    void (*key_drop)(void *) = curr->key.vtable ? curr->key.vtable->drop : NULL;
    moksha_rt_release_with_dtor(curr->key.data, key_drop);

    void (*val_drop)(void *) =
        curr->value.vtable ? curr->value.vtable->drop : NULL;
    moksha_rt_release_with_dtor(curr->value.data, val_drop);

    moksha_mem_free(curr);
    curr = next;
  }
  __builtin_memset(map->buckets, 0, sizeof(MapEntry *) * map->capacity);
  map->head = NULL;
  map->tail = NULL;
  map->size = 0;
  map->iter_cache_node = NULL;
}

void moksha_rt_map_remove(void *map_ptr, MokshaAny *key) {
  if (!map_ptr || !key)
    return;
  MokshaMap *map = (MokshaMap *)map_ptr;

  uint32_t index = hash_any(key) % map->capacity;
  MapEntry *b_curr = map->buckets[index];
  MapEntry *b_prev = NULL;
  MapEntry *target = NULL;

  while (b_curr) {
    if (map_keys_equal(&b_curr->key, key)) {
      target = b_curr;
      if (b_prev)
        b_prev->next = b_curr->next;
      else
        map->buckets[index] = b_curr->next;
      break;
    }
    b_prev = b_curr;
    b_curr = b_curr->next;
  }

  if (!target)
    return;

  if (target->order_prev)
    target->order_prev->order_next = target->order_next;
  else
    map->head = target->order_next;

  if (target->order_next)
    target->order_next->order_prev = target->order_prev;
  else
    map->tail = target->order_prev;

  map->iter_cache_node = NULL;
  void (*key_drop)(void *) =
      target->key.vtable ? target->key.vtable->drop : NULL;
  moksha_rt_release_with_dtor(target->key.data, key_drop);

  void (*val_drop)(void *) =
      target->value.vtable ? target->value.vtable->drop : NULL;
  moksha_rt_release_with_dtor(target->value.data, val_drop);

  map->size--;
  moksha_mem_free(target);
}

void *moksha_rt_map_get_or_create(void *map_ptr, MokshaAny *key,
                                  size_t val_size, uint32_t val_type_id) {
  if (!map_ptr || !key || !key->data)
    return NULL;
  MokshaMap *map = (MokshaMap *)map_ptr;
  uint32_t index = hash_any(key) % map->capacity;

  MapEntry *entry = map->buckets[index];
  while (entry) {
    if (cmp_any(&entry->key, key)) {
      void (*key_drop)(void *) = key->vtable ? key->vtable->drop : NULL;
      moksha_rt_release_with_dtor(key->data, key_drop);
      return entry->value.data;
    }
    entry = entry->next;
  }

  void *val_payload = moksha_rt_alloc(val_size, val_type_id);

  MapEntry *new_entry = (MapEntry *)moksha_mem_alloc(sizeof(MapEntry));
  consume_any_into(&new_entry->key, key);
  new_entry->value.data = val_payload;
  new_entry->value.vtable = NULL;
  new_entry->next = map->buckets[index];
  new_entry->order_next = NULL;
  new_entry->order_prev = map->tail;

  map->buckets[index] = new_entry;
  map->size++;

  if (map->tail)
    map->tail->order_next = new_entry;
  else
    map->head = new_entry;
  map->tail = new_entry;

  return val_payload;
}

MokshaAny *moksha_rt_any_get(MokshaAny *container, MokshaAny *key) {
  if (!container || !key || !container->data)
    return NULL;
  uint32_t type_id = get_any_type(container);

  if (type_id == MOKSHA_TYPE_TABLE) {
    return moksha_rt_map_get(container->data, key);
  } else if (type_id == MOKSHA_TYPE_ARRAY) {
    uint32_t key_type = get_any_type(key);
    int64_t index = 0;
    if (key_type == MOKSHA_TYPE_I32 || key_type == MOKSHA_TYPE_U32) {
      index = *(int32_t *)key->data;
    } else if (key_type == MOKSHA_TYPE_I64 || key_type == MOKSHA_TYPE_U64 ||
               key_type == MOKSHA_TYPE_ISIZE || key_type == MOKSHA_TYPE_USIZE) {
      index = *(int64_t *)key->data;
    } else {
      moksha_rt_panic("Type Error: Array index must be an integer.");
    }
    MokshaSlice *slice = (MokshaSlice *)container->data;
    if (index < 0 || (uint64_t)index >= slice->length) {
      moksha_rt_panic("Array out of bounds");
    }
    MokshaAny *arr = (MokshaAny *)slice->data;
    return &arr[index];
  }
  moksha_rt_panic("Type Error: Cannot index into a non-collection 'any' type.");
  return NULL;
}
