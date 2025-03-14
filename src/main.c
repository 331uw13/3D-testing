#include <raylib.h>
#include <stdio.h>
#include <math.h>
#include <raymath.h>
#include <time.h>
#include <string.h>

#include "state.h"
#include "input.h"
#include "util.h"
#include "terrain.h"

#include <rlgl.h>

#define SCRN_W 1200
#define SCRN_H 800



void cleanup(struct state_t* gst);

void loop(struct state_t* gst) {

    Model testmodel = LoadModelFromMesh(GenMeshKnot(1.3, 3.5, 64, 64));
    testmodel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gst->textures[GRID4x4_TEXID];
    testmodel.materials[0].shader = gst->shaders[DEFAULT_SHADER];



    gst->scrn_w = GetScreenWidth();
    gst->scrn_h = GetScreenHeight();

    gst->env_render_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
    gst->bloomtreshold_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
    gst->depth_texture = LoadRenderTexture(gst->scrn_w, gst->scrn_h);


    while(!WindowShouldClose()) {

        gst->dt = GetFrameTime();
        gst->time = GetTime();



        {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();
            if((sw != gst->scrn_w) || (sh != gst->scrn_h)) {
                gst->scrn_w = sw;
                gst->scrn_h = sh;
                UnloadRenderTexture(gst->env_render_target);
                UnloadRenderTexture(gst->bloomtreshold_target);
                gst->env_render_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
                gst->bloomtreshold_target = LoadRenderTexture(gst->scrn_w, gst->scrn_h);
              
                printf(" Resized to %ix%i\n", sw, sh);
            }
        }

      
        state_update_frame(gst);
        state_update_shader_uniforms(gst);
        state_render_environment(gst);



        BeginDrawing();
        {
            ClearBackground(BLACK);


            // Finally post process everything.
            //
            BeginShaderMode(gst->shaders[POSTPROCESS_SHADER]);
            {
                rlEnableShader(gst->shaders[POSTPROCESS_SHADER].id);
                
                rlSetUniformSampler(GetShaderLocation(gst->shaders[POSTPROCESS_SHADER],
                            "bloom_treshold_texture"), gst->bloomtreshold_target.texture.id);
                
                rlSetUniformSampler(GetShaderLocation(gst->shaders[POSTPROCESS_SHADER],
                            "depth_texture"), gst->depth_texture.texture.id);
                

                DrawTextureRec(
                        gst->env_render_target.texture,
                        (Rectangle) { 
                            0.0, 0.0, 
                            (float)gst->env_render_target.texture.width,
                            -(float)gst->env_render_target.texture.height
                        },
                        (Vector2){ 0.0, 0.0 },
                        WHITE
                        );
            }
            EndShaderMode();


            // Draw player stats.
            {
                const int bar_width = 150;
                const int bar_height = 10;
            
                int bar_next_y = 10;
                int bar_inc = 15;

                // Health
                {

                    DrawRectangle(
                            10,
                            bar_next_y,
                            bar_width,
                            bar_height + 5,
                            (Color){ 20, 20, 20, 255 }
                            );



                    DrawRectangle(
                            10,
                            bar_next_y,
                            map(gst->player.health, 0.0, gst->player.max_health, 0, bar_width),
                            bar_height + 5,
                            color_lerp(
                                normalize(gst->player.health, 0.0, gst->player.max_health),
                                PLAYER_HEALTH_COLOR_LOW,
                                PLAYER_HEALTH_COLOR_HIGH
                                )
                            );


                }
                bar_next_y += bar_inc+5;

                // Fire rate timer.
                {
                    const float timervalue = gst->player.firerate_timer;

                    DrawRectangle(
                            10,
                            bar_next_y,
                            bar_width,
                            bar_height,
                            (Color){ 20, 20, 20, 255 }
                            );

                    DrawRectangle(
                            10,
                            bar_next_y,
                            map(timervalue, 0.0, gst->player.firerate, 0, bar_width),
                            bar_height,
                            (Color){ 10, 180, 255, 255 }
                            );
                }
                bar_next_y += bar_inc;

                // Weapon temperature.
                {
                    const float tempvalue = gst->player.weapon.temp;

                    DrawRectangle(
                            10,
                            bar_next_y,
                            bar_width,
                            bar_height,
                            (Color){ 20, 20, 20, 255 }
                            );

                    DrawRectangle(
                            10,
                            bar_next_y,
                            map(tempvalue, 0.0, gst->player.weapon.overheat_temp, 0, bar_width),
                            bar_height,
                            (Color){ 255, 58, 30, 255 }
                            );
                }
                bar_next_y += bar_inc;

                // Weapon Accuracy
                {
                    float accvalue 
                        = gst->player.weapon.accuracy - gst->player.accuracy_modifier;
                    
                    DrawRectangle(
                            10,
                            bar_next_y,
                            bar_width,
                            bar_height,
                            (Color){ 20, 20, 20, 255 }
                            );

                    DrawRectangle(
                            10,
                            bar_next_y,
                            map(accvalue, WEAPON_ACCURACY_MIN, WEAPON_ACCURACY_MAX, 0, bar_width),
                            bar_height,
                            (Color){ 255, 200, 30, 255 }
                            );
                }
            }


            // Draw Crosshair if player is aiming.
            if(gst->player.is_aiming) {
                int center_x = GetScreenWidth() / 2;
                int center_y = GetScreenHeight() / 2;
                DrawPixel(center_x, center_y, WHITE);
           
                DrawPixel(center_x-1, center_y, GRAY);
                DrawPixel(center_x, center_y-1, GRAY);
                DrawPixel(center_x+1, center_y, GRAY);
                DrawPixel(center_x, center_y+1, GRAY);
          
                DrawPixel(center_x-2, center_y, GRAY);
                DrawPixel(center_x, center_y-2, GRAY);
                DrawPixel(center_x+2, center_y, GRAY);
                DrawPixel(center_x, center_y+2, GRAY);
            }


            // Some info for player:


            /*
            // TODO: do this with texture...
            if(gst->player.weapon.temp >= (gst->player.weapon.overheat_temp*0.7)) {
                DrawText("(WARNING: OVERHEATING...)", 29.0, gst->scrn_h-149, 20,
                        (Color){ 50, 30, 30, 255 });
                DrawText("(WARNING: OVERHEATING...)", 30.0, gst->scrn_h-150, 20,
                        (Color){ (sin(gst->time*10.0)*0.5+0.5)*125+120, 30, 30, 255 });    
            }
            */


            {
                const char* weapon_text = TextFormat("(x) [%s]", (gst->player.weapon_firetype) ? "SemiAuto" : "FullAuto");
                DrawText(weapon_text,
                        29.0, gst->scrn_h-99, 20, (Color){20, 40, 40, 255});
 
                DrawText(weapon_text,
                        30.0, gst->scrn_h-100, 20, (Color){ 30, 100, 100, 255 });
            } 



            if(gst->debug) {
                int next_y = 200;
                const int y_inc = 25;

                DrawText(TextFormat("X: %0.2f", gst->player.position.x),
                            15.0, next_y, 20, GREEN);
                DrawText(TextFormat("Y: %0.2f", gst->player.position.y),
                            15.0+130.0, next_y, 20, GREEN);
                DrawText(TextFormat("Z: %0.2f", gst->player.position.z),
                            15.0+130.0*2, next_y, 20, GREEN);
   
                next_y += y_inc;
                DrawText(TextFormat("NumVisibleChunks: %i", gst->terrain.num_visible_chunks),
                        15.0, next_y, 20, PURPLE);
                
                DrawText("(Debug ON)", gst->scrn_w - 200, 10, 20, GREEN);
            }


            DrawText(TextFormat("FPS: %i", GetFPS()),
                    gst->scrn_w - 100, gst->scrn_h-30, 20, WHITE);

        }
        EndDrawing();
    }


    UnloadModel(testmodel);
}



void cleanup(struct state_t* gst) {
    
   
    delete_terrain(&gst->terrain);
    delete_player(&gst->player);

    state_delete_all_textures(gst);
    state_delete_all_shaders(gst);
    state_delete_all_psystems(gst);
    state_delete_all_sounds(gst);
    state_delete_all_enemy_models(gst);

    UnloadRenderTexture(gst->env_render_target);
    UnloadRenderTexture(gst->bloomtreshold_target);
    UnloadRenderTexture(gst->depth_texture);

    glDeleteBuffers(1, &gst->lights_ubo);
    glDeleteBuffers(1, &gst->prj_lights_ubo);


    printf("\033[35m -> Cleanup done...\033[0m\n");
    CloseWindow();
}



void first_setup(struct state_t* gst) {

    InitWindow(SCRN_W, SCRN_H, "331uw13's 3D-Game");

    // REMOVE THIS:
    SetWindowPosition(1700, 100);


    // Loading screen.
    BeginDrawing();
    {
        ClearBackground((Color){ 10, 10, 10, 255 });
        DrawText("Loading...", 100, 100, 40, (Color){ 180, 180, 180, 255 });
    }
    EndDrawing();

    
    DisableCursor();
    SetTargetFPS(TARGET_FPS);
    SetTraceLogLevel(LOG_ERROR);
    rlSetClipPlanes(rlGetCullDistanceNear()+0.15, rlGetCullDistanceFar()+3000);

    gst->num_textures = 0;
    gst->debug = 0;
    gst->num_enemies = 0;
    gst->num_prj_lights = 0;
    gst->dt = 0.016;
    gst->num_enemy_weapons = 0;

    memset(gst->fs_unilocs, 0, MAX_FS_UNILOCS * sizeof *gst->fs_unilocs);
    memset(gst->enemies, 0, MAX_ALL_ENEMIES * sizeof *gst->enemies);

    const float terrain_scale = 20.0;
    const u32   terrain_size = 1024;
    const float terrain_amplitude = 20.0;
    const float terrain_pnfrequency = 60.0;
    const int   terrain_octaves = 3;
   
    gst->num_crithit_markers = 0;
    gst->crithit_marker_maxlifetime = 1.5;

    memset(gst->crithit_markers, 0, MAX_RENDER_CRITHITS * sizeof *gst->crithit_markers);


    // --- Load textures ---
    
  
    gst->lights_ubo = 0;

    const size_t lights_ubo_size = MAX_NORMAL_LIGHTS * LIGHT_SHADER_STRUCT_SIZE;
    glGenBuffers(1, &gst->lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, gst->lights_ubo);
    glBufferData(GL_UNIFORM_BUFFER, lights_ubo_size, NULL, GL_STATIC_DRAW);
    
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, gst->lights_ubo);
    glBindBufferRange(GL_UNIFORM_BUFFER, 2, gst->lights_ubo, 0, lights_ubo_size);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);




    gst->prj_lights_ubo = 0;
    const size_t prj_lights_ubo_size = MAX_PROJECTILE_LIGHTS * LIGHT_SHADER_STRUCT_SIZE;
    glGenBuffers(1, &gst->prj_lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, gst->prj_lights_ubo);
    glBufferData(GL_UNIFORM_BUFFER, prj_lights_ubo_size, NULL, GL_STATIC_DRAW);
    
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, gst->prj_lights_ubo);
    glBindBufferRange(GL_UNIFORM_BUFFER, 3, gst->prj_lights_ubo, 0, prj_lights_ubo_size);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);


    state_setup_all_textures(gst);
    state_setup_all_shaders(gst);
    state_setup_all_weapons(gst);
    state_setup_all_psystems(gst);
    state_setup_all_sounds(gst);
    state_setup_all_enemy_models(gst);

    init_player_struct(gst, &gst->player);



    // --- Setup Terrain ----
    {
        init_perlin_noise();
        gst->terrain = (struct terrain_t) { 0 };

        const int terrain_seed = 2010357;//GetRandomValue(0, 9999999);

        printf("(INFO) '%s': Terrain seed = %i\n",
                __func__, terrain_seed);

        generate_terrain(
                gst, &gst->terrain,
                terrain_size,
                terrain_scale,
                terrain_amplitude,
                terrain_pnfrequency,
                terrain_octaves,
                terrain_seed
                );

    }

    int seed = time(0);
    gst->rseed = seed;
    SetRandomSeed(seed);


    // Make sure all lights are disabled.
    for(int i = 0; i < MAX_NORMAL_LIGHTS; i++) {
        struct light_t disabled = {
            .enabled = 0,
            .index = i
        };
        set_light(gst, &disabled, gst->lights_ubo);
    }


    struct light_t SUN = (struct light_t) {
        .type = LIGHT_DIRECTIONAL,
        .enabled = 1,
        .color = (Color){ 240, 210, 200, 255 },
        .position = (Vector3){ -1, 1, -1 },
        .strength = 1.0,
        .index = SUN_LIGHT_ID
    };

    gst->player.gun_light = (struct light_t) {
        .type = LIGHT_POINT,
        .enabled = 1,
        .strength = 0.35,
        .index = PLAYER_GUN_LIGHT_ID
    };


    set_light(gst,  &SUN, gst->lights_ubo);
}

int main(void) {

    struct state_t gst;

    first_setup(&gst);
    loop(&gst);
    cleanup(&gst);


    printf("Exit 0.\n");
    return 0;
}
