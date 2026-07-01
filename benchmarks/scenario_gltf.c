/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_paths.h"
#include "scenario.h"

#include "camera.h"
#include "peaberry/peaberry_gltf.h"
#include "peaberry/peaberry_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gltf_state {
    uint32_t tag;
    pb_bench_scenario *scenario;
    pb_pbr_forward_pass *pass;
    pb_gltf_scene *scene;
    float time_seconds;
    bool apply_stress_camera;
} gltf_state;

typedef struct gltf_path_config {
    uint32_t tag;
    char model_path[512];
    bool shadows_enabled;
    bool stress_camera;
} gltf_path_config;

enum {
    GLTF_PATH_CFG_TAG = 0x68536c67u,
    GLTF_STATE_TAG = 0x68537467u,
};

static bool gltf_path_scenario_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path,
    const char *default_subdir,
    const char *default_filename,
    const char *name,
    bool shadows_enabled,
    bool stress_camera);

static uint32_t gltf_total_index_count(const pb_gltf_scene *scene)
{
    uint32_t total = 0;

    for (uint32_t i = 0; i < pb_gltf_scene_draw_count(scene); ++i) {
        pb_gltf_draw_info draw = {0};
        if (pb_gltf_scene_get_draw(scene, i, &draw)) {
            total += draw.index_count;
        }
    }

    return total;
}

static void gltf_apply_stress_headless_camera(pb_pbr_forward_pass *pass, VkExtent2D extent)
{
    const pb_vec3 eye = { 16.0f, 12.0f, 16.0f };
    const pb_vec3 center = { 0.0f, 0.0f, 0.0f };
    const pb_vec3 up = { 0.0f, 1.0f, 0.0f };
    pb_mat4 view;
    pb_mat4 proj;

    pb_mat4_look_at(view, eye, center, up);
    const float aspect =
        (extent.width > 0 && extent.height > 0) ? (float)extent.width / (float)extent.height : 1.0f;
    pb_mat4_perspective(proj, pb_radians(45.0f), aspect, 0.1f, 200.0f);
    pb_pbr_forward_pass_set_camera(pass, view, proj, eye);
}

static bool gltf_fill_default_path(char *path, size_t path_size, const char *subdir, const char *filename)
{
    return snprintf(path, path_size, "%s/%s/%s", PEABERRY_ASSET_DIR, subdir, filename) < (int)path_size;
}

static bool gltf_setup_internal(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkRenderPass render_pass,
    VkExtent2D extent,
    const char *model_path,
    bool shadows_enabled,
    bool stress_camera)
{
    if (!scenario || !model_path) {
        return false;
    }

    char vert_spv[512];
    char frag_spv[512];
    if (snprintf(vert_spv, sizeof(vert_spv), "%s/pbr_forward.vert.spv", PEABERRY_SHADER_DIR) >= (int)sizeof(vert_spv) ||
        snprintf(frag_spv, sizeof(frag_spv), "%s/pbr_forward.frag.spv", PEABERRY_SHADER_DIR) >= (int)sizeof(frag_spv)) {
        return false;
    }

    gltf_state *state = calloc(1, sizeof(*state));
    if (!state) {
        return false;
    }

    state->tag = GLTF_STATE_TAG;
    state->scene = pb_gltf_scene_create(
        &(pb_gltf_scene_desc){
            .context = context,
            .path = model_path,
            .scene_index = PB_GLTF_SCENE_INDEX_DEFAULT,
        });
    if (!state->scene) {
        fprintf(stderr, "failed to load glTF model: %s\n", model_path);
        free(state);
        return false;
    }

    state->pass = pb_pbr_forward_pass_create(
        &(pb_pbr_forward_pass_desc){
            .context = context,
            .render_pass = render_pass,
            .vert_spv_path = vert_spv,
            .frag_spv_path = frag_spv,
            .ibl_shader_dir = PEABERRY_SHADER_DIR,
            .exposure = 1.2f,
            .scene = state->scene,
        });
    if (!state->pass || !pb_pbr_forward_pass_scene_is_bound(state->pass)) {
        pb_gltf_scene_destroy(state->scene);
        free(state);
        return false;
    }

    pb_pbr_forward_pass_set_shadows_enabled(state->pass, shadows_enabled);

    state->apply_stress_camera = stress_camera;
    if (stress_camera) {
        gltf_apply_stress_headless_camera(state->pass, extent);
    }

    scenario->user_data = state;
    state->scenario = scenario;
    scenario->info.draw_calls = pb_gltf_scene_draw_count(state->scene);
    scenario->info.visible_draw_calls = scenario->info.draw_calls;
    scenario->info.index_count = gltf_total_index_count(state->scene);
    scenario->info.material_count = pb_gltf_scene_material_count(state->scene);
    scenario->info.pixels_shaded = extent.width * extent.height;
    return true;
}

static bool gltf_setup(pb_bench_scenario *scenario, pb_context *context, VkRenderPass render_pass, VkExtent2D extent)
{
    const char *model_path = scenario->user_data;
    return gltf_setup_internal(scenario, context, render_pass, extent, model_path, false, false);
}

static bool gltf_path_setup(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkRenderPass render_pass,
    VkExtent2D extent)
{
    if (!scenario || !scenario->user_data) {
        return false;
    }

    gltf_path_config cfg = *(gltf_path_config *)scenario->user_data;
    free(scenario->user_data);
    scenario->user_data = NULL;

    return gltf_setup_internal(
        scenario,
        context,
        render_pass,
        extent,
        cfg.model_path,
        cfg.shadows_enabled,
        cfg.stress_camera);
}

static void gltf_teardown(pb_bench_scenario *scenario)
{
    gltf_state *state = scenario ? scenario->user_data : NULL;
    if (!state) {
        return;
    }

    if (state->pass) {
        pb_pbr_forward_pass_destroy(state->pass);
    }
    if (state->scene) {
        pb_gltf_scene_destroy(state->scene);
    }

    free(state);
    scenario->user_data = NULL;
}

static void gltf_path_teardown(pb_bench_scenario *scenario)
{
    if (!scenario || !scenario->user_data) {
        return;
    }

    const uint32_t tag = *(const uint32_t *)scenario->user_data;
    if (tag == GLTF_PATH_CFG_TAG) {
        free(scenario->user_data);
        scenario->user_data = NULL;
        return;
    }

    if (tag == GLTF_STATE_TAG) {
        gltf_teardown(scenario);
        return;
    }

    gltf_teardown(scenario);
}

static void gltf_apply_window_motion(gltf_state *state, const pb_bench_window_tick *tick)
{
    if (!state || !tick || !state->pass || !state->scene || !tick->camera) {
        return;
    }

    state->time_seconds = tick->time_seconds;

    pb_gltf_scene_set_frame_slot(state->scene, tick->frame_slot);
    pb_pbr_forward_pass_set_frame_slot(state->pass, tick->frame_slot);

    pb_mat4 view;
    pb_mat4 proj;
    pb_vec3 cam_pos;
    pb_example_camera_get_view(tick->camera, view);
    const float aspect =
        tick->extent.height > 0 ? (float)tick->extent.width / (float)tick->extent.height : 1.0f;
    pb_example_camera_get_proj(tick->camera, aspect, proj);
    pb_example_camera_get_position(tick->camera, cam_pos);
    pb_pbr_forward_pass_set_camera(state->pass, view, proj, cam_pos);

    if (pb_gltf_scene_animation_count(state->scene) > 0) {
        const float duration = pb_gltf_scene_animation_duration(state->scene, 0);
        if (duration > 0.0f) {
            pb_gltf_scene_update_animation(state->scene, 0, fmodf(tick->time_seconds, duration));
        }
    }
}

static void gltf_window_tick(pb_bench_scenario *scenario, const pb_bench_window_tick *tick)
{
    gltf_apply_window_motion(scenario ? scenario->user_data : NULL, tick);
}

static void gltf_shadow_pre_record(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    const gltf_state *state = user_data;
    if (!state || !state->pass || !state->scene) {
        return;
    }

    pb_pbr_forward_pass_record_shadow_map(state->pass, cmd, extent, state->scene);
}

static void gltf_record(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    gltf_state *state = user_data;
    if (!state || !state->pass || !state->scene) {
        return;
    }

    if (state->apply_stress_camera &&
        !(state->scenario && state->scenario->has_window_camera)) {
        gltf_apply_stress_headless_camera(state->pass, extent);
    }

    pb_pbr_forward_pass_record(state->pass, cmd, extent, state->scene, state->time_seconds);

    if (state->scenario) {
        state->scenario->info.visible_draw_calls = pb_pbr_forward_pass_last_visible_draw_count(state->pass);
    }
}

bool pb_bench_scenario_gltf_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path)
{
    if (!scenario || !model_path) {
        return false;
    }

    memset(scenario, 0, sizeof(*scenario));
    scenario->name = "gltf";
    scenario->setup = gltf_setup;
    scenario->teardown = gltf_teardown;
    scenario->pre_record = NULL;
    scenario->record = gltf_record;
    scenario->window_tick = gltf_window_tick;
    scenario->user_data = (void *)model_path;
    (void)context;
    (void)extent;
    return true;
}

bool pb_bench_scenario_gltf_shadows_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path)
{
    return gltf_path_scenario_init(
        scenario,
        context,
        extent,
        model_path,
        "models",
        "test_cube.gltf",
        "gltf_shadows",
        true,
        false);
}

static bool gltf_path_scenario_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path,
    const char *default_subdir,
    const char *default_filename,
    const char *name,
    bool shadows_enabled,
    bool stress_camera)
{
    if (!scenario) {
        return false;
    }

    gltf_path_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        return false;
    }

    cfg->tag = GLTF_PATH_CFG_TAG;
    cfg->shadows_enabled = shadows_enabled;
    cfg->stress_camera = stress_camera;

    if (!model_path || model_path[0] == '\0') {
        if (!gltf_fill_default_path(cfg->model_path, sizeof(cfg->model_path), default_subdir, default_filename)) {
            free(cfg);
            return false;
        }
    } else if (snprintf(cfg->model_path, sizeof(cfg->model_path), "%s", model_path) >= (int)sizeof(cfg->model_path)) {
        free(cfg);
        return false;
    }

    memset(scenario, 0, sizeof(*scenario));
    scenario->name = name;
    scenario->setup = gltf_path_setup;
    scenario->teardown = gltf_path_teardown;
    scenario->pre_record = shadows_enabled ? gltf_shadow_pre_record : NULL;
    scenario->record = gltf_record;
    scenario->window_tick = gltf_window_tick;
    scenario->user_data = cfg;

    if (stress_camera) {
        scenario->has_window_camera = true;
        scenario->window_camera = (pb_example_camera_desc){
            .azimuth_rad = 0.6f,
            .elevation_rad = 0.35f,
            .distance = 28.0f,
            .fov_deg = 45.0f,
            .near_plane = 0.1f,
            .far_plane = 200.0f,
        };
    }

    (void)context;
    (void)extent;
    return true;
}

bool pb_bench_scenario_gltf_stress_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path)
{
    return gltf_path_scenario_init(
        scenario,
        context,
        extent,
        model_path,
        "scenes",
        "stress_grid.gltf",
        "gltf_stress",
        false,
        true);
}

bool pb_bench_scenario_gltf_stress_shadows_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *model_path)
{
    return gltf_path_scenario_init(
        scenario,
        context,
        extent,
        model_path,
        "scenes",
        "stress_grid.gltf",
        "gltf_stress_shadows",
        true,
        true);
}
