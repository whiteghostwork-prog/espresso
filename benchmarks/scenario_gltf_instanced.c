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

enum {
    PB_BENCH_INSTANCED_DEFAULT_COUNT = 64,
};

typedef struct gltf_instanced_config {
    char model_path[512];
    uint32_t instance_count;
} gltf_instanced_config;

typedef struct gltf_instanced_state {
    pb_bench_scenario *scenario;
    pb_pbr_forward_pass *pass;
    pb_gltf_scene *scene;
    pb_mat4 *transforms;
    uint32_t instance_count;
    float time_seconds;
} gltf_instanced_state;

static bool parse_instanced_config(const char *arg, gltf_instanced_config *cfg)
{
    cfg->instance_count = PB_BENCH_INSTANCED_DEFAULT_COUNT;
    if (snprintf(cfg->model_path, sizeof(cfg->model_path), "%s/models/test_cube.gltf", PEABERRY_ASSET_DIR) >=
        (int)sizeof(cfg->model_path)) {
        return false;
    }

    if (!arg || arg[0] == '\0') {
        return true;
    }

    char *end = NULL;
    const unsigned long count = strtoul(arg, &end, 10);
    if (end && *end == '\0' && count > 0 && count <= UINT32_MAX) {
        cfg->instance_count = (uint32_t)count;
        return true;
    }

    if (snprintf(cfg->model_path, sizeof(cfg->model_path), "%s", arg) >= (int)sizeof(cfg->model_path)) {
        return false;
    }

    return true;
}

static bool build_grid_transforms(pb_mat4 *transforms, uint32_t instance_count)
{
    const uint32_t side = (uint32_t)ceilf(sqrtf((float)instance_count));
    const float spacing = 2.0f;
    uint32_t placed = 0;

    for (uint32_t z = 0; z < side && placed < instance_count; ++z) {
        for (uint32_t x = 0; x < side && placed < instance_count; ++x) {
            pb_mat4_identity(transforms[placed]);
            transforms[placed][3][0] = ((float)x - (float)(side - 1) * 0.5f) * spacing;
            transforms[placed][3][2] = ((float)z - (float)(side - 1) * 0.5f) * spacing;
            ++placed;
        }
    }

    return placed == instance_count;
}

static uint32_t gltf_instanced_index_count(const pb_gltf_scene *scene, uint32_t draw_index, uint32_t instances)
{
    pb_gltf_draw_info draw = {0};
    if (!pb_gltf_scene_get_draw(scene, draw_index, &draw)) {
        return 0;
    }

    return draw.index_count * instances;
}

static bool gltf_instanced_setup(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkRenderPass render_pass,
    VkExtent2D extent)
{
    if (!scenario || !scenario->user_data) {
        return false;
    }

    gltf_instanced_config cfg = *(gltf_instanced_config *)scenario->user_data;
    free(scenario->user_data);
    scenario->user_data = NULL;

    char vert_spv[512];
    char frag_spv[512];
    if (snprintf(vert_spv, sizeof(vert_spv), "%s/pbr_forward.vert.spv", PEABERRY_SHADER_DIR) >= (int)sizeof(vert_spv) ||
        snprintf(frag_spv, sizeof(frag_spv), "%s/pbr_forward.frag.spv", PEABERRY_SHADER_DIR) >= (int)sizeof(frag_spv)) {
        return false;
    }

    gltf_instanced_state *state = calloc(1, sizeof(*state));
    if (!state) {
        return false;
    }

    state->instance_count = cfg.instance_count;
    state->transforms = calloc(state->instance_count, sizeof(*state->transforms));
    if (!state->transforms || !build_grid_transforms(state->transforms, state->instance_count)) {
        free(state->transforms);
        free(state);
        return false;
    }

    state->scene = pb_gltf_scene_create(
        &(pb_gltf_scene_desc){
            .context = context,
            .path = cfg.model_path,
            .scene_index = PB_GLTF_SCENE_INDEX_DEFAULT,
        });
    if (!state->scene || pb_gltf_scene_draw_count(state->scene) == 0) {
        fprintf(stderr, "failed to load glTF model for instancing: %s\n", cfg.model_path);
        free(state->transforms);
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
        free(state->transforms);
        free(state);
        return false;
    }

    if (!pb_pbr_forward_pass_set_instanced_draw(
            state->pass,
            0,
            state->transforms,
            state->instance_count)) {
        pb_pbr_forward_pass_destroy(state->pass);
        pb_gltf_scene_destroy(state->scene);
        free(state->transforms);
        free(state);
        return false;
    }

    scenario->user_data = state;
    state->scenario = scenario;
    scenario->info.draw_calls = 1;
    scenario->info.visible_draw_calls = 1;
    scenario->info.index_count = gltf_instanced_index_count(state->scene, 0, state->instance_count);
    scenario->info.material_count = pb_gltf_scene_material_count(state->scene);
    scenario->info.pixels_shaded = extent.width * extent.height;
    (void)render_pass;
    return true;
}

static void gltf_instanced_teardown(pb_bench_scenario *scenario)
{
    gltf_instanced_state *state = scenario ? scenario->user_data : NULL;
    if (!state) {
        return;
    }

    if (state->pass) {
        pb_pbr_forward_pass_destroy(state->pass);
    }
    if (state->scene) {
        pb_gltf_scene_destroy(state->scene);
    }

    free(state->transforms);
    free(state);
    scenario->user_data = NULL;
}

static void gltf_instanced_pre_record(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    const gltf_instanced_state *state = user_data;
    if (!state || !state->pass || !state->scene) {
        return;
    }

    pb_pbr_forward_pass_record_shadow_map(state->pass, cmd, extent, state->scene);
}

static void gltf_instanced_apply_window_motion(gltf_instanced_state *state, const pb_bench_window_tick *tick)
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
}

static void gltf_instanced_window_tick(pb_bench_scenario *scenario, const pb_bench_window_tick *tick)
{
    gltf_instanced_apply_window_motion(scenario ? scenario->user_data : NULL, tick);
}

static void gltf_instanced_record(VkCommandBuffer cmd, VkExtent2D extent, void *user_data)
{
    const gltf_instanced_state *state = user_data;
    if (!state || !state->pass || !state->scene) {
        return;
    }

    pb_pbr_forward_pass_record(state->pass, cmd, extent, state->scene, state->time_seconds);

    if (state->scenario) {
        state->scenario->info.visible_draw_calls = pb_pbr_forward_pass_last_visible_draw_count(state->pass);
    }
}

bool pb_bench_scenario_gltf_instanced_init(
    pb_bench_scenario *scenario,
    pb_context *context,
    VkExtent2D extent,
    const char *scenario_arg)
{
    if (!scenario) {
        return false;
    }

    gltf_instanced_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg || !parse_instanced_config(scenario_arg, cfg)) {
        free(cfg);
        return false;
    }

    memset(scenario, 0, sizeof(*scenario));
    scenario->name = "gltf_instanced";
    scenario->setup = gltf_instanced_setup;
    scenario->teardown = gltf_instanced_teardown;
    scenario->pre_record = gltf_instanced_pre_record;
    scenario->record = gltf_instanced_record;
    scenario->window_tick = gltf_instanced_window_tick;
    scenario->user_data = cfg;
    (void)context;
    (void)extent;
    return true;
}
