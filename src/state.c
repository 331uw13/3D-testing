#include <stdio.h>
#include <stdlib.h>

#include "state.h"
#include "input.h"
#include "util.h"

#include <raymath.h>
#include <rlgl.h>


#include "particle_systems/weapon_psys.h"
#include "particle_systems/prj_envhit_psys.h"
#include "particle_systems/enemy_hit_psys.h"
#include "particle_systems/fog_effect_psys.h"
#include "particle_systems/player_hit_psys.h"
#include "particle_systems/enemy_explosion_psys.h"
#include "particle_systems/enemy_explosion2_psys.h"
#include "particle_systems/water_splash_psys.h"
#include "particle_systems/enemy_gunfx_psys.h"
#include "particle_systems/prj_envhit2_psys.h"



void state_setup_render_targets(struct state_t* gst) {

    gst->scrn_w = GetScreenWidth();
    gst->scrn_h = GetScreenHeight();

    gst->env_render_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
    gst->bloomtreshold_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
    gst->depth_texture = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
    
}

static void load_texture(struct state_t* gst, const char* filepath, int texid) {
    if(texid >= MAX_TEXTURES) {
        fprintf(stderr, "\033[31m(ERROR) '%s': Texture id is invalid. (%i) for '%s'\033[0m\n",
                __func__, texid, filepath);
        return;
    }
    gst->textures[texid] = LoadTexture(filepath);
    gst->num_textures++;
}

void state_update_shader_uniforms(struct state_t* gst) {

    // --------------------------
    // TODO: OPTIMIZE THIS LATER
    // --------------------------

    // Update Player view position.
    //
    {
        float camposf3[3] = { 
            gst->player.cam.position.x,
            gst->player.cam.position.y,
            gst->player.cam.position.z
        };
        
        SetShaderValue(gst->shaders[DEFAULT_SHADER], 
                gst->shaders[DEFAULT_SHADER].locs[SHADER_LOC_VECTOR_VIEW], camposf3, SHADER_UNIFORM_VEC3);

        SetShaderValue(gst->shaders[FOLIAGE_SHADER], 
                gst->shaders[FOLIAGE_SHADER].locs[SHADER_LOC_VECTOR_VIEW], camposf3, SHADER_UNIFORM_VEC3);

        SetShaderValue(gst->shaders[CRYSTAL_FOLIAGE_SHADER], 
                gst->shaders[CRYSTAL_FOLIAGE_SHADER].locs[SHADER_LOC_VECTOR_VIEW], camposf3, SHADER_UNIFORM_VEC3);
        
        SetShaderValue(gst->shaders[FOG_PARTICLE_SHADER], 
                gst->shaders[FOG_PARTICLE_SHADER].locs[SHADER_LOC_VECTOR_VIEW], camposf3, SHADER_UNIFORM_VEC3);
        
        SetShaderValue(gst->shaders[WATER_SHADER], 
                gst->shaders[WATER_SHADER].locs[SHADER_LOC_VECTOR_VIEW], camposf3, SHADER_UNIFORM_VEC3);
    }

    // Update camera target and position for post processing.
    {
        const Vector3 camdir = Vector3Normalize(Vector3Subtract(gst->player.cam.target, gst->player.cam.position));
        float camtarget3f[3] = {
            camdir.x, camdir.y, camdir.z
        };
        
        SetShaderValue(gst->shaders[POSTPROCESS_SHADER], 
                gst->fs_unilocs[POSTPROCESS_CAMTARGET_FS_UNILOC], camtarget3f, SHADER_UNIFORM_VEC3);
        
        SetShaderValue(gst->shaders[POSTPROCESS_SHADER], 
                gst->fs_unilocs[POSTPROCESS_CAMPOS_FS_UNILOC], &gst->player.cam.position, SHADER_UNIFORM_VEC3);
    }

    // Update screen size.
    {

        const float screen_size[2] = {
            GetScreenWidth(), GetScreenHeight()
        };

        SetShaderValue(
                gst->shaders[POSTPROCESS_SHADER],
                gst->fs_unilocs[POSTPROCESS_SCREENSIZE_FS_UNILOC],
                screen_size,
                SHADER_UNIFORM_VEC2
                );
    }

    // Update time
    {
        SetShaderValue(gst->shaders[FOLIAGE_SHADER], 
                gst->fs_unilocs[FOLIAGE_SHADER_TIME_FS_UNILOC], &gst->time, SHADER_UNIFORM_FLOAT);
        
        SetShaderValue(gst->shaders[CRYSTAL_FOLIAGE_SHADER], 
                gst->fs_unilocs[CRYSTAL_FOLIAGE_SHADER_TIME_FS_UNILOC], &gst->time, SHADER_UNIFORM_FLOAT);
        
        SetShaderValue(gst->shaders[POSTPROCESS_SHADER], 
                gst->fs_unilocs[POSTPROCESS_TIME_FS_UNILOC], &gst->time, SHADER_UNIFORM_FLOAT);
        
        SetShaderValue(gst->shaders[WATER_SHADER], 
                gst->fs_unilocs[WATER_SHADER_TIME_FS_UNILOC], &gst->time, SHADER_UNIFORM_FLOAT);
    }

    // Water level
    SetShaderValue(gst->shaders[DEFAULT_SHADER], 
            GetShaderLocation(gst->shaders[DEFAULT_SHADER], "water_level"), &gst->terrain.water_ylevel, SHADER_UNIFORM_FLOAT);
    
    SetShaderValue(gst->shaders[DEFAULT_SHADER], 
            GetShaderLocation(gst->shaders[DEFAULT_SHADER], "time"), &gst->time, SHADER_UNIFORM_FLOAT);
    
    {

        float color4f[4] = {
            (float)gst->player.weapon.color.r / 255.0,
            (float)gst->player.weapon.color.g / 255.0,
            (float)gst->player.weapon.color.b / 255.0,
            (float)gst->player.weapon.color.a / 255.0
        };

        SetShaderValue(gst->shaders[GUNFX_SHADER], 
                gst->fs_unilocs[GUNFX_SHADER_COLOR_FS_UNILOC], color4f, SHADER_UNIFORM_VEC4);

    
    }
}


// NOTE: DO NOT RENDER FROM HERE:
void state_update_frame(struct state_t* gst) {
    
    handle_userinput(gst);
    
    if(gst->menu_open) {
        return;
    }
    player_update(gst, &gst->player);


    // Enemies.
    for(size_t i = 0; i < gst->num_enemies; i++) {
        update_enemy(gst, &gst->enemies[i]);
    }

    //update_enemy_spawn_system(gst);
    update_natural_item_spawns(gst);

    // (updated only if needed)
    update_psystem(gst, &gst->player.weapon_psys);
    update_psystem(gst, &gst->psystems[ENEMY_LVL0_WEAPON_PSYS]);
    update_psystem(gst, &gst->psystems[PLAYER_PRJ_ENVHIT_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_PRJ_ENVHIT_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_HIT_PSYS]);
    update_psystem(gst, &gst->psystems[FOG_EFFECT_PSYS]);
    update_psystem(gst, &gst->psystems[PLAYER_HIT_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_EXPLOSION_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_EXPLOSION_PART2_PSYS]);
    update_psystem(gst, &gst->psystems[WATER_SPLASH_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_GUNFX_PSYS]);
    update_psystem(gst, &gst->psystems[PLAYER_PRJ_ENVHIT_PART2_PSYS]);
    update_psystem(gst, &gst->psystems[ENEMY_PRJ_ENVHIT_PART2_PSYS]);

    update_inventory(gst, &gst->player);
    update_items(gst);

    // Update xp level.
    if(gst->xp_value_add > 0) {
        gst->xp_update_timer += gst->dt;

        if(gst->xp_update_timer >= 0.035) {
            gst->xp_update_timer = 0.0;

            gst->player.xp++;
            gst->xp_value_add--;

        }

    }

   
}




void state_setup_all_shaders(struct state_t* gst) {

    SetTraceLogLevel(LOG_ALL);

    // --- Setup Default Shader ---
    {
        Shader* shader = &gst->shaders[DEFAULT_SHADER];
        load_shader(
                "res/shaders/default.vs",
                "res/shaders/default.fs", shader);
        
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(*shader, "matModel");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
    }


    // --- Setup Post Processing Shader ---
    {
        Shader* shader = &gst->shaders[POSTPROCESS_SHADER];
        load_shader(
                "res/shaders/default.vs",
                "res/shaders/postprocess.fs", shader);
        /*
        *shader = LoadShader(
            0, 
            "res/shaders/postprocess.fs"
        );
        */

        gst->fs_unilocs[POSTPROCESS_TIME_FS_UNILOC] = GetShaderLocation(*shader, "time");
        gst->fs_unilocs[POSTPROCESS_SCREENSIZE_FS_UNILOC] = GetShaderLocation(*shader, "screen_size");
        gst->fs_unilocs[POSTPROCESS_PLAYER_HEALTH_FS_UNILOC] = GetShaderLocation(*shader, "health");
        gst->fs_unilocs[POSTPROCESS_CAMTARGET_FS_UNILOC] = GetShaderLocation(*shader, "cam_target");
        gst->fs_unilocs[POSTPROCESS_CAMPOS_FS_UNILOC] = GetShaderLocation(*shader, "cam_pos");
    }


    // --- Setup PRJ_ENVHIT_PSYS_SHADER ---
    {
        Shader* shader = &gst->shaders[PRJ_ENVHIT_PSYS_SHADER];
        load_shader(
                "res/shaders/instance_core.vs",
                "res/shaders/prj_envhit_psys.fs", shader);
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
    }
 

    // --- Setup PLAYER_WEAPON_PSYS_SHADER ---
    {
        Shader* shader = &gst->shaders[BASIC_WEAPON_PSYS_SHADER];
        load_shader(
                "res/shaders/instance_core.vs",
                "res/shaders/basic_weapon_psys.fs", shader);
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
    }
 


    // --- Setup FOLIAGE_SHADER ---
    {
        Shader* shader = &gst->shaders[FOLIAGE_SHADER];
        load_shader(
                "res/shaders/instance_core.vs",
                "res/shaders/foliage.fs", shader);
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
  
        gst->fs_unilocs[FOLIAGE_SHADER_TIME_FS_UNILOC] = GetShaderLocation(*shader, "time");
    }

    // --- Setup CRYSTAL_FOLIAGE_SHADER ---
    {
        Shader* shader = &gst->shaders[CRYSTAL_FOLIAGE_SHADER];
        load_shader(
                "res/shaders/instance_core.vs",
                "res/shaders/crystal.fs", shader);
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
  
        gst->fs_unilocs[CRYSTAL_FOLIAGE_SHADER_TIME_FS_UNILOC] = GetShaderLocation(*shader, "time");
    }
    // --- Setup FOG_EFFECT_PARTICLE_SHADER ---
    {
        Shader* shader = &gst->shaders[FOG_PARTICLE_SHADER];
        load_shader(
                "res/shaders/instance_core.vs",
                "res/shaders/fog_particle.fs", shader);
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
    }


    // --- Setup Water Shader ---
    {
        Shader* shader = &gst->shaders[WATER_SHADER];
        load_shader(
                "res/shaders/default.vs",
                "res/shaders/water.fs", shader);
        
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(*shader, "matModel");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
       
        gst->fs_unilocs[WATER_SHADER_TIME_FS_UNILOC] = GetShaderLocation(*shader, "time");
    }


    // --- Setup Bloom Treshold Shader ---
    {
        Shader* shader = &gst->shaders[BLOOM_TRESHOLD_SHADER];
        *shader = LoadShader(
            0, /* use raylibs default vertex shader */
            "res/shaders/bloom_treshold.fs"
        );
    }


    // --- Setup depth value shader ---
    {
        Shader* shader = &gst->shaders[WDEPTH_SHADER];
        *shader = LoadShader(
            "res/shaders/write_depth.vs", 
            "res/shaders/write_depth.fs"
        );

    }

    // --- Setup depth value shader for instanced drawing ---
    {
        Shader* shader = &gst->shaders[WDEPTH_INSTANCE_SHADER];
        *shader = LoadShader(
            "res/shaders/write_instance_depth.vs", 
            "res/shaders/write_depth.fs"
        );

        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
        
    }



    // --- Setup Gun FX Shader ---
    {
        Shader* shader = &gst->shaders[GUNFX_SHADER];
        *shader = LoadShader(
            "res/shaders/default.vs", /* use raylibs default vertex shader */ 
            "res/shaders/gun_fx.fs"
        );

        
        gst->fs_unilocs[GUNFX_SHADER_COLOR_FS_UNILOC] = GetShaderLocation(*shader, "gun_color");
    }

    // --- Setup Enemy Gun FX Shader ---
    {
        Shader* shader = &gst->shaders[ENEMY_GUNFX_SHADER];
        *shader = LoadShader(
            "res/shaders/instance_core.vs",
            "res/shaders/gun_fx.fs"
        );
       
        shader->locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(*shader, "mvp");
        shader->locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(*shader, "viewPos");
        shader->locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(*shader, "instanceTransform");
        gst->fs_unilocs[ENEMY_GUNFX_SHADER_COLOR_FS_UNILOC] = GetShaderLocation(*shader, "gun_color");

        float color4f[4] = {
            (float)ENEMY_WEAPON_COLOR.r / 255.0,
            (float)ENEMY_WEAPON_COLOR.g / 255.0,
            (float)ENEMY_WEAPON_COLOR.b / 255.0,
            (float)ENEMY_WEAPON_COLOR.a / 255.0
        };

        SetShaderValue(gst->shaders[ENEMY_GUNFX_SHADER], 
                gst->fs_unilocs[ENEMY_GUNFX_SHADER_COLOR_FS_UNILOC], color4f, SHADER_UNIFORM_VEC4);
    }


    


    SetTraceLogLevel(LOG_NONE);
}

void state_setup_all_weapons(struct state_t* gst) {

   
    // Player's weapon.
    gst->player.weapon = (struct weapon_t) {
        .id = PLAYER_WEAPON_ID,
        .accuracy = 7.85,
        .damage = 10.0,
        .critical_chance = 10,
        .critical_mult = 1.85,
        .prj_speed = 530.0,
        .prj_max_lifetime = 5.0,
        .prj_hitbox_size = (Vector3) { 1.0, 1.0, 1.0 },
        .color = (Color) { 20, 255, 200, 255 },
        
        // TODO -----
        .overheat_temp = 100.0,
        .heat_increase = 2.0,
        .cooling_level = 20.0
    };
 

    // Enemy lvl0 weapon.
    gst->enemy_weapons[ENEMY_LVL0_WEAPON] = (struct weapon_t) {
        .id = ENEMY_WEAPON_ID,
        .accuracy = 9.25,
        .damage = 15.0,
        .critical_chance = 15,
        .critical_mult = 5.0,
        .prj_speed = 530.0,
        .prj_max_lifetime = 5.0,
        .prj_hitbox_size = (Vector3) { 1.5, 1.5, 1.5 },
        .color = ENEMY_WEAPON_COLOR,
        .overheat_temp = -1,
    };

    // Enemy lvl1 weapon.
    gst->enemy_weapons[ENEMY_LVL1_WEAPON] = (struct weapon_t) {
        .id = ENEMY_WEAPON_ID,
        .accuracy = 9.835,
        .damage = 25.0,
        .critical_chance = 5,
        .critical_mult = 7.0,
        .prj_speed = 530.0,
        .prj_max_lifetime = 5.0,
        .prj_hitbox_size = (Vector3) { 1.5, 1.5, 1.5 },
        .color = ENEMY_WEAPON_COLOR,
        .overheat_temp = -1,
    };
}

void state_setup_all_psystems(struct state_t* gst) {

    // Create PLAYER_WEAPON_PSYS
    { // (projectile particle system for player)
        struct psystem_t* psystem = &gst->player.weapon_psys;
        create_psystem(
                gst,
                PSYS_GROUPID_PLAYER,
                PSYS_ONESHOT,
                psystem,
                64,
                weapon_psys_prj_update,
                weapon_psys_prj_init,
                BASIC_WEAPON_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(1.25, 16, 16);
        psystem->userptr = &gst->player.weapon;
    }

    // Create PLAYER_PRJ_ENVHIT_PSYS.
    { // (when player's projectile hits environment)
        struct psystem_t* psystem = &gst->psystems[PLAYER_PRJ_ENVHIT_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_PLAYER,
                PSYS_ONESHOT,
                psystem,
                64,
                prj_envhit_psys_update,
                prj_envhit_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.5, 32, 32);
        psystem->userptr = &gst->player.weapon;
    }

    // Create ENEMY_LVL0_WEAPON_PSYS.
    { // (projectile particle system for enemy lvl 0)
        struct psystem_t* psystem = &gst->psystems[ENEMY_LVL0_WEAPON_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENEMY,
                PSYS_ONESHOT,
                psystem,
                512,
                weapon_psys_prj_update,
                weapon_psys_prj_init,
                BASIC_WEAPON_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(1.5, 16, 16);
        psystem->userptr = &gst->enemy_weapons[ENEMY_LVL0_WEAPON];
    }

    // Create ENEMY_PRJ_ENVHIT_PSYS.
    { // (when enemies projectile hits environment)
        struct psystem_t* psystem = &gst->psystems[ENEMY_PRJ_ENVHIT_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENEMY,
                PSYS_ONESHOT,
                psystem,
                64,
                prj_envhit_psys_update,
                prj_envhit_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.5, 32, 32);
        psystem->userptr = &gst->player.weapon;
    }

    // Create PLAYER_PRJ_ENVHIT_PART2_PSYS. (Extra Effect)
    { // (when projectile hits environment)
        struct psystem_t* psystem = &gst->psystems[PLAYER_PRJ_ENVHIT_PART2_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_PLAYER,
                PSYS_ONESHOT,
                psystem,
                64,
                prj_envhit_part2_psys_update,
                prj_envhit_part2_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.5, 16, 16);
    }

    // Create ENEMY_PRJ_ENVHIT_PART2_PSYS. (Extra Effect)
    { // (when projectile hits environment)
        struct psystem_t* psystem = &gst->psystems[ENEMY_PRJ_ENVHIT_PART2_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENEMY,
                PSYS_ONESHOT,
                psystem,
                64,
                prj_envhit_part2_psys_update,
                prj_envhit_part2_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.5, 16, 16);
    }

    // Create ENEMY_HIT_PSYS.
    { // (when enemy gets hit)
        struct psystem_t* psystem = &gst->psystems[ENEMY_HIT_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                512,
                enemy_hit_psys_update,
                enemy_hit_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.8, 8, 8);
        psystem->userptr = &gst->player.weapon;
    }

    // Create FOG_EFFECT_PSYS.
    { // (environment effect)

        const int num_fog_effect_parts = 700;
        struct psystem_t* psystem = &gst->psystems[FOG_EFFECT_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_CONTINUOUS,
                psystem,
                num_fog_effect_parts,
                fog_effect_psys_update,
                fog_effect_psys_init,
                FOG_PARTICLE_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.5, 4, 4);
        add_particles(gst, psystem, num_fog_effect_parts, (Vector3){0}, (Vector3){0}, NULL, NO_EXTRADATA);
    }

    // Create PLAYER_HIT_PSYS.
    { // (when player gets hit)
        struct psystem_t* psystem = &gst->psystems[PLAYER_HIT_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                256,
                player_hit_psys_update,
                player_hit_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.45, 8, 8);
        psystem->userptr = &gst->player.weapon;
    }


    // Create WATER_SPLASH_PSYS.
    { // (when something hits water)
        struct psystem_t* psystem = &gst->psystems[WATER_SPLASH_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                256,
                water_splash_psys_update,
                water_splash_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.45, 8, 8);
        psystem->userptr = &gst->player.weapon;
    }

    // Create ENEMY_EXPLOSION_PSYS.
    { // (when enemy dies it "explodes")
        struct psystem_t* psystem = &gst->psystems[ENEMY_EXPLOSION_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                5000,
                enemy_explosion_psys_update,
                enemy_explosion_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(0.6, 8, 8);
    }

    // Create ENEMY_EXPLOSION_PART2_PSYS.
    { // (when enemy dies it "explodes")
        struct psystem_t* psystem = &gst->psystems[ENEMY_EXPLOSION_PART2_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                16,
                enemy_explosion_part2_psys_update,
                enemy_explosion_part2_psys_init,
                PRJ_ENVHIT_PSYS_SHADER
                );

        psystem->particle_mesh = GenMeshSphere(1.0, 32, 32);
    }


    // Create ENEMY_GUNFX_PSYS.
    { // (gun fx)
        struct psystem_t* psystem = &gst->psystems[ENEMY_GUNFX_PSYS];
        create_psystem(
                gst,
                PSYS_GROUPID_ENV,
                PSYS_ONESHOT,
                psystem,
                64,
                enemy_gunfx_psys_update,
                enemy_gunfx_psys_init,
                ENEMY_GUNFX_SHADER
                );

        //psystem->particle_mesh = GenMeshSphere(0.6, 8, 8);
        psystem->particle_mesh = GenMeshPlane(1.0, 1.0, 1, 1);
        psystem->particle_material.maps[MATERIAL_MAP_DIFFUSE].texture 
            = gst->textures[GUNFX_TEXID];
    }
}

void state_setup_all_textures(struct state_t* gst) {

    SetTraceLogLevel(LOG_ALL);

    load_texture(gst, "res/textures/grid_4x4.png", GRID4x4_TEXID);
    load_texture(gst, "res/textures/grid_6x6.png", GRID6x6_TEXID);
    load_texture(gst, "res/textures/grid_9x9.png", GRID9x9_TEXID);
    load_texture(gst, "res/textures/gun_0.png", GUN_0_TEXID);
    load_texture(gst, "res/textures/enemy_lvl0.png", ENEMY_LVL0_TEXID);
    load_texture(gst, "res/textures/arms.png", PLAYER_ARMS_TEXID);
    load_texture(gst, "res/textures/critical_hit.png", CRITICALHIT_TEXID);
    load_texture(gst, "res/textures/tree_bark.png", TREEBARK_TEXID);
    load_texture(gst, "res/textures/leaf.jpg", LEAF_TEXID);
    load_texture(gst, "res/textures/rock_type0.jpg", ROCK_TEXID);
    load_texture(gst, "res/textures/moss2.png", MOSS_TEXID);
    load_texture(gst, "res/textures/grass.png", GRASS_TEXID);
    load_texture(gst, "res/textures/gun_fx.png", GUNFX_TEXID);
    load_texture(gst, "res/textures/apple_inv.png", APPLE_INV_TEXID);
    load_texture(gst, "res/textures/apple.png", APPLE_TEXID);
    load_texture(gst, "res/textures/blue_metal.png", METALPIECE_TEXID);
    load_texture(gst, "res/textures/metalpiece_inv.png", METALPIECE_INV_TEXID);
    load_texture(gst, "res/textures/cloth.png", PLAYER_HANDS_TEXID);
    load_texture(gst, "res/textures/player_skin.png", PLAYER_SKIN_TEXID);
    load_texture(gst, "res/textures/gun_metal.png", METAL2_TEXID);
    
    SetTraceLogLevel(LOG_NONE);

   
    SetTextureWrap(gst->textures[LEAF_TEXID], TEXTURE_WRAP_MIRROR_REPEAT);
    SetTextureWrap(gst->textures[ROCK_TEXID], TEXTURE_WRAP_MIRROR_REPEAT);
    SetTextureWrap(gst->textures[MOSS_TEXID], TEXTURE_WRAP_MIRROR_REPEAT);
    SetTextureWrap(gst->textures[GRASS_TEXID], TEXTURE_WRAP_CLAMP);
}


void state_setup_all_sounds(struct state_t* gst) {
    gst->has_audio = 0;
    InitAudioDevice();

    if(!IsAudioDeviceReady()) {
        fprintf(stderr, "\033[31m(ERROR) '%s': Failed to initialize audio device\033[0m\n",
                __func__);
        return;
    }

    gst->sounds[PLAYER_GUN_SOUND] = LoadSound("res/audio/player_gun.wav");
    gst->sounds[ENEMY_HIT_SOUND_0] = LoadSound("res/audio/enemy_hit_0.wav");
    gst->sounds[ENEMY_HIT_SOUND_1] = LoadSound("res/audio/enemy_hit_1.wav");
    gst->sounds[ENEMY_HIT_SOUND_2] = LoadSound("res/audio/enemy_hit_2.wav");
    gst->sounds[ENEMY_GUN_SOUND] = LoadSound("res/audio/enemy_gun.wav");
    gst->sounds[PRJ_ENVHIT_SOUND] = LoadSound("res/audio/envhit.wav");
    gst->sounds[PLAYER_HIT_SOUND] = LoadSound("res/audio/playerhit.wav");
    gst->sounds[ENEMY_EXPLOSION_SOUND] = LoadSound("res/audio/enemy_explosion.wav");
    

    SetMasterVolume(30.0);
    gst->has_audio = 1;
}




void state_setup_all_enemy_models(struct state_t* gst) {

    for(size_t i = 0; i < MAX_ALL_ENEMIES; i++) {
        gst->enemies[i].alive = 0;
    }

    load_enemy_model(gst, ENEMY_LVL0, "res/models/enemy_lvl0.glb", ENEMY_LVL0_TEXID);
    load_enemy_model(gst, ENEMY_LVL1, "res/models/enemy_lvl1.glb", GRID9x9_TEXID);

    // ...
}

void state_setup_all_item_models(struct state_t* gst) {
    for(size_t i = 0; i < MAX_ALL_ITEMS; i++) {
        gst->items[i].enabled = 0;
    }
    
    load_item_model(gst, ITEM_APPLE, "res/models/apple.glb", APPLE_TEXID);
    load_item_model(gst, ITEM_METALPIECE, "res/models/metal_piece.glb", METALPIECE_TEXID);
    
    // ...

}


void state_delete_all_shaders(struct state_t* gst) {
    for(int i = 0; i < MAX_SHADERS; i++) {
        UnloadShader(gst->shaders[i]);
    }
    
    printf("\033[35m -> Deleted all Shaders\033[0m\n");
}

void state_delete_all_psystems(struct state_t* gst) {
    delete_psystem(&gst->player.weapon_psys);
    for(int i = 0; i < MAX_PSYSTEMS; i++) {
        delete_psystem(&gst->psystems[i]);
    }
   
    printf("\033[35m -> Deleted all Particle systems\033[0m\n");
}


void state_delete_all_textures(struct state_t* gst) {
    // Delete all textures.
    for(unsigned int i = 0; i < gst->num_textures; i++) {
        UnloadTexture(gst->textures[i]);
    }

    printf("\033[35m -> Deleted all Textures\033[0m\n");
}

void state_delete_all_sounds(struct state_t* gst) {
    if(gst->has_audio) {
        for(int i = 0; i < MAX_SOUNDS; i++) {
            UnloadSound(gst->sounds[i]);
        }    
    }
    if(IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    
    printf("\033[35m -> Deleted all Sounds\033[0m\n");
}

void state_delete_all_enemy_models(struct state_t* gst) {

    for(int i = 0; i < MAX_ENEMY_MODELS; i++) {
        if(IsModelValid(gst->enemy_models[i])) {
            UnloadModel(gst->enemy_models[i]);
        }
    }

    printf("\033[35m -> Deleted all Enemy models\033[0m\n");
}

void state_delete_all_item_models(struct state_t* gst) {

    for(int i = 0; i < MAX_ITEM_MODELS; i++) {
        if(IsModelValid(gst->item_models[i])) {
            UnloadModel(gst->item_models[i]);
        }
    }


    printf("\033[35m -> Deleted all Item models\033[0m\n");
}


void state_add_crithit_marker(struct state_t* gst, Vector3 position) {
    struct crithit_marker_t* marker = &gst->crithit_markers[gst->num_crithit_markers];

    marker->visible = 1;
    marker->lifetime = 0.0;
    marker->position = position;

    const float r = 10.0;
    marker->position.x += RSEEDRANDOMF(-r, r);
    marker->position.z += RSEEDRANDOMF(-r, r);
    marker->position.y += RSEEDRANDOMF(r*1.5, r*2)*0.2;

    gst->num_crithit_markers++;
    if(gst->num_crithit_markers >= MAX_RENDER_CRITHITS) {
        gst->num_crithit_markers = 0;
    }
}

void update_fog_settings(struct state_t* gst) {
    glBindBuffer(GL_UNIFORM_BUFFER, gst->fog_ubo);

    gst->render_bg_color = (Color) {
        gst->fog_color_far.r * 0.6,
        gst->fog_color_far.g * 0.6,
        gst->fog_color_far.b * 0.6,
        255
    };


    int max_far = 100;
    gst->fog_color_far.r = CLAMP(gst->fog_color_far.r, 0, max_far);
    gst->fog_color_far.g = CLAMP(gst->fog_color_far.g, 0, max_far);
    gst->fog_color_far.b = CLAMP(gst->fog_color_far.b, 0, max_far);

    float near_color[4] = {
        (float)gst->fog_color_near.r / 255.0,
        (float)gst->fog_color_near.g / 255.0,
        (float)gst->fog_color_near.b / 255.0,
        1.0
    };

    float far_color[4] = {
        (float)gst->fog_color_far.r / 255.0,
        (float)gst->fog_color_far.g / 255.0,
        (float)gst->fog_color_far.b / 255.0,
        1.0
    };

    size_t offset;
    size_t size;
    size = sizeof(float)*4;

    // DENSITY
   
    float density[4] = {
        // Convert fog density to more friendly scale.
        // Otherwise it is very exponental with very very samll numbers.
        map(log(CLAMP(gst->fog_density, FOG_MIN, FOG_MAX)), FOG_MIN, FOG_MAX, 0.0015, 0.02),
        0.0,
        0.0,  // Maybe adding more settings later.
        0.0
    };
    offset = 0;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, density);

    // NEAR COLOR
    offset = 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, near_color);

    // FAR COLOR
    offset = 16*2;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, far_color);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}



