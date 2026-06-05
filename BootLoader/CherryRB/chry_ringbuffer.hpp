/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <array>

typedef struct {
    uint32_t in;   /*!< Define the write pointer.               */
    uint32_t out;  /*!< Define the read pointer.                */
    uint32_t mask; /*!< Define the write and read pointer mask. */
    void *pool;    /*!< Define the memory pointer.              */
} chry_ringbuffer_t;

namespace {
extern "C" {
extern int chry_ringbuffer_init(chry_ringbuffer_t *rb, void *pool, uint32_t size);
extern void chry_ringbuffer_reset(chry_ringbuffer_t *rb);
extern void chry_ringbuffer_reset_read(chry_ringbuffer_t *rb);

extern uint32_t chry_ringbuffer_get_size(chry_ringbuffer_t *rb);
extern uint32_t chry_ringbuffer_get_used(chry_ringbuffer_t *rb);
extern uint32_t chry_ringbuffer_get_free(chry_ringbuffer_t *rb);

extern bool chry_ringbuffer_check_full(chry_ringbuffer_t *rb);
extern bool chry_ringbuffer_check_empty(chry_ringbuffer_t *rb);

extern bool chry_ringbuffer_write_byte(chry_ringbuffer_t *rb, uint8_t byte);
extern bool chry_ringbuffer_overwrite_byte(chry_ringbuffer_t *rb, uint8_t byte);
extern bool chry_ringbuffer_peek_byte(chry_ringbuffer_t *rb, uint8_t *byte);
extern bool chry_ringbuffer_read_byte(chry_ringbuffer_t *rb, uint8_t *byte);
extern bool chry_ringbuffer_drop_byte(chry_ringbuffer_t *rb);

extern uint32_t chry_ringbuffer_write(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_overwrite(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_peek(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_read(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_drop(chry_ringbuffer_t *rb, uint32_t size);

extern void *chry_ringbuffer_linear_write_setup(chry_ringbuffer_t *rb, uint32_t *size);
extern void *chry_ringbuffer_linear_read_setup(chry_ringbuffer_t *rb, uint32_t *size);
extern uint32_t chry_ringbuffer_linear_write_done(chry_ringbuffer_t *rb, uint32_t size);
extern uint32_t chry_ringbuffer_linear_read_done(chry_ringbuffer_t *rb, uint32_t size);
}
} // namespace

template <chry_ringbuffer_t *rb_, uint32_t size_>
struct Cherry_RingBuffer {

    static std::array<uint8_t, size_> pool_;

    static_assert((size_ & (size_ - 1)) == 0, "the size of the memory pool must be a power of 2 !!!");
    static_assert(rb_ != nullptr, "the ringbuffer pointer must be initialized !!!");
    static_assert(pool_.size() != 0, "the memory pool size must be > 0 !!!");
    static int init()
    {
        return chry_ringbuffer_init(rb_, (void *)pool_.data(), size_);
    }
    static void reset()
    {
        chry_ringbuffer_reset(rb_);
    }
    static void reset_read()
    {
        chry_ringbuffer_reset_read(rb_);
    }
    static uint32_t get_size()
    {
        return chry_ringbuffer_get_size(rb_);
    }
    static uint32_t get_used()
    {
        return chry_ringbuffer_get_used(rb_);
    }
    static uint32_t get_free()
    {
        return chry_ringbuffer_get_free(rb_);
    }
    static bool check_full()
    {
        return chry_ringbuffer_check_full(rb_);
    }
    static bool check_empty()
    {
        return chry_ringbuffer_check_empty(rb_);
    }
    static bool write_byte(uint8_t byte)
    {
        return chry_ringbuffer_write_byte(rb_, byte);
    }
    static bool overwrite_byte(uint8_t byte)
    {
        return chry_ringbuffer_overwrite_byte(rb_, byte);
    }
    static bool peek_byte(uint8_t byte)
    {
        uint8_t *p_byte = &byte;
        return chry_ringbuffer_peek_byte(rb_, p_byte);
    }
    static bool read_byte(uint8_t *byte)
    {
        return chry_ringbuffer_read_byte(rb_, byte);
    }
    static bool drop_byte()
    {
        return chry_ringbuffer_drop_byte(rb_);
    }
    static uint32_t write(void *data, uint32_t size)
    {
        return chry_ringbuffer_write(rb_, data, size);
    }
    static uint32_t overwrite(void *data, uint32_t size)
    {
        return chry_ringbuffer_overwrite(rb_, data, size);
    }
    static uint32_t peek(void *data, uint32_t size)
    {
        return chry_ringbuffer_peek(rb_, data, size);
    }
    static uint32_t read(void *data, uint32_t size)
    {
        return chry_ringbuffer_read(rb_, data, size);
    }
    static uint32_t drop(uint32_t size)
    {
        return chry_ringbuffer_drop(rb_, size);
    }
    static void *linear_write_setup(uint32_t *size)
    {
        return chry_ringbuffer_linear_write_setup(rb_, size);
    }
    static void *linear_read_setup(uint32_t *size)
    {
        return chry_ringbuffer_linear_read_setup(rb_, size);
    }
    static uint32_t linear_write_done(uint32_t size)
    {
        return chry_ringbuffer_linear_write_done(rb_, size);
    }
    static uint32_t linear_read_done(uint32_t size)
    {
        return chry_ringbuffer_linear_read_done(rb_, size);
    }
};
