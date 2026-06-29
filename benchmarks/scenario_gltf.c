/*
 * Copyright 2026 The Peaberry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_paths.h"
#include "scenario.h"

#include "camera.h"
#include "peaberry/peaberry_gltf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gltf_state {
    pb_bench_scenario *scenario;
    pb_pbr_forward_pass *pass;
    pb_gltf_scene *scene;
    float time_seconds;
} gltf_state;

typedef struct gltf_shadow_config {
    uint32_t tag;
    char model_path[512];
} gltf_shadow_config;

enum {
    GLTF_SHADOW_CFG_TAG = 0x68536c67u,
};

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

static bool gltf_setup_internal(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkRenderPass render_pass,
    VkExtent2D extent,
    const char *model_path,
    bool shadows_enabled)
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
    return gltf_setup_internal(scenario, context, render_pass, extent, model_path, false);
}

static bool gltf_shadows_setup(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkRenderPass render_pass,
    VkExtent2D extent)
{
    if (!scenario || !scenario->user_data) {
        return false;
    }

    gltf_shadow_config cfg = *(gltf_shadow_config *)scenario->user_data;
    free(scenario->user_data);
    scenario->user_data = NULL;

    return gltf_setup_internal(scenario, context, render_pass, extent, cfg.model_path, true);
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

static void gltf_shadows_teardown(pb_bench_scenario *scenario)
{
    if (!scenario || !scenario->user_data) {
        return;
    }

    const gltf_shadow_config *cfg = scenario->user_data;
    if (cfg->tag == GLTF_SHADOW_CFG_TAG) {
        free(scenario->user_data);
        scenario->user_data = NULL;
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
    const gltf_state *state = user_data;
    if (!state || !state->pass || !state->scene) {
        return;
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
    if (!scenario) {
        return false;
    }

    gltf_shadow_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        return false;
    }

    cfg->tag = GLTF_SHADOW_CFG_TAG;

    if (!model_path || model_path[0] == '\0') {
        if (snprintf(cfg->model_path, sizeof(cfg->model_path), "%s/models/test_cube.gltf", PEABERRY_ASSET_DIR) >=
            (int)sizeof(cfg->model_path)) {
            free(cfg);
            return false;
        }
    } else if (snprintf(cfg->model_path, sizeof(cfg->model_path), "%s", model_path) >= (int)sizeof(cfg->model_path)) {
        free(cfg);
        return false;
    }

    memset(scenario, 0, sizeof(*scenario));
    scenario->name = "gltf_shadows";
    scenario->setup = gltf_shadows_setup;
    scenario->teardown = gltf_shadows_teardown;
    scenario->pre_record = gltf_shadow_pre_record;
    scenario->record = gltf_record;
    scenario->window_tick = gltf_window_tick;
    scenario->user_data = cfg;
    (void)context;
    (void)extent;
    return true;
}
