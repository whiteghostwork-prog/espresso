/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_window.h"

#include "camera.h"
#include "wsi.h"

#include "peaberry/peaberry_frame_metrics.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PB_BENCH_WINDOW_ORBIT_SPEED_RAD_S 0.45f
#define PB_BENCH_WINDOW_GPU_FPS_WINDOW_S 1.0

typedef struct bench_gpu_fps_accumulator {
    double window_start_s;
    uint32_t frames_in_window;
    uint64_t gpu_total_ns_sum;
    double gpu_fps;
} bench_gpu_fps_accumulator;

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
    char base_title[128];
    bench_gpu_fps_accumulator gpu_fps_accumulator;
};

static void bench_pre_render(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    pb_bench_scenario *scenario = user_data;
    if (scenario && scenario->pre_record) {
        scenario->pre_record(cmd, extent, scenario->user_data);
    }
}

static void bench_gpu_fps_accumulator_push(
    bench_gpu_fps_accumulator *acc,
    double now_s,
    uint64_t gpu_total_ns)
{
    if (!acc || gpu_total_ns == 0) {
        return;
    }

    if (acc->frames_in_window == 0) {
        acc->window_start_s = now_s;
    }

    acc->frames_in_window++;
    acc->gpu_total_ns_sum += gpu_total_ns;

    const double elapsed = now_s - acc->window_start_s;
    if (elapsed < PB_BENCH_WINDOW_GPU_FPS_WINDOW_S) {
        return;
    }

    if (acc->frames_in_window > 0) {
        const uint64_t avg_gpu_ns = acc->gpu_total_ns_sum / acc->frames_in_window;
        acc->gpu_fps = pb_frame_metrics_fps_from_ns(avg_gpu_ns);
    }

    acc->window_start_s = now_s;
    acc->frames_in_window = 0;
    acc->gpu_total_ns_sum = 0;
}

static void bench_window_update_fps_title(pb_bench_window *window, const pb_bench_frame *frame, double now_s)
{
    GLFWwindow *glfw_window = pb_example_wsi_window(window->wsi);
    if (!glfw_window || !frame) {
        return;
    }

    pb_frame_metrics metrics;
    pb_frame_metrics_from_bench_frame(frame, 0, 0, PB_FRAME_BUDGET_60HZ_NS, &metrics);

    bench_gpu_fps_accumulator_push(&window->gpu_fps_accumulator, now_s, frame->gpu_total_ns);

    const double display_fps = window->gpu_fps_accumulator.gpu_fps > 0.0 ? window->gpu_fps_accumulator.gpu_fps :
                                                                             metrics.gpu_fps;

    char overlay[160];
    const double gpu_ms = metrics.gpu_total_ns / 1e6;
    if (snprintf(
            overlay,
            sizeof(overlay),
            "FPS %.1f | GPU %.2f ms (%.0f%%)",
            display_fps,
            gpu_ms,
            metrics.gpu_load_percent) <= 0) {
        return;
    }

    char title[320];
    snprintf(title, sizeof(title), "%s | %s", window->base_title, overlay);
    glfwSetWindowTitle(glfw_window, title);
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
        scenario->has_window_camera ? &scenario->window_camera :
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

    snprintf(window->base_title, sizeof(window->base_title), "%s", title);
    memset(&window->gpu_fps_accumulator, 0, sizeof(window->gpu_fps_accumulator));

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

    if (!pb_example_wsi_end_frame_bench(window->wsi, out_frame)) {
        return false;
    }

    bench_window_update_fps_title(window, out_frame, now_s);
    return true;
}

VkExtent2D pb_bench_window_extent(const pb_bench_window *window)
{
    if (!window || !window->wsi) {
        return (VkExtent2D){0, 0};
    }

    return pb_example_wsi_extent(window->wsi);
}
