#include "play_render.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "../SDL/SDL_render.h"
#include "../SDL/event_handlers.h"
#include "../game/autosave.h"
#include "../game/gen_map.h"
#include "../game/obstacle.h"
#include "../game/player_movement.h"
#include "../game/specialConditions/spread.h"
#include "../game/specialConditions/wind.h"
#include "../math/math.h"
#include "../math/rand.h"
#include "log/log.h"

enum RandomEvent {
  EVENT_NONE = 0,
  EVENT_SHIELD,
  EVENT_DOUBLE_DAMAGE,
  EVENT_BROKEN_WEAPON,
};

static enum RandomEvent generateRandomEvent(void) {
  int32_t roll = getRandomValue(1, 100);
  if (roll <= 55) {
    return EVENT_NONE;
  } else if (roll <= 70) {
    return EVENT_SHIELD;
  } else if (roll <= 85) {
    return EVENT_DOUBLE_DAMAGE;
  } else {
    return EVENT_BROKEN_WEAPON;
  }
}

static void playMain(App* app, uint32_t SEED);

static void renderMap(SDL_Renderer* renderer, int32_t* heightmap,
                      int32_t* basedMap, int32_t width, int32_t height) {
  for (int32_t x = 0; x < width; x++) {
    for (int32_t y = heightmap[x]; y >= 0; y--) {
      if (y < basedMap[x] * 0.8) {
        SDL_SetRenderDrawColor(renderer, 1, 97, 1, 255);
        SDL_RenderDrawPoint(renderer, x, height - y);
      } else if (y < basedMap[x] * 0.9) {
        SDL_SetRenderDrawColor(renderer, 4, 137, 3, 255);
        SDL_RenderDrawPoint(renderer, x, height - y);
      } else {
        SDL_SetRenderDrawColor(renderer, 1, 181, 0, 255);
        SDL_RenderDrawPoint(renderer, x, height - y);
      }
    }
  }
}

// saving current render state to a texture, so it will be faster to output
static SDL_Texture* saveRenderMapToTexture(SDL_Renderer* renderer,
                                           int32_t width, int32_t height,
                                           int32_t* heightMap,
                                           int32_t* basedMap) {
  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_TARGET, width, height);

  SDL_SetRenderDrawColor(renderer, 135, 206, 235, 255);

  SDL_SetRenderTarget(renderer, texture);
  SDL_RenderClear(renderer);

  renderMap(renderer, heightMap, basedMap, width, height);
  SDL_SetRenderTarget(renderer, NULL);

  return texture;
}

//////////////////////////////////////////////////////////////////////
static void playMainLoop(App* app, struct playMainObjects* objs) {
  char temp[256];

  if (app->currPlayer != objs->oldCurrPlayer) {
    objs->oldCurrPlayer = app->currPlayer;
    objs->showEventSpin = SDL_TRUE;
    objs->resultShown = SDL_FALSE;
    objs->eventSpinStartTime = SDL_GetTicks();
    objs->isSpinning = SDL_TRUE;
    objs->eventSpinText->disableRendering = SDL_FALSE;

    app->currPlayer->buffs.isDoubleDamage = SDL_FALSE;
    app->currPlayer->buffs.isShielded = SDL_FALSE;
    app->currPlayer->buffs.weaponIsBroken = SDL_FALSE;
  }

  if (objs->isSpinning) {
    uint32_t currentTime = SDL_GetTicks();
    uint32_t elapsed = currentTime - objs->eventSpinStartTime;
    const uint32_t EVENT_SPIN_DURATION = 1500;  // 1.5 seconds

    if (elapsed < EVENT_SPIN_DURATION) {
      if (objs->showEventSpin) {
        SDL_DestroyTexture(objs->eventSpinText->data.texture.texture);
        objs->eventSpinText->data.texture.texture = createTextTexture(
            app->renderer, objs->smallFont, "Spinning...", 255, 255, 0, 255);
        SDL_QueryTexture(objs->eventSpinText->data.texture.texture, NULL, NULL,
                         &objs->eventSpinText->data.texture.constRect.w,
                         &objs->eventSpinText->data.texture.constRect.h);
        objs->eventSpinText->data.texture.constRect.x =
            (app->screenWidth / app->scalingFactorX -
             objs->eventSpinText->data.texture.constRect.w) /
            2;
        objs->eventSpinText->disableRendering = SDL_FALSE;
        objs->showEventSpin = SDL_FALSE;
      }
    } else if (elapsed < EVENT_SPIN_DURATION + 1000) {
      if (!objs->resultShown) {
        enum RandomEvent currentEvent = generateRandomEvent();
        const char* eventText = "";
        SDL_Color eventColor = {255, 255, 255, 255};
        switch (currentEvent) {
          case EVENT_NONE:
            eventText = "No event";
            eventColor = (SDL_Color){128, 128, 128, 255};
            break;
          case EVENT_SHIELD:
            eventText = "Shield activated!";
            eventColor = (SDL_Color){0, 255, 255, 255};
            app->currPlayer->buffs.isShielded = SDL_TRUE;
            break;
          case EVENT_DOUBLE_DAMAGE:
            eventText = "Double damage!";
            eventColor = (SDL_Color){255, 0, 0, 255};
            app->currPlayer->buffs.isDoubleDamage = SDL_TRUE;
            break;
          case EVENT_BROKEN_WEAPON:
            eventText = "Weapon broken!";
            eventColor = (SDL_Color){255, 165, 0, 255};
            app->currPlayer->buffs.weaponIsBroken = SDL_TRUE;
            break;
        }
        SDL_DestroyTexture(objs->eventSpinText->data.texture.texture);
        objs->eventSpinText->data.texture.texture = createTextTexture(
            app->renderer, objs->smallFont, eventText, eventColor.r,
            eventColor.g, eventColor.b, eventColor.a);
        SDL_QueryTexture(objs->eventSpinText->data.texture.texture, NULL, NULL,
                         &objs->eventSpinText->data.texture.constRect.w,
                         &objs->eventSpinText->data.texture.constRect.h);
        objs->eventSpinText->data.texture.constRect.x =
            (app->screenWidth / app->scalingFactorX -
             objs->eventSpinText->data.texture.constRect.w) /
            2;
        objs->resultShown = SDL_TRUE;
      }
    } else {
      objs->eventSpinText->disableRendering = SDL_TRUE;
      objs->isSpinning = SDL_FALSE;
      objs->resultShown = SDL_FALSE;
    }
  }

  // regenin' map if needed
  if (objs->regenMap) {
    SDL_DestroyTexture(objs->gameMap);
    objs->gameMap = saveRenderMapToTexture(app->renderer, app->screenWidth,
                                           app->screenHeight, objs->heightMap,
                                           objs->basedMap);
    objs->regenMap = SDL_FALSE;
  }
  SDL_RenderCopy(app->renderer, objs->gameMap, NULL, NULL);
  // filling rect for info at the bottom
  SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
  SDL_RenderFillRect(
      app->renderer,
      &(SDL_Rect){0, app->screenHeight - 40 * app->scalingFactorY,
                  app->screenWidth, app->screenHeight});
  SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
  if (objs->updateConditions.updateWind) {
    updateWind(app);
    objs->updateConditions.updateWind = SDL_FALSE;
  }
  // if flag was set but texture hasnt been hidden yet
  if (objs->hideBulletPath && objs->bulletPath->disableRendering == SDL_FALSE) {
    objs->bulletPath->disableRendering = SDL_TRUE;
    objs->spreadArea->disableRendering = SDL_TRUE;
  } else if (!objs->hideBulletPath &&
             objs->bulletPath->disableRendering == SDL_TRUE) {
    objs->bulletPath->disableRendering = SDL_FALSE;
    objs->spreadArea->disableRendering = SDL_FALSE;
  }
  // recalc bullet path
  if (objs->recalcBulletPath) {
    renderBulletPath(app, objs->bulletPath);
    if (app->currPlayer->buffs.weaponIsBroken) {
      renderSpreadArea(app, objs->spreadArea);
    }
    objs->recalcBulletPath = SDL_FALSE;
  }
  // redrawing info texture
  if (objs->oldAngle != app->currPlayer->gunAngle ||
      objs->oldFiringPower != app->currPlayer->firingPower ||
      objs->oldMovesLeft != app->currPlayer->movesLeft ||
      objs->oldX != app->currPlayer->x ||
      objs->oldScorePlayer1 != objs->firstPlayer.score ||
      objs->oldScorePlayer2 != objs->secondPlayer.score ||
      app->currWeapon != objs->oldWeapon) {
    SDL_DestroyTexture(objs->currentPlayerInfo->data.texture.texture);
    objs->oldAngle = app->currPlayer->gunAngle;
    objs->oldMovesLeft = app->currPlayer->movesLeft;
    objs->oldFiringPower = app->currPlayer->firingPower;
    objs->oldX = app->currPlayer->x;
    objs->oldWeapon = app->currWeapon;
    snprintf(temp, sizeof(temp),
             "Moves left: %d Gun angle: %02d Firing power: %02d ",
             objs->oldMovesLeft, (int32_t)objs->oldAngle, objs->oldFiringPower);
    while (app->currWeapon == -1) {
      app->currWeapon = getAllowedNumber(app);
    }
    switch (app->currWeapon) {
      case 0:
        strcat(temp, "small bullet");
        break;
      case 1:
        strcat(temp, "BIG bullet");
        break;
      case 2:
        strcat(temp, "small boom");
        break;
      case 3:
        strcat(temp, "BIG boom");
        break;
      default:
        break;
    }
    objs->currentPlayerInfo->data.texture.texture = createTextTexture(
        app->renderer, objs->smallFont, temp, 255, 255, 255, 255);
    SDL_QueryTexture(objs->currentPlayerInfo->data.texture.texture, NULL, NULL,
                     &objs->currentPlayerInfo->data.texture.constRect.w,
                     &objs->currentPlayerInfo->data.texture.constRect.h);
    if (app->currPlayer == &objs->firstPlayer) {
      objs->currentPlayerInfo->data.texture.constRect.x = 10;
      // moving arrow to "follow" firstPlayer
      objs->arrow->data.texture.constRect.y =
          objs->Player1Tank->data.texture.constRect.y -
          objs->Player1Tank->data.texture.constRect.w *
              fabs(sin(DEGTORAD(objs->Player1Tank->data.texture.angle))) -
          80;
      objs->arrow->data.texture.constRect.x =
          objs->Player1Tank->data.texture.constRect.x - 15;
      objs->arrow->data.texture.flipFlag = SDL_FLIP_NONE;
    } else {
      objs->currentPlayerInfo->data.texture.constRect.x =
          app->screenWidth / app->scalingFactorX -
          objs->currentPlayerInfo->data.texture.constRect.w - 10;
      // moving arrow to "follow" secondPlayer
      objs->arrow->data.texture.constRect.y =
          objs->Player2Tank->data.texture.constRect.y -
          objs->Player2Tank->data.texture.constRect.w *
              fabs(sin(DEGTORAD(objs->Player2Tank->data.texture.angle))) -
          80;
      objs->arrow->data.texture.constRect.x =
          objs->Player2Tank->data.texture.constRect.x;
      objs->arrow->data.texture.flipFlag = SDL_FLIP_HORIZONTAL;
    }
  }
  if (objs->oldScorePlayer1 != objs->firstPlayer.score) {
    SDL_DestroyTexture(objs->playerScore1->data.texture.texture);
    objs->oldScorePlayer1 = objs->firstPlayer.score;
    snprintf(temp, sizeof(temp), "SCORE: %4d", objs->oldScorePlayer1);
    objs->playerScore1->data.texture.texture =
        createTextTexture(app->renderer, objs->smallFont, temp, 168, 0, 0, 255);
    SDL_QueryTexture(objs->playerScore1->data.texture.texture, NULL, NULL,
                     &objs->playerScore1->data.texture.constRect.w,
                     &objs->playerScore1->data.texture.constRect.h);
    // reposition icons
    objs->p1DoubleDmgIcon->data.texture.constRect.x =
        objs->playerScore1->data.texture.constRect.x;
    objs->p1DoubleDmgIcon->data.texture.constRect.y =
        objs->playerScore1->data.texture.constRect.y +
        objs->playerScore1->data.texture.constRect.h + 6;
    objs->p1ShieldIcon->data.texture.constRect.x =
        objs->p1DoubleDmgIcon->data.texture.constRect.x +
        objs->p1DoubleDmgIcon->data.texture.constRect.w + 6;
    objs->p1ShieldIcon->data.texture.constRect.y =
        objs->p1DoubleDmgIcon->data.texture.constRect.y;
  }
  if (objs->oldScorePlayer2 != objs->secondPlayer.score) {
    SDL_DestroyTexture(objs->playerScore2->data.texture.texture);
    objs->oldScorePlayer2 = objs->secondPlayer.score;
    snprintf(temp, sizeof(temp), "SCORE: %4d", objs->oldScorePlayer2);
    objs->playerScore2->data.texture.texture = createTextTexture(
        app->renderer, objs->smallFont, temp, 0, 168, 107, 255);
    SDL_QueryTexture(objs->playerScore2->data.texture.texture, NULL, NULL,
                     &objs->playerScore2->data.texture.constRect.w,
                     &objs->playerScore2->data.texture.constRect.h);
    // reposition icons
    objs->p2DoubleDmgIcon->data.texture.constRect.y =
        objs->playerScore2->data.texture.constRect.y +
        objs->playerScore2->data.texture.constRect.h + 6;
    objs->p2ShieldIcon->data.texture.constRect.y =
        objs->p2DoubleDmgIcon->data.texture.constRect.y;
    int32_t rightEdge2 = objs->playerScore2->data.texture.constRect.x +
                         objs->playerScore2->data.texture.constRect.w;
    objs->p2ShieldIcon->data.texture.constRect.x =
        rightEdge2 - objs->p2ShieldIcon->data.texture.constRect.w;
    objs->p2DoubleDmgIcon->data.texture.constRect.x =
        objs->p2ShieldIcon->data.texture.constRect.x - 6 -
        objs->p2DoubleDmgIcon->data.texture.constRect.w;
  }
  // toggle visibility
  objs->p1DoubleDmgIcon->disableRendering =
      objs->firstPlayer.buffs.isDoubleDamage ? SDL_FALSE : SDL_TRUE;
  objs->p1ShieldIcon->disableRendering =
      objs->firstPlayer.buffs.isShielded ? SDL_FALSE : SDL_TRUE;
  objs->p2DoubleDmgIcon->disableRendering =
      objs->secondPlayer.buffs.isDoubleDamage ? SDL_FALSE : SDL_TRUE;
  objs->p2ShieldIcon->disableRendering =
      objs->secondPlayer.buffs.isShielded ? SDL_FALSE : SDL_TRUE;
}

inline static void playMainClear(App* app, struct playMainObjects* objs) {
  freeRenderObject(objs->Player1Tank);
  freeRenderObject(objs->Player1Gun);
  freeRenderObject(objs->Player2Tank);
  freeRenderObject(objs->Player2Gun);
  freeRenderObject(objs->currentPlayerInfo);
  freeRenderObject(objs->betmentAvatar);
  freeRenderObject(objs->emoji);
  freeRenderObject(objs->jonklerAvatar);
  freeRenderObject(objs->arrow);
  freeRenderObject(objs->projectile);
  freeRenderObject(objs->explosionObj);
  freeRenderObject(objs->playerScore1);
  freeRenderObject(objs->playerScore2);
  freeRenderObject(objs->p1DoubleDmgIcon);
  freeRenderObject(objs->p1ShieldIcon);
  freeRenderObject(objs->p2DoubleDmgIcon);
  freeRenderObject(objs->p2ShieldIcon);
  freeRenderObject(objs->eventSpinText);
  freeRenderObject(objs->bulletPath);
  freeRenderObject(objs->spreadArea);
  freeRenderObject(objs->speedLabelObject);
  freeRenderObject(objs->directionIconObject);
  freeRenderObject(objs->tree1);
  freeRenderObject(objs->tree2);
  freeRenderObject(objs->tree3);
  freeRenderObject(objs->tree4);
  freeRenderObject(objs->tree5);
  freeRenderObject(objs->stone1);
  freeRenderObject(objs->stone2);
  freeRenderObject(objs->cloud1);
  freeRenderObject(objs->cloud2);
  freeRenderObject(objs->cloud3);
  freeRenderObject(objs->cloud4);
  freeRenderObject(objs->cloud5);
  freeRenderObject(objs->cloud6);
  freeRenderObject(objs->cloud7);
  freeRenderObject(objs->cloud8);
  freeRenderObject(objs->cloud9);
  freeRenderObject(objs->cloud10);
  freeRenderObject(objs->cloud11);
  freeRenderObject(objs->cloud12);
  freeRenderObject(objs->cloud13);
  freeRenderObject(objs->cloud14);

  TTF_CloseFont(objs->smallFont);
  SDL_DestroyTexture(objs->gameMap);

  free(objs->heightMap);
  free(objs->basedMap);
}

// main game loop
static void playMain(App* app, uint32_t SEED) {
  struct playMainObjects* objs =
      (struct playMainObjects*)malloc(sizeof(struct playMainObjects));

  if (!objs) {
    log_fatal("CANNOT ALLOCATE MEMORY FOR OBJECTS");
    exit(-1);
  }
  {
    char temp[256];
    snprintf(temp, sizeof(temp), "%smedia/fonts/PixeloidSans.ttf",
             app->basePath);
    objs->smallFont = loadFont(temp, 30);
    SDL_bool wasLoaded = SDL_FALSE;
    // if save was loaded
    if (SEED == 1000000001u) {
      wasLoaded = SDL_TRUE;
    }
    // if seed wasn't set
    if (SEED == 1000000000u) {
      srand(time(NULL));
      objs->mapSeed = getRandomValue(0, 1000000000u);
      log_info("setting gen map seed to: RAND - %u", objs->mapSeed);
    } else {
      objs->mapSeed = SEED;
      log_info("setting gen map seed to fixed: %u", objs->mapSeed);
    }
    int32_t x1;
    int32_t x2;
    // default heightMap for saving map levels
    objs->heightMap = (int32_t*)malloc(app->screenWidth * sizeof(int32_t));
    // baseMap used for pretty color output even when smth is destroyed
    objs->basedMap = (int32_t*)malloc(app->screenWidth * sizeof(int32_t));
    if (!objs->heightMap || !objs->basedMap) exit(0);
    // if player pressed "LOAD" button
    if (wasLoaded) {
      switch (loadSavedState(app, &objs->firstPlayer, &objs->secondPlayer,
                             objs->heightMap, &objs->mapSeed)) {
        // incorrect saveFile
        case 1:
          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                   "SAVE_LOAD_ERROR", "save is broken! :(",
                                   app->window);
          app->currState = LOGO;
          return;
        // incorrect screen info in saveFile
        case 2:
          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                   "SAVE_LOAD_ERROR",
                                   "incorrect screen size! :(", app->window);
          app->currState = LOGO;
          return;
        default:
          break;
      }
      x1 = objs->firstPlayer.x;
      x2 = objs->secondPlayer.x;
      genHeightMap(objs->basedMap, objs->mapSeed, app->screenWidth,
                   app->screenHeight);
      addSpawnPlates(objs->basedMap, app->screenWidth,
                     70 * app->scalingFactorX);
    } else {
      genHeightMap(objs->heightMap, objs->mapSeed, app->screenWidth,
                   app->screenHeight);
      addSpawnPlates(objs->heightMap, app->screenWidth,
                     70 * app->scalingFactorX);
      memcpy(objs->basedMap, objs->heightMap,
             app->screenWidth * sizeof(int32_t));
      x1 = 20;
      x2 = app->screenWidth / app->scalingFactorX - 44 - 20;
      app->timesPlayed = 0;
    }
    // default buffs state (disabled by default)
    objs->firstPlayer.buffs.weaponIsBroken = SDL_FALSE;
    objs->firstPlayer.buffs.isDoubleDamage = SDL_FALSE;
    objs->firstPlayer.buffs.isShielded = SDL_FALSE;
    objs->secondPlayer.buffs.weaponIsBroken = SDL_FALSE;
    objs->secondPlayer.buffs.isDoubleDamage = SDL_FALSE;
    objs->secondPlayer.buffs.isShielded = SDL_FALSE;
    // height map will contain the 'base' for map (for rendering different
    // colors)
    objs->gameMap = saveRenderMapToTexture(app->renderer, app->screenWidth,
                                           app->screenHeight, objs->heightMap,
                                           objs->basedMap);
    // fuck it just using a simple angle calc :-)
    double anglePlayer1 = getAngle((x1 + 5) * app->scalingFactorX,
                                   objs->heightMap, 25 * app->scalingFactorX);
    double anglePlayer2 = getAngle((x2 + 8) * app->scalingFactorX,
                                   objs->heightMap, 25 * app->scalingFactorX);
    // 1st player textures
    objs->Player1Tank = createRenderObject(
        app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
        "media/imgs/tank_betment.png",
        &(SDL_Point){
            x1, -27 + app->screenHeight / app->scalingFactorY -
                    objs->heightMap[(int32_t)((x1 + 5) * app->scalingFactorX)] /
                        app->scalingFactorY},
        360 - anglePlayer1, SDL_FLIP_NONE,
        &(SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY});
    objs->Player1Gun = createRenderObject(
        app->renderer, TEXTURE | EXTENDED | DOUBLE_EXTENDED, 1, LEFT_GUN,
        "media/imgs/bet_dulo.png",
        &(SDL_Point){
            x1, -27 + app->screenHeight / app->scalingFactorY -
                    objs->heightMap[(int32_t)((x1 + 5) * app->scalingFactorX)] /
                        app->scalingFactorY},
        360 - anglePlayer1, 0.0, SDL_FLIP_NONE,
        &(SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY},
        &(SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});
    // 2nd player textures
    objs->Player2Tank = createRenderObject(
        app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
        "media/imgs/tank_jonkler.png",
        &(SDL_Point){
            x2, -27 + app->screenHeight / app->scalingFactorY -
                    objs->heightMap[(int32_t)((x2 + 8) * app->scalingFactorX)] /
                        app->scalingFactorY},
        360 - anglePlayer2, SDL_FLIP_HORIZONTAL,
        &(SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY});
    objs->Player2Gun = createRenderObject(
        app->renderer, TEXTURE | EXTENDED | DOUBLE_EXTENDED, 1, RIGHT_GUN,
        "media/imgs/jonk_dulo.png",
        &(SDL_Point){
            x2, -27 + app->screenHeight / app->scalingFactorY -
                    objs->heightMap[(int32_t)((x2 + 8) * app->scalingFactorX)] /
                        app->scalingFactorY},
        360 - anglePlayer2, 0.0, SDL_FLIP_HORIZONTAL,
        &(SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY},
        &(SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});
    objs->betmentAvatar = createRenderObject(
        app->renderer, GIF, 1, b_NONE, "media/imgs/betment.png",
        &(SDL_Point){12, 30}, 39, 2, SDL_FALSE);
    objs->emoji = createRenderObject(app->renderer, GIF, 1, b_NONE,
                                     "media/imgs/cl_goblin.png",
                                     &(SDL_Point){0, 0}, 32, 5, SDL_TRUE);
    objs->emoji->disableRendering = SDL_TRUE;
    objs->jonklerAvatar =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/jonkler.png", &(SDL_Point){0, 30});
    objs->jonklerAvatar->data.texture.constRect.x =
        app->screenWidth / app->scalingFactorX - 12 -
        objs->jonklerAvatar->data.texture.constRect.w;
    objs->explosionObj = createRenderObject(
        app->renderer, GIF, 1, b_NONE, "media/imgs/explosion.png",
        &(SDL_Point){0, 30}, 24, 3, SDL_TRUE);
    objs->explosionObj->disableRendering = SDL_TRUE;
    objs->arrow = createRenderObject(app->renderer, GIF, 1, b_NONE,
                                     "media/imgs/arrow.png", &(SDL_Point){0, 0},
                                     8, 3, SDL_FALSE);
    objs->projectile =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/proj.png", &(SDL_Point){0, 0});
    objs->projectile->disableRendering = SDL_TRUE;
    objs->bulletPath =
        createRenderObject(app->renderer, EMPTY, 0, b_NONE, 333, 333);
    objs->spreadArea =
        createRenderObject(app->renderer, EMPTY, 0, b_NONE, 333, 333);
    objs->speedLabelObject = createRenderObject(
        app->renderer, TEXT, 1, b_NONE, "1337", objs->smallFont,
        &(SDL_Point){0, 0}, &(SDL_Color){255, 255, 255, 255});
    objs->speedLabelObject->data.texture.constRect.x =
        (app->screenWidth / app->scalingFactorX -
         objs->speedLabelObject->data.texture.constRect.w) /
        2;
    objs->speedLabelObject->data.texture.constRect.y = 80;
    objs->directionIconObject = createRenderObject(
        app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
        "media/imgs/windDirection.png", &(SDL_Point){0, 0}, 0, SDL_FLIP_NONE,
        &(SDL_Point){32 * app->scalingFactorX, 32 * app->scalingFactorY});
    objs->directionIconObject->data.texture.constRect.x =
        (app->screenWidth / app->scalingFactorX -
         objs->directionIconObject->data.texture.constRect.w) /
        2;
    objs->directionIconObject->data.texture.constRect.y = 10;
    app->globalConditions.wind.speedLabel = objs->speedLabelObject;
    app->globalConditions.wind.directionIcon = objs->directionIconObject;
    objs->firstPlayer.tankObj = objs->Player1Tank;
    objs->firstPlayer.tankGunObj = objs->Player1Gun;
    objs->secondPlayer.tankObj = objs->Player2Tank;
    objs->secondPlayer.tankGunObj = objs->Player2Gun;
    objs->firstPlayer.inAnimation = SDL_FALSE;
    objs->secondPlayer.inAnimation = SDL_FALSE;
    // if settings wasnt loaded
    // setting up the 'default' settings
    if (!wasLoaded) {
      log_info("using default settings for players!");
      objs->firstPlayer.movesLeft = 30;
      objs->firstPlayer.health = 100;
      objs->firstPlayer.gunAngle = 0.0;
      objs->firstPlayer.firingPower = 30;
      objs->firstPlayer.tankAngle = anglePlayer1;
      objs->firstPlayer.inAnimation = SDL_FALSE;
      objs->firstPlayer.x = 30;
      objs->firstPlayer.score = 0;
      switch (app->p1Diff) {
        case b_P1Player:
          objs->firstPlayer.type = MONKE;
          break;
        case b_P1BOT1:
          objs->firstPlayer.type = BOT1;
          break;
        case b_P1BOT2:
          objs->firstPlayer.type = BOT2;
          break;
        case b_P1BOT3:
          objs->firstPlayer.type = BOT3;
          break;
        default:
          break;
      }
      objs->secondPlayer.movesLeft = 30;
      objs->secondPlayer.health = 100;
      objs->secondPlayer.gunAngle = 0.0;
      objs->secondPlayer.firingPower = 30;
      objs->secondPlayer.tankAngle = anglePlayer2;
      objs->secondPlayer.inAnimation = SDL_FALSE;
      objs->secondPlayer.type = MONKE;
      objs->secondPlayer.x = app->screenWidth / app->scalingFactorX - 44 - 30;
      objs->secondPlayer.score = 0;
      switch (app->p2Diff) {
        case b_P2Player:
          objs->secondPlayer.type = MONKE;
          break;
        case b_P2BOT1:
          objs->secondPlayer.type = BOT1;
          break;
        case b_P2BOT2:
          objs->secondPlayer.type = BOT2;
          break;
        case b_P2BOT3:
          objs->secondPlayer.type = BOT3;
          break;
        default:
          break;
      }
      app->currPlayer = &objs->firstPlayer;
      saveCurrentState(app, &objs->firstPlayer, &objs->secondPlayer,
                       objs->heightMap, SDL_TRUE, objs->mapSeed);
    }
    objs->regenMap = SDL_FALSE;
    objs->hideBulletPath = SDL_FALSE;
    objs->recalcBulletPath = SDL_TRUE;
    objs->updateConditions = (struct UpdateConditions){
        .updateWind = SDL_TRUE,
    };
    objs->playerMove_Params = (struct paramsStruct){
        .app = app,
        .firstPlayer = &objs->firstPlayer,
        .secondPlayer = &objs->secondPlayer,
        .heightMap = objs->heightMap,
        .projectile = objs->projectile,
        .explosion = objs->explosionObj,
        .regenMap = &objs->regenMap,
        .recalcBulletPath = &objs->recalcBulletPath,
        .updateConditions = &objs->updateConditions,
        .hideBulletPath = &objs->hideBulletPath,
        .isSpinning = &objs->isSpinning,
        .mapSeed = objs->mapSeed,
    };
    // creating trees with some probability of apearing
    //
    objs->tree1 = createTree(app, objs->heightMap, 100, 150, 10);
    objs->tree2 = createTree(app, objs->heightMap, 350, 425, 20);
    objs->tree3 = createTree(app, objs->heightMap, 475, 500, 20);
    objs->tree4 = createTree(app, objs->heightMap, 525, 540, 20);
    objs->tree5 = createTree(app, objs->heightMap, 850, 950, 10);
    uint32_t currCnt = 0;
    // creating clouds
    objs->cloud1 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud2 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud3 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud4 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud5 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud6 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud7 = createCloud(app, objs->heightMap, 80, 400, 0, currCnt++);
    objs->cloud8 = createCloud(app, objs->heightMap, 690, 940, 0, currCnt++);
    objs->cloud9 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);
    objs->cloud10 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);
    objs->cloud11 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);
    objs->cloud12 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);
    objs->cloud13 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);
    objs->cloud14 = createCloud(app, objs->heightMap, 690, 800, 0, currCnt++);

    currCnt = 0;
    // creating stones
    objs->stone1 = createStone(app, objs->heightMap, 200, 300, currCnt++);
    objs->stone2 =
        createStone(app, objs->heightMap, 600,
                    app->screenWidth / app->scalingFactorX - 250, currCnt++);
    recalcPlayerPos(app, &objs->firstPlayer, objs->heightMap, 0, 5);
    recalcPlayerPos(app, &objs->secondPlayer, objs->heightMap, 0, 8);
    recalcPlayerGunAngle(&objs->firstPlayer, 0);
    recalcPlayerGunAngle(&objs->secondPlayer, 0);
    // setting init or loaded angles for the gun
    objs->Player1Gun->data.texture.angleAlt = -objs->firstPlayer.gunAngle;
    objs->Player2Gun->data.texture.angleAlt = -objs->secondPlayer.gunAngle;
    // creating thread for a proceeding player movements
    objs->playerMoveThread =
        SDL_CreateThread(playerMove, NULL, (void*)&objs->playerMove_Params);
    // old values for optimized update
    // just to be sure we rendering pointing arrow correctly
    objs->oldAngle = app->currPlayer->gunAngle - 1;
    objs->oldMovesLeft = app->currPlayer->movesLeft;
    objs->oldFiringPower = app->currPlayer->firingPower;
    objs->oldX = app->currPlayer->x;
    objs->oldScorePlayer1 = objs->firstPlayer.score;
    objs->oldScorePlayer2 = objs->secondPlayer.score;
    objs->oldWeapon = app->currWeapon;
    snprintf(temp, sizeof(temp),
             "Moves left: %d Gun angle: %02d Firing power: %02d ",
             objs->oldMovesLeft, (int32_t)objs->oldAngle, objs->oldFiringPower);
    if (app->currWeapon == -1) {
      app->currWeapon = getAllowedNumber(app);
    }
    switch (app->currWeapon) {
      case 0:
        strcat(temp, "small bullet");
        break;
      case 1:
        strcat(temp, "BIG bullet");
        break;
      case 2:
        strcat(temp, "small boom");
        break;
      case 3:
        strcat(temp, "BIG boom");
        break;
      default:
        break;
    }
    objs->currentPlayerInfo = createRenderObject(
        app->renderer, TEXT, 1, b_NONE, temp, objs->smallFont,
        &(SDL_Point){10, app->screenHeight / app->scalingFactorY - 40},
        &(SDL_Color){255, 255, 255, 255});
    snprintf(temp, sizeof(temp), "HEALTH: %3d", objs->firstPlayer.health);
    objs->playerScore1 = createRenderObject(
        app->renderer, TEXT, 1, b_NONE, temp, objs->smallFont,
        &(SDL_Point){10, objs->betmentAvatar->data.texture.constRect.y +
                             objs->betmentAvatar->data.texture.constRect.h +
                             10},
        &(SDL_Color){168, 0, 0, 255});
    snprintf(temp, sizeof(temp), "HEALTH: %3d", objs->secondPlayer.health);
    objs->playerScore2 = createRenderObject(
        app->renderer, TEXT, 1, b_NONE, temp, objs->smallFont,
        &(SDL_Point){0, objs->betmentAvatar->data.texture.constRect.y +
                            objs->betmentAvatar->data.texture.constRect.h + 10},
        &(SDL_Color){0, 168, 107, 255});
    objs->playerScore2->data.texture.constRect.x =
        app->screenWidth / app->scalingFactorX -
        objs->playerScore2->data.texture.constRect.w - 10;
    // Buff icons under SCORE (double damage, then shield) for both players
    objs->p1DoubleDmgIcon =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/doubleDamage.png", &(SDL_Point){0, 0});
    objs->p1ShieldIcon =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/shield.png", &(SDL_Point){0, 0});
    objs->p2DoubleDmgIcon =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/doubleDamage.png", &(SDL_Point){0, 0});
    objs->p2ShieldIcon =
        createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                           "media/imgs/shield.png", &(SDL_Point){0, 0});
    // buffs icons
    objs->p1DoubleDmgIcon->data.texture.constRect.x =
        objs->playerScore1->data.texture.constRect.x;
    objs->p1DoubleDmgIcon->data.texture.constRect.y =
        objs->playerScore1->data.texture.constRect.y +
        objs->playerScore1->data.texture.constRect.h + 6;
    objs->p1ShieldIcon->data.texture.constRect.x =
        objs->p1DoubleDmgIcon->data.texture.constRect.x +
        objs->p1DoubleDmgIcon->data.texture.constRect.w + 6;
    objs->p1ShieldIcon->data.texture.constRect.y =
        objs->p1DoubleDmgIcon->data.texture.constRect.y;
    objs->p2DoubleDmgIcon->data.texture.constRect.y =
        objs->playerScore2->data.texture.constRect.y +
        objs->playerScore2->data.texture.constRect.h + 6;
    objs->p2ShieldIcon->data.texture.constRect.y =
        objs->p2DoubleDmgIcon->data.texture.constRect.y;
    // align right player's icons to the right edge of SCORE2
    {
      int32_t rightEdge = objs->playerScore2->data.texture.constRect.x +
                          objs->playerScore2->data.texture.constRect.w;
      objs->p2ShieldIcon->data.texture.constRect.x =
          rightEdge - objs->p2ShieldIcon->data.texture.constRect.w;
      objs->p2DoubleDmgIcon->data.texture.constRect.x =
          objs->p2ShieldIcon->data.texture.constRect.x - 6 -
          objs->p2DoubleDmgIcon->data.texture.constRect.w;
    }
    if (app->currPlayer == &objs->secondPlayer) {
      objs->currentPlayerInfo->data.texture.constRect.x =
          app->screenWidth / app->scalingFactorX -
          objs->currentPlayerInfo->data.texture.constRect.w - 10;
    }

    // Random event spin display
    objs->eventSpinText = createRenderObject(
        app->renderer, TEXT, 1, b_NONE, "Spinning...", objs->smallFont,
        &(SDL_Point){0, 0}, &(SDL_Color){255, 255, 0, 255});
    objs->eventSpinText->data.texture.constRect.x =
        (app->screenWidth / app->scalingFactorX -
         objs->eventSpinText->data.texture.constRect.w) /
        2;
    objs->eventSpinText->data.texture.constRect.y = 150;
    objs->eventSpinText->disableRendering = SDL_TRUE;

    // Initialize random events fields
    objs->showEventSpin = SDL_FALSE;
    objs->resultShown = SDL_FALSE;
    objs->isSpinning = SDL_FALSE;
    objs->eventSpinStartTime = 0;
    objs->oldCurrPlayer = app->currPlayer;
  }

  RenderObject* objectsArr[] = {
      objs->currentPlayerInfo,
      objs->projectile,
      objs->betmentAvatar,
      objs->emoji,
      objs->jonklerAvatar,
      objs->arrow,
      objs->firstPlayer.tankGunObj,
      objs->firstPlayer.tankObj,
      objs->secondPlayer.tankGunObj,
      objs->secondPlayer.tankObj,
      objs->explosionObj,
      objs->tree1,
      objs->tree2,
      objs->tree3,
      objs->tree4,
      objs->tree5,
      objs->stone1,
      objs->stone2,
      objs->cloud1,
      objs->cloud2,
      objs->cloud3,
      objs->cloud4,
      objs->cloud5,
      objs->cloud6,
      objs->cloud7,
      objs->cloud8,
      objs->cloud9,
      objs->cloud10,
      objs->cloud11,
      objs->cloud12,
      objs->cloud13,
      objs->cloud14,
      objs->playerScore1,
      objs->playerScore2,
      objs->p1DoubleDmgIcon,
      objs->p1ShieldIcon,
      objs->p2DoubleDmgIcon,
      objs->p2ShieldIcon,
      objs->bulletPath,
      objs->spreadArea,
      objs->speedLabelObject,
      objs->directionIconObject,
      objs->eventSpinText,
  };

  while (app->currState == PLAY) {
    pollAllEvents(app);
    while (app->currWeapon == -1) {
      app->currWeapon = getAllowedNumber(app);
    }

    SDL_SetRenderDrawColor(app->renderer, 127, 127, 127, 255);
    SDL_RenderClear(app->renderer);

    playMainLoop(app, objs);

    renderTextures(app, objectsArr, sizeof(objectsArr) / sizeof(*objectsArr),
                   SDL_TRUE);
    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);
  }

  SDL_WaitThread(objs->playerMoveThread, NULL);
  playMainClear(app, objs);
}
//////////////////////////////////////////////////////////////////////
static void preGameMainInit(App* app, PreGameMainObjects* objs) {
  app->p1Diff = b_NULL;
  app->p2Diff = b_NULL;

  char temp[256];

  snprintf(temp, sizeof(temp), "%smedia/fonts/PixeloidSans.ttf", app->basePath);
  TTF_Font* smallFont = loadFont(temp, 30);

  snprintf(temp, sizeof(temp), "%smedia/fonts/PixeloidSans-Bold.ttf",
           app->basePath);
  TTF_Font* mainFont = loadFont(temp, 60);

  objs->seedText =
      createRenderObject(app->renderer, TEXT, 1, b_NONE, "SEED:", smallFont,
                         &(SDL_Point){0, 0}, &(SDL_Color){128, 128, 128, 255});
  objs->seedText->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->seedText->data.texture.constRect.w);
  objs->seedText->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY -
       objs->seedText->data.texture.constRect.h) /
          2 -
      300;

  objs->seedInput = createRenderObject(
      app->renderer, TEXT_INPUT, 1, bTI_SEED,
      &(SDL_Rect){objs->seedText->data.texture.constRect.x - 5,
                  objs->seedText->data.texture.constRect.y +
                      objs->seedText->data.texture.constRect.h,
                  200, 50},
      9, 2, smallFont);

  objs->loadTextButton = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_PREGAME_LOAD, "LOAD SAVE",
      mainFont, &(SDL_Point){0, 0}, &(SDL_Color){128, 128, 128, 255},
      &(SDL_Color){230, 25, 25, 255});
  objs->loadTextButton->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       objs->loadTextButton->data.texture.constRect.w) /
      2;
  objs->loadTextButton->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY - 170);

  objs->startTextButton = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_PREGAME_START,
      "START NEW GAME", mainFont, &(SDL_Point){0, 0},
      &(SDL_Color){0, 255, 189, 200}, &(SDL_Color){230, 25, 25, 255});
  objs->startTextButton->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       objs->startTextButton->data.texture.constRect.w) /
      2;
  objs->startTextButton->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY - 100);

  objs->difficultyText = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "DIFFICULTY", mainFont,
      &(SDL_Point){0, objs->seedInput->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  objs->difficultyText->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       objs->difficultyText->data.texture.constRect.w) /
      2;

  // player1 difficulty choice
  objs->Player1DiffLabel = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "Player 1: ", smallFont,
      &(SDL_Point){30, objs->difficultyText->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  objs->Player1DiffLabel->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->Player1DiffLabel->data.texture.constRect.w) /
      2;

  // player
  objs->Player1Diff_p = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1Player, "livin' man",
      smallFont,
      &(SDL_Point){0, objs->Player1DiffLabel->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player1Diff_p->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->Player1Diff_p->data.texture.constRect.w) /
      2;

  // easy diff
  objs->Player1Diff_bE = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1BOT1, "BOT1", smallFont,
      &(SDL_Point){0, objs->Player1Diff_p->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player1Diff_bE->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->Player1Diff_bE->data.texture.constRect.w) /
      2;

  // normal diff
  objs->Player1Diff_bN = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1BOT2, "BOT2", smallFont,
      &(SDL_Point){0, objs->Player1Diff_bE->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player1Diff_bN->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->Player1Diff_bN->data.texture.constRect.w) /
      2;

  // hard diff
  objs->Player1Diff_bH = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1BOT3, "BOT3", smallFont,
      &(SDL_Point){0, objs->Player1Diff_bN->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player1Diff_bH->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       objs->Player1Diff_bH->data.texture.constRect.w) /
      2;

  // player2 difficulty choice
  objs->Player2DiffLabel = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "Player 2:", smallFont,
      &(SDL_Point){0, objs->difficultyText->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  objs->Player2DiffLabel->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       objs->Player2DiffLabel->data.texture.constRect.w) /
      2;

  // player
  objs->Player2Diff_p = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2Player, "livin' man",
      smallFont,
      &(SDL_Point){0, objs->Player2DiffLabel->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player2Diff_p->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       objs->Player2Diff_p->data.texture.constRect.w) /
      2;

  // easy diff
  objs->Player2Diff_bE = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2BOT1, "BOT1", smallFont,
      &(SDL_Point){0, objs->Player2Diff_p->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player2Diff_bE->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       objs->Player2Diff_bE->data.texture.constRect.w) /
      2;

  // normal diff
  objs->Player2Diff_bN = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2BOT2, "BOT2", smallFont,
      &(SDL_Point){0, objs->Player2Diff_bE->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player2Diff_bN->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       objs->Player2Diff_bN->data.texture.constRect.w) /
      2;

  // hard diff
  objs->Player2Diff_bH = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2BOT3, "BOT3", smallFont,
      &(SDL_Point){0, objs->Player2Diff_bN->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  objs->Player2Diff_bH->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       objs->Player2Diff_bH->data.texture.constRect.w) /
      2;

  TTF_CloseFont(objs->smallFont);
  TTF_CloseFont(objs->mainFont);
}

inline static void preGameMainClear(App* app, PreGameMainObjects* objs) {
  SDL_DestroyTexture(objs->seedInput->data.textInputLine.textTexture);
  TTF_CloseFont(objs->smallFont);
  TTF_CloseFont(objs->mainFont);
  freeRenderObject(objs->seedText);
  freeRenderObject(objs->startTextButton);
  freeRenderObject(objs->loadTextButton);
  freeRenderObject(objs->Player1DiffLabel);
  freeRenderObject(objs->Player1Diff_p);
  freeRenderObject(objs->Player1Diff_bE);
  freeRenderObject(objs->Player1Diff_bN);
  freeRenderObject(objs->Player1Diff_bH);
  freeRenderObject(objs->Player2DiffLabel);
  freeRenderObject(objs->Player2Diff_p);
  freeRenderObject(objs->Player2Diff_bE);
  freeRenderObject(objs->Player2Diff_bN);
  freeRenderObject(objs->Player2Diff_bH);
  freeRenderObject(objs->difficultyText);
  free(objs->seedInput);
}

// pre game (like settings before game starts :-) )
void preGameMain(App* app) {
  PreGameMainObjects objs = {0};
  preGameMainInit(app, &objs);

  RenderObject* objectsArr[] = {
      objs.difficultyText,   objs.Player1DiffLabel, objs.Player1Diff_p,
      objs.Player1Diff_bE,   objs.Player1Diff_bN,   objs.Player1Diff_bH,
      objs.Player2DiffLabel, objs.Player2Diff_p,    objs.Player2Diff_bE,
      objs.Player2Diff_bN,   objs.Player2Diff_bH,   objs.seedText,
      objs.seedInput,        objs.loadTextButton,   objs.startTextButton};

  while (app->currState == PREGAME_SETTING) {
    pollAllEvents(app);
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);

    // lines in the middle of the screen
    SDL_SetRenderDrawColor(app->renderer, 128, 128, 128, 255);
    SDL_RenderDrawLine(app->renderer, 0,
                       (objs.difficultyText->data.texture.constRect.y - 10) *
                           app->scalingFactorY,
                       app->screenWidth,
                       (objs.difficultyText->data.texture.constRect.y - 10) *
                           app->scalingFactorY);
    SDL_RenderDrawLine(app->renderer, 0,
                       (objs.difficultyText->data.texture.constRect.y + 90) *
                           app->scalingFactorY,
                       app->screenWidth,
                       (objs.difficultyText->data.texture.constRect.y + 90) *
                           app->scalingFactorY);
    SDL_RenderDrawLine(app->renderer, app->screenWidth / 2,
                       (objs.difficultyText->data.texture.constRect.y + 90) *
                           app->scalingFactorY,
                       app->screenWidth / 2,
                       (objs.loadTextButton->data.texture.constRect.y - 10) *
                           app->scalingFactorY);
    SDL_RenderDrawLine(app->renderer, 0,
                       (objs.loadTextButton->data.texture.constRect.y - 10) *
                           app->scalingFactorY,
                       app->screenWidth,
                       (objs.loadTextButton->data.texture.constRect.y - 10) *
                           app->scalingFactorY);
    //

    renderTextures(app, objectsArr, sizeof(objectsArr) / sizeof(*objectsArr),
                   SDL_TRUE);

    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);
  }

  char temp[256];

  memcpy(temp, objs.seedInput->data.textInputLine.savedText,
         objs.seedInput->data.textInputLine.maxInputChars);

  preGameMainClear(app, &objs);

  if (app->currState == LOAD) {
    app->currState = PLAY;
    playMain(app, 1000000001u);
    return;
  }

  if (app->currState == PLAY) {
    if (*temp != '\0') {
      playMain(app, (uint32_t)SDL_atoi(temp));
      return;
    } else {
      playMain(app, 1000000000u);
      return;
    }
  }

  return;
}
