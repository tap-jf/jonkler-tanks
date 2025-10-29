#ifndef OBSTACLE_H
#define OBSTACLE_H

#include "../App.h"


RenderObject* fall(App* app, RenderObject* object, int32_t* heightmap,
                   int32_t x);
void renderTree(App* app, RenderObject* objectsArr[], SDL_bool* flag_regenTree,
                int32_t* count_tree, int32_t* x, int32_t* heightmap);
void renderCloud(App* app, RenderObject* objectsArr[],
                 SDL_bool* flag_regencloud, int32_t* count_cloud, int32_t* x,
                 int32_t* heightmap);
void renderShelter76(App* app, RenderObject* objectsArr[],
                     SDL_bool* flag_regenShelter, int32_t* count_shelter,
                     int32_t* x, int32_t* heightmap);
extern uint32_t obstacleRock[4];
#endif