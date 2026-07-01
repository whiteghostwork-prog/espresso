/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PEABERRY_BENCH_SCENARIO_H
#define PEABERRY_BENCH_SCENARIO_H

#include "peaberry/peaberry.h"
#include "peaberry/peaberry_bench.h"

#include "camera.h"

#include <stdbool.h>
#include <stdint.h>
#include <volk.h>

struct pb_example_camera;

typedef struct pb_bench_scenario pb_bench_scenario;

typedef struct pb_bench_scenario_info {
    uint32_t draw_calls;
    uint32_t visible_draw_calls;
    uint32_t index_count;
    uint32_t material_count;
    uint32_t pixels_shaded;
} pb_bench_scenario_info;

typedef void (*pb_bench_record_fn)(VkCommandBuffer cmd, VkExtent2D extent, void *user_data);

typedef struct pb_bench_window_tick {
    float time_seconds;
    float delta_seconds;
    uint32_t frame_slot;
    VkExtent2D extent;
    struct pb_example_camera *camera;
} pb_bench_window_tick;

typedef void (*pb_bench_window_tick_fn)(pb_bench_scenario *scenario, const pb_bench_window_tick *tick);

typedef struct pb_bench_scenario {
    const char *name;
    bool (*setup)(pb_bench_scenario *scenario, pb_context *context, VkRenderPass render_pass, VkExtent2D extent);
    void (*teardown)(pb_bench_scenario *scenario);
    pb_bench_record_fn pre_record;
    pb_bench_record_fn record;
    pb_bench_window_tick_fn window_tick;
    void *user_data;
    pb_bench_scenario_info info;
    bool has_window_camera;
    pb_example_camera_desc window_camera;
} pb_bench_scenario;

bool pb_bench_scenario_clear_init(pb_bench_scenario *scenario, pb_context *context, VkExtent2D extent);
bool pb_bench_scenario_sphere_init(pb_bench_scenario *scenario, pb_context *context, VkExtent2D extent);
bool pb_bench_scenario_gltf_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path);

bool pb_bench_scenario_gltf_shadows_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path);

bool pb_bench_scenario_gltf_instanced_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *scenario_arg);

bool pb_bench_scenario_gltf_stress_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path);

bool pb_bench_scenario_gltf_stress_shadows_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path);

#endif
