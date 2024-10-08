/// @file ring_buffer.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Declares a fixed-size ring buffer.
#define DECLARE_FIXED_SIZE_RING_BUFFER(type, name, length, init)                  \
    typedef struct {                                                              \
        type buffer[length];                                                      \
        unsigned size;                                                            \
        unsigned head;                                                            \
        unsigned tail;                                                            \
        unsigned count;                                                           \
    } rb_##name##_t;                                                              \
                                                                                  \
    static inline void rb_##name##_init(rb_##name##_t *rb)                        \
    {                                                                             \
        rb->head  = 0;                                                            \
        rb->tail  = 0;                                                            \
        rb->count = 0;                                                            \
        rb->size  = length;                                                       \
        for (unsigned i = 0; i < length; i++) {                                   \
            rb->buffer[i] = init;                                                 \
        }                                                                         \
    }                                                                             \
                                                                                  \
    static inline unsigned rb_##name##_is_empty(rb_##name##_t *rb)                \
    {                                                                             \
        return rb->count == 0;                                                    \
    }                                                                             \
                                                                                  \
    static inline unsigned rb_##name##_is_full(rb_##name##_t *rb)                 \
    {                                                                             \
        return rb->count == rb->size;                                             \
    }                                                                             \
                                                                                  \
    static inline void rb_##name##_push_back(rb_##name##_t *rb, type item)        \
    {                                                                             \
        rb->buffer[rb->head] = item;                                              \
        if (rb_##name##_is_full(rb)) {                                            \
            rb->tail = (rb->tail + 1) % rb->size;                                 \
        } else {                                                                  \
            rb->count++;                                                          \
        }                                                                         \
        rb->head = (rb->head + 1) % rb->size;                                     \
    }                                                                             \
                                                                                  \
    static inline void rb_##name##_push_front(rb_##name##_t *rb, type item)       \
    {                                                                             \
        if (rb_##name##_is_full(rb)) {                                            \
            rb->head = (rb->head == 0) ? rb->size - 1 : rb->head - 1;             \
        } else {                                                                  \
            rb->count++;                                                          \
        }                                                                         \
        rb->tail             = (rb->tail == 0) ? rb->size - 1 : rb->tail - 1;     \
        rb->buffer[rb->tail] = item;                                              \
    }                                                                             \
                                                                                  \
    static inline type rb_##name##_pop_front(rb_##name##_t *rb)                   \
    {                                                                             \
        type item = init;                                                         \
        if (!rb_##name##_is_empty(rb)) {                                          \
            item                 = rb->buffer[rb->tail];                          \
            rb->buffer[rb->tail] = init;                                          \
            rb->tail             = (rb->tail + 1) % rb->size;                     \
            rb->count--;                                                          \
        }                                                                         \
        return item;                                                              \
    }                                                                             \
                                                                                  \
    static inline type rb_##name##_pop_back(rb_##name##_t *rb)                    \
    {                                                                             \
        type item = init;                                                         \
        if (!rb_##name##_is_empty(rb)) {                                          \
            rb->head             = (rb->head == 0) ? rb->size - 1 : rb->head - 1; \
            item                 = rb->buffer[rb->head];                          \
            rb->buffer[rb->head] = init;                                          \
            rb->count--;                                                          \
        }                                                                         \
        return item;                                                              \
    }                                                                             \
                                                                                  \
    static inline type rb_##name##_peek_front(rb_##name##_t *rb)                  \
    {                                                                             \
        if (rb_##name##_is_empty(rb)) {                                           \
            return init;                                                          \
        }                                                                         \
        return rb->buffer[rb->tail];                                              \
    }                                                                             \
                                                                                  \
    static inline type rb_##name##_peek_back(rb_##name##_t *rb)                   \
    {                                                                             \
        if (rb_##name##_is_empty(rb)) {                                           \
            return init;                                                          \
        }                                                                         \
        return rb->buffer[(rb->head == 0) ? rb->size - 1 : rb->head - 1];         \
    }                                                                             \
                                                                                  \
    static inline type rb_##name##_get(rb_##name##_t *rb, unsigned position)      \
    {                                                                             \
        type item = init;                                                         \
        if (!rb_##name##_is_empty(rb) && (position < rb->size)) {                 \
            item = rb->buffer[(rb->tail + position) % rb->size];                  \
        }                                                                         \
        return item;                                                              \
    }                                                                             \
                                                                                  \
    static inline void rb_##name##_iterate(rb_##name##_t *rb,                     \
                                           void (*callback)(type))                \
    {                                                                             \
        for (unsigned i = 0; i < rb->count; i++) {                                \
            callback(rb_##name##_get(rb, i));                                     \
        }                                                                         \
    }

/// @brief Declares a dynamic-size ring-buffer.
#define DECLARE_DYNAMIC_SIZE_RING_BUFFER(type, name, init)                            \
    typedef struct {                                                                  \
        type *buffer;                                                                 \
        unsigned size;                                                                \
        unsigned head;                                                                \
        unsigned tail;                                                                \
        unsigned count;                                                               \
    } rb_##name##_t;                                                                  \
                                                                                      \
    static inline int rb_##name##_init(rb_##name##_t *rb,                             \
                                       unsigned length,                               \
                                       void *(*alloc_func)(size_t))                   \
    {                                                                                 \
        rb->buffer = (type *)alloc_func(length * sizeof(type));                       \
        if (!rb->buffer) {                                                            \
            return -1; /* Memory allocation failed */                                 \
        }                                                                             \
        rb->head  = 0;                                                                \
        rb->tail  = 0;                                                                \
        rb->count = 0;                                                                \
        rb->size  = length;                                                           \
        for (unsigned i = 0; i < length; i++) {                                       \
            rb->buffer[i] = init;                                                     \
        }                                                                             \
        return 0; /* Success */                                                       \
    }                                                                                 \
                                                                                      \
    static inline void rb_##name##_free(rb_##name##_t *rb, void (*free_func)(void *)) \
    {                                                                                 \
        if (rb->buffer) {                                                             \
            free_func(rb->buffer);                                                    \
        }                                                                             \
        rb->buffer = 0;                                                               \
        rb->size   = 0;                                                               \
        rb->head   = 0;                                                               \
        rb->tail   = 0;                                                               \
        rb->count  = 0;                                                               \
    }                                                                                 \
                                                                                      \
    static inline unsigned rb_##name##_is_empty(rb_##name##_t *rb)                    \
    {                                                                                 \
        return rb->count == 0;                                                        \
    }                                                                                 \
                                                                                      \
    static inline unsigned rb_##name##_is_full(rb_##name##_t *rb)                     \
    {                                                                                 \
        return rb->count == rb->size;                                                 \
    }                                                                                 \
                                                                                      \
    static inline void rb_##name##_push_back(rb_##name##_t *rb, type item)            \
    {                                                                                 \
        rb->buffer[rb->head] = item;                                                  \
        if (rb_##name##_is_full(rb)) {                                                \
            rb->tail = (rb->tail + 1) % rb->size;                                     \
        } else {                                                                      \
            rb->count++;                                                              \
        }                                                                             \
        rb->head = (rb->head + 1) % rb->size;                                         \
    }                                                                                 \
                                                                                      \
    static inline void rb_##name##_push_front(rb_##name##_t *rb, type item)           \
    {                                                                                 \
        if (rb_##name##_is_full(rb)) {                                                \
            rb->head = (rb->head == 0) ? rb->size - 1 : rb->head - 1;                 \
        } else {                                                                      \
            rb->count++;                                                              \
        }                                                                             \
        rb->tail             = (rb->tail == 0) ? rb->size - 1 : rb->tail - 1;         \
        rb->buffer[rb->tail] = item;                                                  \
    }                                                                                 \
                                                                                      \
    static inline type rb_##name##_pop_front(rb_##name##_t *rb)                       \
    {                                                                                 \
        if (rb_##name##_is_empty(rb)) { return init; }                                \
        type item            = rb->buffer[rb->tail];                                  \
        rb->buffer[rb->tail] = init;                                                  \
        rb->tail             = (rb->tail + 1) % rb->size;                             \
        rb->count--;                                                                  \
        return item;                                                                  \
    }                                                                                 \
                                                                                      \
    static inline type rb_##name##_pop_back(rb_##name##_t *rb)                        \
    {                                                                                 \
        if (rb_##name##_is_empty(rb)) { return init; }                                \
        rb->head             = (rb->head == 0) ? rb->size - 1 : rb->head - 1;         \
        type item            = rb->buffer[rb->head];                                  \
        rb->buffer[rb->head] = init;                                                  \
        rb->count--;                                                                  \
        return item;                                                                  \
    }                                                                                 \
                                                                                      \
    static inline type rb_##name##_peek_front(rb_##name##_t *rb)                      \
    {                                                                                 \
        if (rb_##name##_is_empty(rb)) { return init; }                                \
        return rb->buffer[rb->tail];                                                  \
    }                                                                                 \
                                                                                      \
    static inline type rb_##name##_peek_back(rb_##name##_t *rb)                       \
    {                                                                                 \
        if (rb_##name##_is_empty(rb)) { return init; }                                \
        return rb->buffer[(rb->head == 0) ? rb->size - 1 : rb->head - 1];             \
    }                                                                                 \
                                                                                      \
    static inline type rb_##name##_get(rb_##name##_t *rb, unsigned position)          \
    {                                                                                 \
        if (rb_##name##_is_empty(rb) || (position >= rb->size)) { return init; }      \
        return rb->buffer[(rb->tail + position) % rb->size];                          \
    }                                                                                 \
                                                                                      \
    static inline void rb_##name##_iterate(rb_##name##_t *rb,                         \
                                           void (*callback)(type))                    \
    {                                                                                 \
        for (unsigned i = 0; i < rb->count; i++) {                                    \
            callback(rb_##name##_get(rb, i));                                         \
        }                                                                             \
    }
