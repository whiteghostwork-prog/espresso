/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_window.h"

#include "camera.h"
#include "wsi.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>

#define PB_BENCH_WINDOW_ORBIT_SPEED_RAD_S 0.45f

struct pb_bench_window {
    pb_context *context;
    pb_example_wsi *wsi;
    pb_bench_scenario *scenario;
    pb_rhi_query_pool *query_pool;
    pb_example_camera *camera;
    bool detailed;
    bool have_time_origin;
    double time_origin_s;
    double prev_time_s;
};

static void bench_pre_render(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    pb_bench_scenario *scenario = user_data;
    if (scenario && scenario->pre_record) {
        scenario->pre_record(cmd, extent, scenario->user_data);
    }
}

bool pb_bench_window_create(
    pb_bench_window **out_window,
    pb_context *context,
    uint32_t width,
    uint32_t height,
    const char *title,
    pb_bench_scenario *scenario,
    bool detailed)
{
    if (!out_window || !context || !title || !scenario || width == 0 || height == 0) {
        return false;
    }

    pb_bench_window *window = calloc(1, sizeof(*window));
    if (!window) {
        return false;
    }

    window->context = context;
    window->scenario = scenario;
    window->detailed = detailed;

    window->wsi = pb_example_wsi_create(
        &(pb_example_wsi_desc){
            .context = context,
            .width = width,
            .height = height,
            .title = title,
            .enable_stats = false,
            .disable_msaa = true,
        });
    if (!window->wsi) {
        free(window);
        return false;
    }

    const VkExtent2D extent = pb_example_wsi_extent(window->wsi);
    const VkRenderPass render_pass = pb_example_wsi_render_pass(window->wsi);
    if (!scenario->setup(scenario, context, render_pass, extent)) {
        pb_example_wsi_destroy(window->wsi);
        free(window);
        return false;
    }

    if (!pb_rhi_query_pool_create(context, detailed, &window->query_pool)) {
        if (scenario->teardown) {
            scenario->teardown(scenario);
        }
        pb_example_wsi_destroy(window->wsi);
        free(window);
        return false;
    }

    pb_example_wsi_set_bench_timing(window->wsi, window->query_pool, detailed);
    if (scenario->pre_record) {
        pb_example_wsi_set_pre_render(window->wsi, bench_pre_render, scenario);
    }

    window->camera = pb_example_camera_create(
        &(pb_example_camera_desc){
            .azimuth_rad = 0.0f,
            .elevation_rad = 0.35f,
            .distance = 3.0f,
        });
    if (!window->camera) {
        pb_bench_window_destroy(window);
        *out_window = NULL;
        return false;
    }

    *out_window = window;
    return true;
}

void pb_bench_window_destroy(pb_bench_window *window)
{
    if (!window) {
        return;
    }

    if (window->scenario && window->scenario->teardown) {
        window->scenario->teardown(window->scenario);
    }

    if (window->context && pb_context_device_ready(window->context)) {
        pb_rhi_query_pool_destroy(window->context, window->query_pool);
    }

    pb_example_camera_destroy(window->camera);
    pb_example_wsi_destroy(window->wsi);
    free(window);
}

void pb_bench_window_poll(pb_bench_window *window)
{
    if (window && window->wsi) {
        pb_example_wsi_poll(window->wsi);
    }
}

bool pb_bench_window_should_close(const pb_bench_window *window)
{
    return !window || !window->wsi || pb_example_wsi_should_close(window->wsi);
}

bool pb_bench_window_run_frame(pb_bench_window *window, pb_bench_frame *out_frame)
{
    if (!window || !out_frame) {
        return false;
    }

    pb_example_wsi_poll(window->wsi);

    const double now_s = glfwGetTime();
    if (!window->have_time_origin) {
        window->time_origin_s = now_s;
        window->prev_time_s = now_s;
        window->have_time_origin = true;
    }

    const float elapsed_s = (float)(now_s - window->time_origin_s);
    const float delta_s = (float)(now_s - window->prev_time_s);
    window->prev_time_s = now_s;

    pb_example_camera_orbit_auto(window->camera, delta_s, PB_BENCH_WINDOW_ORBIT_SPEED_RAD_S);

    const VkExtent2D extent = pb_example_wsi_extent(window->wsi);
    if (window->scenario->window_tick) {
        window->scenario->window_tick(
            window->scenario,
            &(pb_bench_window_tick){
                .time_seconds = elapsed_s,
                .delta_seconds = delta_s,
                .frame_slot = pb_example_wsi_frame_index(window->wsi),
                .extent = extent,
                .camera = window->camera,
            });
    }

    if (!pb_example_wsi_begin_frame(window->wsi, 0.02f, 0.02f, 0.025f, 1.0f)) {
        return false;
    }

    VkCommandBuffer cmd = pb_example_wsi_command_buffer(window->wsi);
    if (window->scenario->record) {
        window->scenario->record(cmd, extent, window->scenario->user_data);
    }

    return pb_example_wsi_end_frame_bench(window->wsi, out_frame);
}

VkExtent2D pb_bench_window_extent(const pb_bench_window *window)
{
    if (!window || !window->wsi) {
        return (VkExtent2D){0, 0};
    }

    return pb_example_wsi_extent(window->wsi);
}
