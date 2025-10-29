#include "obstacle.h"
#include <SDL2/SDL_stdinc.h> 
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include "../SDL/SDL_render.h"
#include "../math/math.h"
#include "log/log.h"


void renderTree(App* app, RenderObject* objectsArr[], SDL_bool* flag_regenTree,
                int32_t* count_tree, int32_t* x, int32_t* heightmap) {
  if (*flag_regenTree) {
    *count_tree = rand() % 6;
    log_debug("count trees = %d", *count_tree);
    for (int i = 0; i < *count_tree; i++) {
      x[i] = rand() % app->screenWidth;
    }
    *flag_regenTree = false;
  }
  if (*count_tree == 0) return;
  for (int i = 0; i < *count_tree; i++) {
    objectsArr[i] = createRenderObject(
        app->renderer, TEXTURE, 1, b_NONE, "media/imgs/tree1.png",
        &(SDL_Point){
            x[i], -120 + app->screenHeight / app->scalingFactorY -
                      heightmap[(int32_t)((x[i] + 54) * app->scalingFactorX)] /
                          app->scalingFactorY});
  }
}
void renderCloud(App* app, RenderObject* objectsArr[],
                 SDL_bool* flag_regencloud, int32_t* count_cloud, int32_t* x,
                 int32_t* heightmap) {
  if (*flag_regencloud) {
    *count_cloud = rand() % 6;
    log_debug("count cloud = %d", *count_cloud);
    for (int i = 0; i < *count_cloud; i++) {
      x[i] = rand() % app->screenWidth;
    }
    *flag_regencloud = false;
  }
  if (*count_cloud == 0) {
    *flag_regencloud = true;
    return;
  }
  for (int i = 0; i < *count_cloud; i++) {
    objectsArr[i] = createRenderObject(
        app->renderer, TEXTURE, 1, b_NONE, "media/imgs/cloud.png",
        &(SDL_Point){x[i],
                     -250 + app->screenHeight / app->scalingFactorY -
                         heightmap[(int32_t)(x[i] * app->scalingFactorX)] /
                             app->scalingFactorY});
    x[i] = x[i] + 1;
    if (x[i] > app->screenWidth) *flag_regencloud = true;
  }
}



