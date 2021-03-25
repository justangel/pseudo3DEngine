//
// Created by ivan- on 18.01.2020.
//

#ifndef PSEUDO3DENGINE_SETTINGS_H
#define PSEUDO3DENGINE_SETTINGS_H

#include <SFML/Graphics.hpp>

#define PI 3.1415926535897932384626433832795

#define SCALE 40 //How much pixels = 1 unit of distance measure

#define SCALE_WINDOW 500

// NETWORK
#define NETWORK_VERSION 2U
#define TIMEOUT_SECONDS 5
#define WORLD_UPDATE_RATE 30
#define RELIABLE_RETRY_TIME (1.0/20)

// OBJECTS
#define OUTLINE_THICKNESS 3
#define OUTLINE_COLOR {0, 0, 0, 100}
#define FILL_COLOR {255, 174, 174, 255}

#define CIRCLE_CONVEX_NUMBER 10

// CAMERA
#define OUTLINE_CAMERA_THICKNESS 1
#define OUTLINE_CAMERA_COLOR {0, 0, 0, 255}
#define FILL_CAMERA_COLOR {255, 67, 67, 255}
#define RADIUS_CAMERA SCALE/10
#define CONVEX_NUMBER 100
#define FILED_OF_VEW_COLOR {255, 67, 67, 0}
#define RED_COLOR {255, 0, 0}
#define SKIN "../texture/hero.png"

// FOR COLLISION DETECTION
#define COLLISION_DISTANCE 0.1
#define COLLISION_AREA 1

#define OPTICAL_HEIGHT 100

//RENDER SETTINGS

#define FILL_RENDER_COLOR {255, 255, 255, 255}

// SCREEN SIZE
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define DIS_SEG_FOR_BOTS 8 // How much vertical sectors we use in ray cast
#define DISTANCES_SEGMENTS 1280 // How much vertical sectors we use in ray cast
#define FLOOR_SEGMENT_SIZE 8

//BACKGROUND
#define OUTLINE_BACK_THICKNESS 5
#define OUTLINE_BACK_COLOR {255, 255, 255, 255}
#define FILL_BACK_COLOR {255, 255, 255, 255}

//TEXTURES

#define WALL_TEXTURE "../texture/building_texture.png"
#define WALL_TEXTURE2 "../texture/brickWall2.jpg"
#define COLUMN_TEXTURE "../texture/column.png"
#define VOLODYA_TEXTURE "../texture/volodya.jpg"
#define FROZEN_TEXTURE "../texture/frozen.jpg"
#define DOG_TEXTURE "../texture/dog.jpg"
#define DINOZAVR_TEXTURE "../texture/dinozavr.png"
#define WHITE_TEXTURE "../texture/white.jpg"
#define BLACK_TEXTURE "../texture/black.jpg"

#define SKY_TEXTURE "../texture/back.jpg"
#define FLOOR_TEXTURE "../texture/173483.jpg"

#define PLAYGAME_PRESSED_TEXTURE "../texture/buttons/playGameSelected.jpg"
#define SETTINGS_PRESSED_TEXTURE "../texture/buttons/settingsSelected.jpg"
#define ABOUT_PRESSED_TEXTURE "../texture/buttons/aboutSelected.jpg"
#define QUIT_PRESSED_TEXTURE "../texture/buttons/quitSelected.jpg"

#define PLAYGAME_TEXTURE "../texture/buttons/playGame.jpg"
#define SETTINGS_TEXTURE "../texture/buttons/settings.jpg"
#define ABOUT_TEXTURE "../texture/buttons/about.jpg"
#define QUIT_TEXTURE "../texture/buttons/quit.jpg"

#define ABOUT_INFO "../texture/instruction.jpg"

#define TEXTURING_SELECT "../texture/buttons/texturing.jpg"
#define SMOOTHING_SELECT "../texture/buttons/smoothing.jpg"
#define COLLISION_SELECT "../texture/buttons/collosion.jpg"

#define TEXTURING_SELECT_S "../texture/buttons/texturingSelected.jpg"
#define SMOOTHING_SELECT_S "../texture/buttons/smoothingSelected.jpg"
#define COLLISION_SELECT_S "../texture/buttons/collosionSelected.jpg"

#define INSTRUCTION_TEXTURE "../texture/instruction.jpg"

// WINDOWS

#define TIMEQUANTA 0.1

// WEAPONS

#define SHOTGUN_TEXTURE "../texture/weapons/shotgun.png"
#define SHOTGUN_ARM_TEXTURE "../texture/weapons/shotgun_arm.png"
#define SHOTGUN_HANDLE_TEXTURE "../texture/weapons/shotgun_handle.png"
#define SHOTGUN_TRUNK_TEXTURE "../texture/weapons/shotgun_trunk.png"

#define AIM_TEXTURE "../texture/weapons/aim.png"
#define FIRE_SHOTGUN_TEXTURE "../texture/weapons/gunBlast.png"

#define GUN_SHOT_SOUND "../sounds/gunShot.ogg"
#define WALK_SOUND "../sounds/walkSound.ogg"

#define HEALTH_TEXTURE "../texture/health.png"
#define SHOUT_TEXTURE "../texture/shots.png"

#define HEALTH_BONUS_TEXTURE "../texture/healBonus.png"
#define AMMUNATION_BONUS_TEXTURE "../texture/ammunBonus.png"
#define SPEED_BONUS_TEXTURE "../texture/speedBonus.png"
#define VIEW_BONUS_TEXTURE "../texture/viewBonus.png"

#define TEXT_FONT "../fonts/19471.ttf"


#endif //PSEUDO3DENGINE_SETTINGS_H
