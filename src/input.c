#include <raylib.h>
#include <rcamera.h>
#include <raymath.h>
#include <stdio.h>

#include "state.h"


#define CLAMP(v, min, max) ((v < min) ? min : (v > max) ? max : v)





void handle_userinput(struct state_t* gst) {

    Vector2 md = GetMouseDelta();
    float dt = GetFrameTime();

    CameraYaw(&gst->player.cam, (-md.x * CAMERA_SENSETIVITY) * dt, 0);
    CameraPitch(&gst->player.cam, (-md.y * CAMERA_SENSETIVITY) * dt, 1, 0, 0);

    gst->player.cam_yaw = (-md.x * CAMERA_SENSETIVITY) * dt;
    gst->player.looking_at = Vector3Normalize(Vector3Subtract(gst->player.cam.target, gst->player.cam.position));

    
    float camspeed = gst->player.walkspeed;

    if(IsKeyDown(KEY_LEFT_SHIFT) && !gst->player.is_aiming) {
        camspeed *= gst->player.run_mult;
    }
    
    camspeed *= dt;
    
    if(IsKeyDown(KEY_W)) {
        gst->player.velocity.z += camspeed;
    }
    if(IsKeyDown(KEY_S)) {
        gst->player.velocity.z -= camspeed;
    }
    if(IsKeyDown(KEY_A)) {
        gst->player.velocity.x -= camspeed;
    }
    if(IsKeyDown(KEY_D)) {
        gst->player.velocity.x += camspeed;
    }


    // ----- Handle player Y movement -------
    if(!gst->player.noclip) {
        if(IsKeyPressed(KEY_SPACE) && gst->player.onground) {

            gst->player.velocity.y = 0;
            gst->player.velocity.y += gst->player.jump_force;
            
            gst->player.num_jumps_inair++;
            if(gst->player.num_jumps_inair >= gst->player.max_jumps) {
                gst->player.onground = 0;
            }
        }

        gst->player.velocity.y = CLAMP(gst->player.velocity.y,
                -5.0, 5.0);

        CameraMoveUp(&gst->player.cam, gst->player.velocity.y);

        if(gst->player.cam.position.y > 2.1) {
            gst->player.velocity.y -= gst->player.gravity * dt;
        }
        else {
            // Player is on ground
            gst->player.onground = 1;
            gst->player.velocity.y = 0.0;
            gst->player.num_jumps_inair = 0;
        }
    }
    else {
        if(IsKeyDown(KEY_SPACE)) {
            CameraMoveUp(&gst->player.cam, dt * 15.0);
        }
        else if(IsKeyDown(KEY_LEFT_CONTROL)) {
            CameraMoveUp(&gst->player.cam, -(dt * 18.0));
        }
    }



    // X Z  movement.

    const float vmax = 1.0;

    gst->player.velocity.z = CLAMP(gst->player.velocity.z, -vmax, vmax);
    gst->player.velocity.x = CLAMP(gst->player.velocity.x, -vmax, vmax);

    CameraMoveForward(&gst->player.cam, gst->player.velocity.z, 1);
    CameraMoveRight(&gst->player.cam, gst->player.velocity.x, 1);
    


    const float f = (1.0 - gst->player.friction);
    gst->player.velocity.z *= f;
    gst->player.velocity.x *= f;


    gst->player.position = gst->player.cam.position;


    // ----- User interaction ---------

    if(IsKeyPressed(KEY_Y)) {
        gst->draw_debug = !gst->draw_debug;
    }

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        player_shoot(gst, &gst->player);
    }

    if(IsKeyPressed(KEY_G)) {
        gst->player.noclip = !gst->player.noclip;
    }

    gst->player.is_aiming = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

}







