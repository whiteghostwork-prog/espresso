/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PEABERRY_BENCH_WINDOW_H
#define PEABERRY_BENCH_WINDOW_H

#include "peaberry/peaberry.h"
#include "peaberry/peaberry_bench.h"
#include "scenario.h"

#include <stdbool.h>
#include <stdint.h>
#include <volk.h>

typedef struct pb_bench_window pb_bench_window;

bool pb_bench_window_create(
    pb_bench_window **out_window,
    pb_context *context,
    uint32_t width,
    uint32_t height,
    const char *title,
    pb_bench_scenario *scenario,
    bool detailed);

void pb_bench_window_destroy(pb_bench_window *window);

bool pb_bench_window_run_frame(pb_bench_window *window, pb_bench_frame *out_frame);

VkExtent2D pb_bench_window_extent(const pb_bench_window *window);

void pb_bench_window_poll(pb_bench_window *window);

bool pb_bench_window_should_close(const pb_bench_window *window);

#endif
