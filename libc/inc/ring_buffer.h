/// @file ring_buffer.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Declares a fixed-size ring-buffer.
#define DECLARE_FIXED_SIZE_RING_BUFFER(type, name, length, init)                     \
    typedef struct fs_rb_##name##_t {                                                \
        unsigned size, read, write;                                                  \
        type buffer[length];                                                         \
    } fs_rb_##name##_t;                                                              \
    static inline void fs_rb_##name##_init(fs_rb_##name##_t *rb)                     \
    {                                                                                \
        rb->size = length;                                                           \
        rb->read = rb->write = 0;                                                    \
        char *dst            = (char *)rb->buffer;                                   \
        long num             = sizeof(type) * length;                                \
        while (num--) *dst++ = (char)(init & 0xFF);                                  \
    }                                                                                \
    static inline unsigned fs_rb_##name##_step(fs_rb_##name##_t *rb, unsigned index) \
    {                                                                                \
        return (index == (rb->size - 1)) ? 0 : index + 1;                            \
    }                                                                                \
    static inline void fs_rb_##name##_push_front(fs_rb_##name##_t *rb, type item)    \
    {                                                                                \
        if (fs_rb_##name##_step(rb, rb->write) == rb->read)                          \
            rb->read = fs_rb_##name##_step(rb, rb->read);                            \
        rb->buffer[rb->write] = item;                                                \
        rb->write             = fs_rb_##name##_step(rb, rb->write);                  \
    }                                                                                \
    static inline type fs_rb_##name##_empty(fs_rb_##name##_t *rb)                    \
    {                                                                                \
        return rb->write == rb->read;                                                \
    }                                                                                \
    static inline type fs_rb_##name##_pop_back(fs_rb_##name##_t *rb)                 \
    {                                                                                \
        type item = init;                                                            \
        if (!fs_rb_##name##_empty(rb)) {                                             \
            item     = rb->buffer[rb->read];                                         \
            rb->read = fs_rb_##name##_step(rb, rb->read);                            \
        }                                                                            \
        return item;                                                                 \
    }                                                                                \
    static inline type fs_rb_##name##_pop_front(fs_rb_##name##_t *rb)                \
    {                                                                                \
        if (fs_rb_##name##_empty(rb))                                                \
            return init;                                                             \
        rb->write = (rb->write > 0) ? rb->write - 1 : rb->size - 1;                  \
        return rb->buffer[rb->write];                                                \
    }                                                                                \
    static inline type fs_rb_##name##_get(fs_rb_##name##_t *rb, unsigned index)      \
    {                                                                                \
        if (index < rb->size)                                                        \
            return rb->buffer[index];                                                \
        return init;                                                                 \
    }                                                                                \
    static inline type fs_rb_##name##_back(fs_rb_##name##_t *rb)                     \
    {                                                                                \
        if (fs_rb_##name##_empty(rb))                                                \
            return init;                                                             \
        return rb->buffer[rb->read];                                                 \
    }                                                                                \
    static inline type fs_rb_##name##_front(fs_rb_##name##_t *rb)                    \
    {                                                                                \
        if (fs_rb_##name##_empty(rb))                                                \
            return init;                                                             \
        return rb->buffer[(rb->write > 0) ? rb->write - 1 : rb->size - 1];           \
    }

#ifdef __KERNEL__
/// Function for allocating memory for the ring buffer.
#define RING_BUFFER_ALLOC kmalloc
/// Function for freeing the memory for the ring buffer.
#define RING_BUFFER_FREE  kfree
#else
/// Function for allocating memory for the ring buffer.
#define RING_BUFFER_ALLOC malloc
/// Function for freeing the memory for the ring buffer.
#define RING_BUFFER_FREE  free
#endif

/// @brief Declares a dynamic-size ring-buffer.
#define DECLARE_RING_BUFFER(type, name, init)                                                       \
    typedef struct rb_##name##_t {                                                                  \
        const unsigned size;                                                                        \
        unsigned read, write;                                                                       \
        type *buffer;                                                                               \
    } rb_##name##_t;                                                                                \
    static inline rb_##name##_t alloc_rb_##name(unsigned len)                                       \
    {                                                                                               \
        rb_##name##_t rb = { len, 0U, 0U, len > 0 ? RING_BUFFER_ALLOC(sizeof(type) * len) : NULL }; \
        memset(rb.buffer, init, sizeof(type) * len);                                                \
        return rb;                                                                                  \
    }                                                                                               \
    static inline void free_rb_##name(rb_##name##_t *rb)                                            \
    {                                                                                               \
        RING_BUFFER_FREE(rb->buffer);                                                               \
    }                                                                                               \
    static inline unsigned step_rb_##name(rb_##name##_t *rb, unsigned index)                        \
    {                                                                                               \
        return (index == (rb->size - 1)) ? 0 : index + 1;                                           \
    }                                                                                               \
    static inline void push_rb_##name(rb_##name##_t *rb, type item)                                 \
    {                                                                                               \
        if (step_rb_##name(rb, rb->write) == rb->read)                                              \
            rb->read = step_rb_##name(rb, rb->read);                                                \
        rb->buffer[rb->write] = item;                                                               \
        rb->write             = step_rb_##name(rb, rb->write);                                      \
    }                                                                                               \
    static inline void pop_rb_##name(rb_##name##_t *rb, type *item)                                 \
    {                                                                                               \
        *item = init;                                                                               \
        if (rb->write != rb->read) {                                                                \
            *item    = rb->buffer[rb->read];                                                        \
            rb->read = step_rb_##name(rb, rb->read);                                                \
        }                                                                                           \
    }                                                                                               \
    static inline void get_rb_##name(rb_##name##_t *rb, unsigned index, type *item)                 \
    {                                                                                               \
        if (index < rb->size)                                                                       \
            *item = rb->buffer[index];                                                              \
    }

#undef RING_BUFFER_ALLOC
#undef RING_BUFFER_FREE
