#include "weapon_psys.h"
#include <raymath.h>
#include <stdio.h>
#include "../state.h"
#include "../util.h"
#include "../enemy.h"


#define MISSING_PSYSUSERPTR fprintf(stderr, "\033[31m(ERROR) '%s': Missing psystem 'userptr'\033[0m\n", __func__)


void weapon_psys_prj_update(
        struct state_t* gst,
        struct psystem_t* psys,
        struct particle_t* part
){
    struct weapon_t* weapon = (struct weapon_t*)psys->userptr;
    if(!weapon) {
        MISSING_PSYSUSERPTR;
        return;
    }

    if(weapon->id >= INVLID_WEAPON_ID) {
        fprintf(stderr, "\033[31m(ERROR) '%s': Invalid weapon id\033[0m\n",
                __func__);
        return;
    }

    Vector3 vel = Vector3Scale(part->velocity, gst->dt * weapon->prj_speed);
    part->position = Vector3Add(part->position, vel);

    Matrix transform = MatrixTranslate(part->position.x, part->position.y, part->position.z);
    *part->transform = transform;

    part->light.position = part->position;
    set_light(gst, &part->light, gst->prj_lights_ubo);



    // Check collision with terrain

    RayCollision t_hit = raycast_terrain(&gst->terrain, part->position.x, part->position.z);

    if(t_hit.point.y >= part->position.y) {

        struct psystem_t* psystem = NULL;
        if(weapon->id == PLAYER_WEAPON_ID) {
            psystem = &gst->psystems[PLAYER_PRJ_ENVHIT_PSYS];
        }
        else
        if(weapon->id == ENEMY_WEAPON_ID) {
            psystem = &gst->psystems[ENEMY_PRJ_ENVHIT_PSYS];
        }

        add_particles(
                gst,
                psystem,
                1,
                part->position,
                (Vector3){0, 0, 0},
                NULL,
                NO_EXTRADATA
                );
        
        disable_particle(gst, part);
        return;
    }


    BoundingBox part_boundingbox = (BoundingBox) {
        (Vector3) { // Min box corner
            part->position.x - weapon->prj_hitbox_size.x/2,
            part->position.y - weapon->prj_hitbox_size.y/2,
            part->position.z - weapon->prj_hitbox_size.z/2
        },
        (Vector3) { // Max box corner
            part->position.x + weapon->prj_hitbox_size.x/2,
            part->position.y + weapon->prj_hitbox_size.y/2,
            part->position.z + weapon->prj_hitbox_size.z/2
        }
    };

    // TODO: Optimize this! <---
   
    if(psys->groupid == PSYS_GROUPID_PLAYER) {
        // Check collision with enemies.
        
        struct enemy_t* enemy = NULL;
        for(size_t i = 0; i < gst->num_enemies; i++) {
            enemy = &gst->enemies[i];
            if(enemy->health <= 0.001) {
                continue;
            }

            if(CheckCollisionBoxes(part_boundingbox, get_enemy_boundingbox(enemy))) {
                enemy_hit(gst, enemy, weapon, part->position, part->velocity);

                add_particles(gst,
                        &gst->psystems[PLAYER_PRJ_ENVHIT_PSYS],
                        1,
                        part->position,
                        (Vector3){0, 0, 0},
                        NULL, NO_EXTRADATA
                        );

                add_particles(gst,
                        &gst->psystems[ENEMY_HIT_PSYS],
                        GetRandomValue(10, 30),
                        part->position,
                        part->velocity,
                        NULL, NO_EXTRADATA
                        );

                disable_particle(gst, part);
            }
        }
    }
    else
    if(psys->groupid == PSYS_GROUPID_ENEMY) {
        // Check collision with player.

        if(CheckCollisionBoxes(part_boundingbox, get_player_boundingbox(&gst->player))) {
            player_hit(gst, &gst->player, weapon);

            add_particles(gst,
                    &gst->psystems[ENEMY_PRJ_ENVHIT_PSYS],
                    1,
                    part->position,
                    (Vector3){0, 0, 0},
                    NULL, NO_EXTRADATA
                    );

            disable_particle(gst, part);
        }
    }

}

void weapon_psys_prj_init(
        struct state_t* gst,
        struct psystem_t* psys, 
        struct particle_t* part,
        Vector3 origin,
        Vector3 velocity,
        void* extradata, int has_extradata
){
    struct weapon_t* weapon = (struct weapon_t*)psys->userptr;
    if(!weapon) {
        MISSING_PSYSUSERPTR;
        return;
    }

    part->velocity = velocity;
    part->position = origin;
    Matrix transform = MatrixTranslate(part->position.x, part->position.y, part->position.z);
    Matrix rotation_m = MatrixRotateXYZ((Vector3){
            RSEEDRANDOMF(-3.0, 3.0), 0.0, RSEEDRANDOMF(-3.0, 3.0)
            });

    transform = MatrixMultiply(transform, rotation_m);
    //add_projectile_light(gst, &part->light, part->position, weapon->color, gst->shaders[DEFAULT_SHADER]);

    part->light = (struct light_t) {
        .enabled = 1,
        .type = LIGHT_POINT,
        .color = weapon->color,
        .strength = 1.0,
        .index = gst->num_prj_lights
        // position is updated later.
    };

    gst->num_prj_lights++;
    if(gst->num_prj_lights >= MAX_PROJECTILE_LIGHTS) {
        gst->num_prj_lights = 0;
    }

    part->has_light = 1;

    *part->transform = transform;
    part->max_lifetime = weapon->prj_max_lifetime;
}
