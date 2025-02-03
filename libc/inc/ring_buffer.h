/// @file ring_buffer.h
/// @brief This file provides a macro to declare a ring buffer with either fixed or
/// dynamic size, and also single or two-dimensional ring buffers.
/// @details
/// The ring buffer supports standard queue operations like pushing to the back
/// or front, popping from the front or back, and peeking at elements without
/// removing them. Additionally, it allows for customization through a
/// user-defined copy function, enabling flexible handling of how data is copied
/// between buffer entries.
///
/// Each entry in the buffer consists of a one-dimensional array (second
/// dimension), and the buffer is designed to manage multiple such entries
/// (first dimension). A copy function can be provided at initialization,
/// allowing for custom behaviors during data insertion, such as deep copies or
/// specialized copying logic. This abstraction is useful for scenarios
/// requiring efficient circular buffer behavior while handling structured or
/// complex data types.
///
/// The macro creates several key functions, including:
/// - Initialization of the ring buffer with a custom copy function.
/// - Push operations to add elements to the front or back of the buffer.
/// - Pop operations to remove elements from the front or back of the buffer.
/// - Peek operations to inspect elements without removal.
/// - Utility functions like checking whether the buffer is empty or full.
///
/// Usage example for a 1D ring buffer:
/// ```c
/// #define DECLARE_FIXED_SIZE_RING_BUFFER(type, name, size, init_value)
/// DECLARE_FIXED_SIZE_RING_BUFFER(int, 1d_buffer, 10, 0);
///
/// rb_1d_buffer_t buffer;
/// rb_1d_buffer_init(&buffer);
///
/// // Push values to the buffer
/// int value = 5;
/// rb_1d_buffer_push_back(&buffer, value);
///
/// // Pop a value from the front
/// int popped_value;
/// rb_1d_buffer_pop_front(&buffer, &popped_value);
/// ```
///
/// Usage example for a 2D ring buffer:
/// ```c
/// #define DECLARE_FIXED_SIZE_2D_RING_BUFFER(type, name, size1, size2, init_value, copy_func)
/// DECLARE_FIXED_SIZE_2D_RING_BUFFER(int, 2d_buffer, 10, 5, 0, NULL);
///
/// rb_2d_buffer_t buffer;
/// rb_2d_buffer_init(&buffer);
///
/// // Create an entry to push to the buffer
/// rb_2d_buffer_arr_t entry;
/// entry.size = 5;  // Set the size for the second dimension
/// for (unsigned i = 0; i < entry.size; i++) {
///     entry.buffer[i] = i;  // Fill the entry buffer
/// }
///
/// // Push the entry to the buffer
/// rb_2d_buffer_push_back(&buffer, &entry);
///
/// // Pop an entry from the front
/// rb_2d_buffer_pop_front(&buffer, &entry);
/// ```
///
/// This structure is suitable for applications that require a circular buffer for
/// multi-dimensional data, such as managing a history of states, logs, or streaming
/// data where array-based data must be efficiently stored and retrieved.
///
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Declares a fixed-size ring buffer.
#define DECLARE_FIXED_SIZE_RING_BUFFER(type, name, length, init)                                                       \
    typedef struct {                                                                                                   \
        type buffer[length];                                                                                           \
        unsigned size;                                                                                                 \
        unsigned head;                                                                                                 \
        unsigned tail;                                                                                                 \
        unsigned count;                                                                                                \
    } rb_##name##_t;                                                                                                   \
                                                                                                                       \
    static inline void rb_##name##_init(rb_##name##_t *rb)                                                             \
    {                                                                                                                  \
        rb->head  = 0;                                                                                                 \
        rb->tail  = 0;                                                                                                 \
        rb->count = 0;                                                                                                 \
        rb->size  = length;                                                                                            \
        for (unsigned i = 0; i < length; i++) {                                                                        \
            rb->buffer[i] = init;                                                                                      \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static inline unsigned rb_##name##_is_empty(rb_##name##_t *rb) { return rb->count == 0; }                          \
                                                                                                                       \
    static inline unsigned rb_##name##_is_full(rb_##name##_t *rb) { return rb->count == rb->size; }                    \
                                                                                                                       \
    static inline void rb_##name##_push_back(rb_##name##_t *rb, type item)                                             \
    {                                                                                                                  \
        rb->buffer[rb->head] = item;                                                                                   \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->tail = (rb->tail + 1) % rb->size;                                                                      \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->head = (rb->head + 1) % rb->size;                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_push_front(rb_##name##_t *rb, type item)                                            \
    {                                                                                                                  \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->head = (rb->head == 0) ? rb->size - 1 : rb->head - 1;                                                  \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->tail             = (rb->tail == 0) ? rb->size - 1 : rb->tail - 1;                                          \
        rb->buffer[rb->tail] = item;                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_pop_front(rb_##name##_t *rb)                                                        \
    {                                                                                                                  \
        type item = init;                                                                                              \
        if (!rb_##name##_is_empty(rb)) {                                                                               \
            item                 = rb->buffer[rb->tail];                                                               \
            rb->buffer[rb->tail] = init;                                                                               \
            rb->tail             = (rb->tail + 1) % rb->size;                                                          \
            rb->count--;                                                                                               \
        }                                                                                                              \
        return item;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_pop_back(rb_##name##_t *rb)                                                         \
    {                                                                                                                  \
        type item = init;                                                                                              \
        if (!rb_##name##_is_empty(rb)) {                                                                               \
            rb->head             = (rb->head == 0) ? rb->size - 1 : rb->head - 1;                                      \
            item                 = rb->buffer[rb->head];                                                               \
            rb->buffer[rb->head] = init;                                                                               \
            rb->count--;                                                                                               \
        }                                                                                                              \
        return item;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_peek_front(rb_##name##_t *rb)                                                       \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        return rb->buffer[rb->tail];                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_peek_back(rb_##name##_t *rb)                                                        \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        return rb->buffer[(rb->head == 0) ? rb->size - 1 : rb->head - 1];                                              \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_get(rb_##name##_t *rb, unsigned position)                                           \
    {                                                                                                                  \
        type item = init;                                                                                              \
        if (!rb_##name##_is_empty(rb) && (position < rb->size)) {                                                      \
            item = rb->buffer[(rb->tail + position) % rb->size];                                                       \
        }                                                                                                              \
        return item;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_iterate(rb_##name##_t *rb, void (*callback)(type))                                  \
    {                                                                                                                  \
        for (unsigned i = 0; i < rb->count; i++) {                                                                     \
            callback(rb_##name##_get(rb, i));                                                                          \
        }                                                                                                              \
    }

/// @brief Declares a dynamic-size ring-buffer.
#define DECLARE_DYNAMIC_SIZE_RING_BUFFER(type, name, init)                                                             \
    typedef struct {                                                                                                   \
        type *buffer;                                                                                                  \
        unsigned size;                                                                                                 \
        unsigned head;                                                                                                 \
        unsigned tail;                                                                                                 \
        unsigned count;                                                                                                \
    } rb_##name##_t;                                                                                                   \
                                                                                                                       \
    static inline int rb_##name##_init(rb_##name##_t *rb, unsigned length, void *(*alloc_func)(size_t))                \
    {                                                                                                                  \
        rb->buffer = (type *)alloc_func(length * sizeof(type));                                                        \
        if (!rb->buffer) {                                                                                             \
            return -1; /* Memory allocation failed */                                                                  \
        }                                                                                                              \
        rb->head  = 0;                                                                                                 \
        rb->tail  = 0;                                                                                                 \
        rb->count = 0;                                                                                                 \
        rb->size  = length;                                                                                            \
        for (unsigned i = 0; i < length; i++) {                                                                        \
            rb->buffer[i] = init;                                                                                      \
        }                                                                                                              \
        return 0; /* Success */                                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_free(rb_##name##_t *rb, void (*free_func)(void *))                                  \
    {                                                                                                                  \
        if (rb->buffer) {                                                                                              \
            free_func(rb->buffer);                                                                                     \
        }                                                                                                              \
        rb->buffer = 0;                                                                                                \
        rb->size   = 0;                                                                                                \
        rb->head   = 0;                                                                                                \
        rb->tail   = 0;                                                                                                \
        rb->count  = 0;                                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    static inline unsigned rb_##name##_is_empty(rb_##name##_t *rb) { return rb->count == 0; }                          \
                                                                                                                       \
    static inline unsigned rb_##name##_is_full(rb_##name##_t *rb) { return rb->count == rb->size; }                    \
                                                                                                                       \
    static inline void rb_##name##_push_back(rb_##name##_t *rb, type item)                                             \
    {                                                                                                                  \
        rb->buffer[rb->head] = item;                                                                                   \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->tail = (rb->tail + 1) % rb->size;                                                                      \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->head = (rb->head + 1) % rb->size;                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_push_front(rb_##name##_t *rb, type item)                                            \
    {                                                                                                                  \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->head = (rb->head == 0) ? rb->size - 1 : rb->head - 1;                                                  \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->tail             = (rb->tail == 0) ? rb->size - 1 : rb->tail - 1;                                          \
        rb->buffer[rb->tail] = item;                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_pop_front(rb_##name##_t *rb)                                                        \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        type item            = rb->buffer[rb->tail];                                                                   \
        rb->buffer[rb->tail] = init;                                                                                   \
        rb->tail             = (rb->tail + 1) % rb->size;                                                              \
        rb->count--;                                                                                                   \
        return item;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_pop_back(rb_##name##_t *rb)                                                         \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        rb->head             = (rb->head == 0) ? rb->size - 1 : rb->head - 1;                                          \
        type item            = rb->buffer[rb->head];                                                                   \
        rb->buffer[rb->head] = init;                                                                                   \
        rb->count--;                                                                                                   \
        return item;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_peek_front(rb_##name##_t *rb)                                                       \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        return rb->buffer[rb->tail];                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_peek_back(rb_##name##_t *rb)                                                        \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return init;                                                                                               \
        }                                                                                                              \
        return rb->buffer[(rb->head == 0) ? rb->size - 1 : rb->head - 1];                                              \
    }                                                                                                                  \
                                                                                                                       \
    static inline type rb_##name##_get(rb_##name##_t *rb, unsigned position)                                           \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb) || (position >= rb->size)) {                                                      \
            return init;                                                                                               \
        }                                                                                                              \
        return rb->buffer[(rb->tail + position) % rb->size];                                                           \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_iterate(rb_##name##_t *rb, void (*callback)(type))                                  \
    {                                                                                                                  \
        for (unsigned i = 0; i < rb->count; i++) {                                                                     \
            callback(rb_##name##_get(rb, i));                                                                          \
        }                                                                                                              \
    }

/// @brief Declares a fixed-size 2d ring buffer.
#define DECLARE_FIXED_SIZE_2D_RING_BUFFER(type, name, size1, size2, init)                                              \
    typedef struct {                                                                                                   \
        type buffer[size2];                                                                                            \
        unsigned size;                                                                                                 \
    } rb_##name##_entry_t;                                                                                             \
                                                                                                                       \
    void rb_##name##_entry_default_copy_fun(type *dest, const type *src, unsigned size)                                \
    {                                                                                                                  \
        for (unsigned i = 0; i < size; i++) {                                                                          \
            dest[i] = src[i];                                                                                          \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    typedef struct {                                                                                                   \
        rb_##name##_entry_t buffer[size1];                                                                             \
        unsigned size;                                                                                                 \
        unsigned head;                                                                                                 \
        unsigned tail;                                                                                                 \
        unsigned count;                                                                                                \
        void (*copy)(type * dest, const type *src, unsigned);                                                          \
    } rb_##name##_t;                                                                                                   \
                                                                                                                       \
    static inline void rb_##name##_init_entry(rb_##name##_entry_t *entry)                                              \
    {                                                                                                                  \
        entry->size = size2;                                                                                           \
        for (unsigned i = 0; i < size2; i++) {                                                                         \
            entry->buffer[i] = init;                                                                                   \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_init(rb_##name##_t *rb, void (*copy_fun)(type * dest, const type *src, unsigned))   \
    {                                                                                                                  \
        rb->head  = 0;                                                                                                 \
        rb->tail  = 0;                                                                                                 \
        rb->count = 0;                                                                                                 \
        rb->size  = size1;                                                                                             \
        if (copy_fun) {                                                                                                \
            rb->copy = copy_fun;                                                                                       \
        } else {                                                                                                       \
            rb->copy = rb_##name##_entry_default_copy_fun;                                                             \
        }                                                                                                              \
        for (unsigned i = 0; i < size1; i++) {                                                                         \
            rb_##name##_init_entry(rb->buffer + i);                                                                    \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static inline unsigned rb_##name##_is_empty(rb_##name##_t *rb) { return rb->count == 0; }                          \
                                                                                                                       \
    static inline unsigned rb_##name##_is_full(rb_##name##_t *rb) { return rb->count == size1; }                       \
                                                                                                                       \
    static inline void rb_##name##_push_back(rb_##name##_t *rb, rb_##name##_entry_t *item)                             \
    {                                                                                                                  \
        rb->copy(rb->buffer[rb->head].buffer, item->buffer, size2);                                                    \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->tail = (rb->tail + 1) % size1;                                                                         \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->head = (rb->head + 1) % size1;                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_push_front(rb_##name##_t *rb, rb_##name##_entry_t *item)                            \
    {                                                                                                                  \
        if (rb_##name##_is_full(rb)) {                                                                                 \
            rb->head = (rb->head == 0) ? size1 - 1 : rb->head - 1;                                                     \
        } else {                                                                                                       \
            rb->count++;                                                                                               \
        }                                                                                                              \
        rb->tail = (rb->tail == 0) ? size1 - 1 : rb->tail - 1;                                                         \
        rb->copy(rb->buffer[rb->tail].buffer, item->buffer, size2);                                                    \
    }                                                                                                                  \
                                                                                                                       \
    static inline int rb_##name##_pop_back(rb_##name##_t *rb, rb_##name##_entry_t *item)                               \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return 1;                                                                                                  \
        }                                                                                                              \
        rb->head = (rb->head == 0) ? size1 - 1 : rb->head - 1;                                                         \
        rb->copy(item->buffer, rb->buffer[rb->head].buffer, size2);                                                    \
        rb->count--;                                                                                                   \
        return 0;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline int rb_##name##_pop_front(rb_##name##_t *rb, rb_##name##_entry_t *item)                              \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return 1;                                                                                                  \
        }                                                                                                              \
        rb->copy(item->buffer, rb->buffer[rb->tail].buffer, size2);                                                    \
        rb->tail = (rb->tail + 1) % size1;                                                                             \
        rb->count--;                                                                                                   \
        return 0;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline int rb_##name##_peek_back(rb_##name##_t *rb, rb_##name##_entry_t *item)                              \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return 1;                                                                                                  \
        }                                                                                                              \
        unsigned index = ((rb->head == 0) ? size1 - 1 : rb->head - 1);                                                 \
        rb->copy(item->buffer, rb->buffer[index].buffer, size2);                                                       \
        return 0;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline int rb_##name##_peek_front(rb_##name##_t *rb, rb_##name##_entry_t *item)                             \
    {                                                                                                                  \
        if (rb_##name##_is_empty(rb)) {                                                                                \
            return 1;                                                                                                  \
        }                                                                                                              \
        rb->copy(item->buffer, rb->buffer[rb->tail].buffer, size2);                                                    \
        return 0;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline int rb_##name##_get(rb_##name##_t *rb, unsigned position, rb_##name##_entry_t *item)                 \
    {                                                                                                                  \
        if (!rb_##name##_is_empty(rb) && (position < rb->count)) {                                                     \
            unsigned index = (rb->tail + position) % size1;                                                            \
            rb->copy(item->buffer, rb->buffer[index].buffer, size2);                                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
        return 0;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline void rb_##name##_iterate(rb_##name##_t *rb, void (*callback)(rb_##name##_entry_t *))                 \
    {                                                                                                                  \
        rb_##name##_entry_t item;                                                                                      \
        for (unsigned i = 0; i < rb->count; i++) {                                                                     \
            rb_##name##_get(rb, i, &item);                                                                             \
            callback(&item);                                                                                           \
        }                                                                                                              \
    }
