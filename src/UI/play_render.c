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
#include "../game/player_movement.h"
#include "../game/specialConditions/wind.h"
#include "../math/math.h"

#include "log/log.h"

static void playMain(App* app, uint32_t SEED);

static void renderMap(SDL_Renderer* renderer, int32_t* heightmap,
                      int32_t* basedMap, int32_t width, int32_t height) {
  uint8_t currR1, currG1, currB1;
  uint8_t currR2, currG2, currB2;
  uint8_t currR3, currG3, currB3;

  currR1 = rand() % 255;
  currG1 = rand() % 255;
  currB1 = rand() % 255;

  currR2 = rand() % 255;
  currG2 = rand() % 255;
  currB2 = rand() % 255;

  currR3 = rand() % 255;
  currG3 = rand() % 255;
  currB3 = rand() % 255;

  for (int32_t x = 0; x < width; x++) {
    for (int32_t y = heightmap[x]; y >= 0; y--) {
      if (y < basedMap[x] * 0.8) {
        SDL_SetRenderDrawColor(renderer, currR1, currG1, currB1, 255);
        SDL_RenderDrawPoint(renderer, x, height - y);
      } else if (y < basedMap[x] * 0.9) {
        SDL_SetRenderDrawColor(renderer, currR2, currG2, currB2, 255);
        SDL_RenderDrawPoint(renderer, x, height - y);
      } else {
        SDL_SetRenderDrawColor(renderer, currR3, currG3, currB3, 255);
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
  // for (int32_t x = 0; x < width; x += 20 + getRandomValue(0, 30)) {
  //   SDL_SetRenderDrawColor(renderer, 4, 137, 3, 255);
  //   SDL_RenderDrawLine(renderer, x, (height - heightMap[x]), x,
  //                      (height - heightMap[x]) - 15);
  //   SDL_RenderDrawLine(renderer, x, (height - heightMap[x]), x - 5,
  //                      (height - heightMap[x]) - 10);
  //   SDL_RenderDrawLine(renderer, x, (height - heightMap[x]), x + 5,
  //                      (height - heightMap[x]) - 10);
  // }

  SDL_SetRenderTarget(renderer, NULL);

  return texture;
}

// main game loop
static void playMain(App* app, uint32_t SEED) {
  char temp[256];
  sprintf(temp, "%smedia/fonts/PixeloidSans.ttf", app->basePath);
  TTF_Font* smallFont = loadFont(temp, 30);

  uint32_t mapSeed;
  SDL_bool wasLoaded = SDL_FALSE;

  // if save was loaded
  if (SEED == 1000000001u) {
    wasLoaded = SDL_TRUE;
  }

  // if seed wasn't set
  if (SEED == 1000000000u) {
    srand(time(NULL));
    mapSeed = rand();
    log_info("setting gen map seed to: RAND - %u", mapSeed);

  } else {
    mapSeed = SEED;
    log_info("setting gen map seed to fixed: %u", mapSeed);
  }

  int32_t x1;
  int32_t x2;

  // default heightMap for saving map levels
  int32_t* heightMap = (int32_t*)malloc(app->screenWidth * sizeof(int32_t));
  // baseMap used for pretty color output even when smth is destroyed
  int32_t* basedMap = (int32_t*)malloc(app->screenWidth * sizeof(int32_t));
  if (!heightMap || !basedMap) exit(0);
  Player firstPlayer;
  Player secondPlayer;

  // if player pressed "LOAD" button
  if (wasLoaded) {
    switch (
        loadSavedState(app, &firstPlayer, &secondPlayer, heightMap, &mapSeed)) {
      // incorrect saveFile
      case 1:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "SAVE_LOAD_ERROR",
                                 "save is broken! :(", app->window);
        app->currState = PREGAME_SETTING;
        return;
      // incorrect screen info in saveFile
      case 2:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "SAVE_LOAD_ERROR",
                                 "incorrect screen size! :(", app->window);
        app->currState = PREGAME_SETTING;
        return;
      default:
        break;
    }
    x1 = firstPlayer.x;
    x2 = secondPlayer.x;
    genHeightMap(basedMap, mapSeed, app->screenWidth, app->screenHeight);
    addSpawnPlates(basedMap, app->screenWidth, 70 * app->scalingFactorX);
  } else {
    genHeightMap(heightMap, mapSeed, app->screenWidth, app->screenHeight);
    addSpawnPlates(heightMap, app->screenWidth, 70 * app->scalingFactorX);
    memcpy(basedMap, heightMap, app->screenWidth * sizeof(int32_t));
    x1 = 20;
    x2 = app->screenWidth / app->scalingFactorX - 44 - 20;
    app->timesPlayed = 0;
  }

  // height map will contain the 'base' for map (for rendering different
  // colors)
  SDL_Texture* gameMap = saveRenderMapToTexture(
      app->renderer, app->screenWidth, app->screenHeight, heightMap, basedMap);

  // fuck it just using a simple angle calc :-)
  double anglePlayer1 = getAngle((x1 + 5) * app->scalingFactorX, heightMap,
                                 25 * app->scalingFactorX);
  double anglePlayer2 = getAngle((x2 + 8) * app->scalingFactorX, heightMap,
                                 25 * app->scalingFactorX);

  // 1st player textures
  RenderObject* Player1Tank = createRenderObject(
      app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
      "media/imgs/tank_betment.png",
      &(SDL_Point){x1,
                   -27 + app->screenHeight / app->scalingFactorY -
                       heightMap[(int32_t)((x1 + 5) * app->scalingFactorX)] /
                           app->scalingFactorY},
      360 - anglePlayer1, SDL_FLIP_NONE,
      &(SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY});

  RenderObject* Player1Gun = createRenderObject(
      app->renderer, TEXTURE | EXTENDED | DOUBLE_EXTENDED, 1, LEFT_GUN,
      "media/imgs/bet_dulo.png",
      &(SDL_Point){x1,
                   -27 + app->screenHeight / app->scalingFactorY -
                       heightMap[(int32_t)((x1 + 5) * app->scalingFactorX)] /
                           app->scalingFactorY},
      360 - anglePlayer1, 0.0, SDL_FLIP_NONE,
      &(SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY},
      &(SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});

  // 2nd player textures
  RenderObject* Player2Tank = createRenderObject(
      app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
      "media/imgs/tank_jonkler.png",
      &(SDL_Point){x2,
                   -27 + app->screenHeight / app->scalingFactorY -
                       heightMap[(int32_t)((x2 + 8) * app->scalingFactorX)] /
                           app->scalingFactorY},
      360 - anglePlayer2, SDL_FLIP_HORIZONTAL,
      &(SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY});

  RenderObject* Player2Gun = createRenderObject(
      app->renderer, TEXTURE | EXTENDED | DOUBLE_EXTENDED, 1, RIGHT_GUN,
      "media/imgs/jonk_dulo.png",
      &(SDL_Point){x2,
                   -27 + app->screenHeight / app->scalingFactorY -
                       heightMap[(int32_t)((x2 + 8) * app->scalingFactorX)] /
                           app->scalingFactorY},
      360 - anglePlayer2, 0.0, SDL_FLIP_HORIZONTAL,
      &(SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY},
      &(SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});

  RenderObject* betmentAvatar = createRenderObject(
      app->renderer, GIF, 1, b_NONE, "media/imgs/betment.png",
      &(SDL_Point){12, 30}, 39, 2, SDL_FALSE);

  RenderObject* jonklerAvatar =
      createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                         "media/imgs/jonkler.png", &(SDL_Point){0, 30});
  jonklerAvatar->data.texture.constRect.x =
      app->screenWidth / app->scalingFactorX - 12 -
      jonklerAvatar->data.texture.constRect.w;

  RenderObject* explosionObj = createRenderObject(
      app->renderer, GIF, 1, b_NONE, "media/imgs/explosion.png",
      &(SDL_Point){0, 30}, 24, 3, SDL_TRUE);
  explosionObj->disableRendering = SDL_TRUE;

  RenderObject* arrow =
      createRenderObject(app->renderer, GIF, 1, b_NONE, "media/imgs/arrow.png",
                         &(SDL_Point){0, 0}, 8, 3, SDL_FALSE);

  RenderObject* projectile =
      createRenderObject(app->renderer, TEXTURE, 1, b_NONE,
                         "media/imgs/proj.png", &(SDL_Point){0, 0});
  projectile->disableRendering = SDL_TRUE;

  RenderObject* bulletPath =
      createRenderObject(app->renderer, EMPTY, 0, b_NONE, 333, 333);

  RenderObject* speedLabelObject =
      createRenderObject(app->renderer, TEXT, 1, b_NONE, "1337", smallFont,
                         &(SDL_Point){0, 0}, &(SDL_Color){255, 255, 255, 255});
  speedLabelObject->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       speedLabelObject->data.texture.constRect.w) /
      2;
  speedLabelObject->data.texture.constRect.y = 80;

  RenderObject* directionIconObject = createRenderObject(
      app->renderer, TEXTURE | EXTENDED, 1, b_NONE,
      "media/imgs/windDirection.png", &(SDL_Point){0, 0}, 0, SDL_FLIP_NONE,
      &(SDL_Point){32 * app->scalingFactorX, 32 * app->scalingFactorY});

  directionIconObject->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       directionIconObject->data.texture.constRect.w) /
      2;
  directionIconObject->data.texture.constRect.y = 10;

  app->globalConditions.wind.speedLabel = speedLabelObject;
  app->globalConditions.wind.directionIcon = directionIconObject;

  firstPlayer.tankObj = Player1Tank;
  firstPlayer.tankGunObj = Player1Gun;
  secondPlayer.tankObj = Player2Tank;
  secondPlayer.tankGunObj = Player2Gun;
  firstPlayer.inAnimation = SDL_FALSE;
  secondPlayer.inAnimation = SDL_FALSE;

  // if settings wasnt loaded
  // setting up the 'default' settings
  if (!wasLoaded) {
    log_info("using default settings for players!");

    firstPlayer.movesLeft = 9;
    firstPlayer.gunAngle = 0.0;
    firstPlayer.firingPower = 30;
    firstPlayer.tankAngle = anglePlayer1;
    firstPlayer.inAnimation = SDL_FALSE;
    firstPlayer.x = 30;
    firstPlayer.score = 0;
    switch (app->p1Diff) {
      case b_P1Player:
        firstPlayer.type = MONKE;
        break;
      case b_P1Easy:
        firstPlayer.type = EASY;
        break;
      case b_P1Normal:
        firstPlayer.type = NORMAL;
        break;
      case b_P1Hard:
        firstPlayer.type = HARD;
        break;
      default:
        break;
    }

    secondPlayer.movesLeft = 9;
    secondPlayer.gunAngle = 0.0;
    secondPlayer.firingPower = 30;
    secondPlayer.tankAngle = anglePlayer2;
    secondPlayer.inAnimation = SDL_FALSE;
    secondPlayer.type = MONKE;
    secondPlayer.x = app->screenWidth / app->scalingFactorX - 44 - 30;
    secondPlayer.score = 0;
    switch (app->p2Diff) {
      case b_P2Player:
        secondPlayer.type = MONKE;
        break;
      case b_P2Easy:
        secondPlayer.type = EASY;
        break;
      case b_P2Normal:
        secondPlayer.type = NORMAL;
        break;
      case b_P2Hard:
        secondPlayer.type = HARD;
        break;
      default:
        break;
    }

    app->currPlayer = &firstPlayer;

    saveCurrentState(app, &firstPlayer, &secondPlayer, heightMap, SDL_TRUE,
                     mapSeed);
  }

  struct UpdateConditions {
    SDL_bool updateWind;
  } updateConditions;
  SDL_bool regenMap = SDL_FALSE;
  SDL_bool recalcBulletPath = SDL_TRUE;
  updateConditions = (struct UpdateConditions){
      .updateWind = SDL_TRUE,
  };

  struct paramsStruct {
    App* app;
    Player* firstPlayer;
    Player* secondPlayer;
    int32_t* heightMap;
    RenderObject* projectile;
    RenderObject* explosion;
    SDL_bool* regenMap;
    SDL_bool* recalcBulletPath;
    struct UpdateConditions* updateConditions;
    uint32_t mapSeed;
  };

  struct paramsStruct playerMove_Params = {
      .app = app,
      .firstPlayer = &firstPlayer,
      .secondPlayer = &secondPlayer,
      .heightMap = heightMap,
      .projectile = projectile,
      .explosion = explosionObj,
      .regenMap = &regenMap,
      .recalcBulletPath = &recalcBulletPath,
      .updateConditions = &updateConditions,
      .mapSeed = mapSeed,
  };

  recalcPlayerPos(app, &firstPlayer, heightMap, 0, 5);
  recalcPlayerPos(app, &secondPlayer, heightMap, 0, 8);
  recalcPlayerGunAngle(&firstPlayer, 0);
  recalcPlayerGunAngle(&secondPlayer, 0);

  // setting init or loaded angles for the gun
  Player1Gun->data.texture.angleAlt = -firstPlayer.gunAngle;
  Player2Gun->data.texture.angleAlt = -secondPlayer.gunAngle;

  // creating thread for a proceeding player movements
  SDL_Thread* playerMoveThread =
      SDL_CreateThread(playerMove, NULL, (void*)&playerMove_Params);

  // old values for optimized update
  //just to be sure we rendering pointing arrow correctly
  double oldAngle = app->currPlayer->gunAngle - 1;
  int32_t oldMovesLeft = app->currPlayer->movesLeft;
  int32_t oldFiringPower = app->currPlayer->firingPower;
  int32_t oldX = app->currPlayer->x;
  int32_t oldScorePlayer1 = firstPlayer.score;
  int32_t oldScorePlayer2 = secondPlayer.score;
  int32_t oldWeapon = app->currWeapon;

  sprintf(temp, "Moves left: %d Gun angle: %02d Firing power: %02d ",
          oldMovesLeft, (int32_t)oldAngle, oldFiringPower);

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

  RenderObject* currentPlayerInfo = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, temp, smallFont,
      &(SDL_Point){10, app->screenHeight / app->scalingFactorY - 40},
      &(SDL_Color){255, 255, 255, 255});

  sprintf(temp, "SCORE: %4d", firstPlayer.score);
  RenderObject* playerScore1 = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, temp, smallFont,
      &(SDL_Point){10, betmentAvatar->data.texture.constRect.y +
                           betmentAvatar->data.texture.constRect.h + 10},
      &(SDL_Color){168, 0, 0, 255});

  sprintf(temp, "SCORE: %4d", secondPlayer.score);
  RenderObject* playerScore2 = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, temp, smallFont,
      &(SDL_Point){0, betmentAvatar->data.texture.constRect.y +
                          betmentAvatar->data.texture.constRect.h + 10},
      &(SDL_Color){0, 168, 107, 255});

  playerScore2->data.texture.constRect.x =
      app->screenWidth / app->scalingFactorX -
      playerScore2->data.texture.constRect.w - 30;

  if (app->currPlayer == &secondPlayer) {
    currentPlayerInfo->data.texture.constRect.x =
        app->screenWidth / app->scalingFactorX -
        currentPlayerInfo->data.texture.constRect.w - 10;
  }

  RenderObject* objectsArr[] = {
      currentPlayerInfo,
      projectile,
      betmentAvatar,
      jonklerAvatar,
      playerScore1,
      playerScore2,
      arrow,
      firstPlayer.tankGunObj,
      firstPlayer.tankObj,
      secondPlayer.tankGunObj,
      secondPlayer.tankObj,
      explosionObj,
      bulletPath,
      speedLabelObject,
      directionIconObject,
  };

  while (app->currState == PLAY) {
    pollAllEvents(app);
    while (app->currWeapon == -1) {
      app->currWeapon = getAllowedNumber(app);
    }

    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);

    // rendering map
    if (regenMap || 1) {
      SDL_DestroyTexture(gameMap);
      gameMap = saveRenderMapToTexture(app->renderer, app->screenWidth,
                                       app->screenHeight, heightMap, basedMap);
      regenMap = SDL_FALSE;
    }

    SDL_RenderCopy(app->renderer, gameMap, NULL, NULL);

    // filling rect for info at the bottom
    SDL_SetRenderDrawColor(app->renderer, 10, 10, 10, 255);
    SDL_RenderFillRect(
        app->renderer,
        &(SDL_Rect){0, app->screenHeight - 40 * app->scalingFactorY,
                    app->screenWidth, app->screenHeight});
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);

    if (updateConditions.updateWind) {
      updateWind(app);
      updateConditions.updateWind = SDL_FALSE;
    }

    // recalc bullet path if needed
    if (recalcBulletPath) {
      renderBulletPath(app, bulletPath);
      recalcBulletPath = SDL_FALSE;
    }

    // redrawing info texture
    if (oldAngle != app->currPlayer->gunAngle ||
        oldFiringPower != app->currPlayer->firingPower ||
        oldMovesLeft != app->currPlayer->movesLeft ||
        oldX != app->currPlayer->x || oldScorePlayer1 != firstPlayer.score ||
        oldScorePlayer2 != secondPlayer.score || app->currWeapon != oldWeapon) {
      SDL_DestroyTexture(currentPlayerInfo->data.texture.texture);

      oldAngle = app->currPlayer->gunAngle;
      oldMovesLeft = app->currPlayer->movesLeft;
      oldFiringPower = app->currPlayer->firingPower;
      oldX = app->currPlayer->x;
      oldWeapon = app->currWeapon;

      sprintf(temp, "Moves left: %d Gun angle: %02d Firing power: %02d ",
              oldMovesLeft, (int32_t)oldAngle, oldFiringPower);

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

      currentPlayerInfo->data.texture.texture =
          createTextTexture(app->renderer, smallFont, temp, 255, 255, 255, 255);

      SDL_QueryTexture(currentPlayerInfo->data.texture.texture, NULL, NULL,
                       &currentPlayerInfo->data.texture.constRect.w,
                       &currentPlayerInfo->data.texture.constRect.h);

      if (app->currPlayer == &firstPlayer) {
        currentPlayerInfo->data.texture.constRect.x = 10;

        // moving arrow to "follow" firstPlayer
        arrow->data.texture.constRect.y =
            Player1Tank->data.texture.constRect.y -
            Player1Tank->data.texture.constRect.w *
                fabs(sin(DEGTORAD(Player1Tank->data.texture.angle))) -
            80;
        arrow->data.texture.constRect.x =
            Player1Tank->data.texture.constRect.x - 15;
        arrow->data.texture.flipFlag = SDL_FLIP_NONE;
      } else {
        currentPlayerInfo->data.texture.constRect.x =
            app->screenWidth / app->scalingFactorX -
            currentPlayerInfo->data.texture.constRect.w - 10;

        // moving arrow to "follow" secondPlayer
        arrow->data.texture.constRect.y =
            Player2Tank->data.texture.constRect.y -
            Player2Tank->data.texture.constRect.w *
                fabs(sin(DEGTORAD(Player2Tank->data.texture.angle))) -
            80;
        arrow->data.texture.constRect.x = Player2Tank->data.texture.constRect.x;
        arrow->data.texture.flipFlag = SDL_FLIP_HORIZONTAL;
      }
    }

    if (oldScorePlayer1 != firstPlayer.score) {
      SDL_DestroyTexture(playerScore1->data.texture.texture);

      oldScorePlayer1 = firstPlayer.score;

      sprintf(temp, "SCORE: %4d", oldScorePlayer1);

      playerScore1->data.texture.texture =
          createTextTexture(app->renderer, smallFont, temp, 168, 0, 0, 255);

      SDL_QueryTexture(playerScore1->data.texture.texture, NULL, NULL,
                       &playerScore1->data.texture.constRect.w,
                       &playerScore1->data.texture.constRect.h);
    }

    if (oldScorePlayer2 != secondPlayer.score) {
      SDL_DestroyTexture(playerScore2->data.texture.texture);

      oldScorePlayer2 = secondPlayer.score;

      sprintf(temp, "SCORE: %4d", oldScorePlayer2);

      playerScore2->data.texture.texture =
          createTextTexture(app->renderer, smallFont, temp, 0, 168, 107, 255);

      SDL_QueryTexture(playerScore2->data.texture.texture, NULL, NULL,
                       &playerScore2->data.texture.constRect.w,
                       &playerScore2->data.texture.constRect.h);
    }

    renderTextures(app, objectsArr, sizeof(objectsArr) / sizeof(*objectsArr),
                   SDL_TRUE);

    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);
  }

  SDL_WaitThread(playerMoveThread, NULL);

  freeRenderObject(Player1Tank);
  freeRenderObject(Player1Gun);
  freeRenderObject(Player2Tank);
  freeRenderObject(Player2Gun);
  freeRenderObject(currentPlayerInfo);
  freeRenderObject(betmentAvatar);
  freeRenderObject(jonklerAvatar);
  freeRenderObject(arrow);
  freeRenderObject(projectile);
  freeRenderObject(explosionObj);
  freeRenderObject(playerScore1);
  freeRenderObject(playerScore2);
  freeRenderObject(bulletPath);
  freeRenderObject(speedLabelObject);
  freeRenderObject(directionIconObject);

  TTF_CloseFont(smallFont);
  SDL_DestroyTexture(gameMap);

  free(heightMap);
  free(basedMap);
}

// pre game (like settings before game starts :-) )
void preGameMain(App* app) {
  app->p1Diff = b_NULL;
  app->p2Diff = b_NULL;

  char temp[256];

  sprintf(temp, "%smedia/fonts/PixeloidSans.ttf", app->basePath);
  TTF_Font* smallFont = loadFont(temp, 30);

  sprintf(temp, "%smedia/fonts/PixeloidSans-Bold.ttf", app->basePath);
  TTF_Font* mainFont = loadFont(temp, 60);

  RenderObject* seedText =
      createRenderObject(app->renderer, TEXT, 1, b_NONE, "SEED:", smallFont,
                         &(SDL_Point){0, 0}, &(SDL_Color){128, 128, 128, 255});
  seedText->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       seedText->data.texture.constRect.w);
  seedText->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY -
       seedText->data.texture.constRect.h) /
          2 -
      300;

  RenderObject* seedInput =
      createRenderObject(app->renderer, TEXT_INPUT, 1, bTI_SEED,
                         &(SDL_Rect){seedText->data.texture.constRect.x - 5,
                                     seedText->data.texture.constRect.y +
                                         seedText->data.texture.constRect.h,
                                     200, 50},
                         9, 2, smallFont);

  RenderObject* loadTextButton = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_PREGAME_LOAD, "LOAD SAVE",
      mainFont, &(SDL_Point){0, 0}, &(SDL_Color){128, 128, 128, 255},
      &(SDL_Color){230, 25, 25, 255});
  loadTextButton->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       loadTextButton->data.texture.constRect.w) /
      2;
  loadTextButton->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY - 170);

  RenderObject* startTextButton = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_PREGAME_START,
      "START NEW GAME", mainFont, &(SDL_Point){0, 0},
      &(SDL_Color){0, 255, 189, 200}, &(SDL_Color){230, 25, 25, 255});
  startTextButton->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       startTextButton->data.texture.constRect.w) /
      2;
  startTextButton->data.texture.constRect.y =
      (app->screenHeight / app->scalingFactorY - 100);

  RenderObject* difficultyText = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "DIFFICULTY", mainFont,
      &(SDL_Point){0, seedInput->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  difficultyText->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX -
       difficultyText->data.texture.constRect.w) /
      2;

  // player1 difficulty choice
  RenderObject* Player1Diff = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "Player 1: ", smallFont,
      &(SDL_Point){30, difficultyText->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  Player1Diff->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       Player1Diff->data.texture.constRect.w) /
      2;

  // player
  RenderObject* Player1Diff_p = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1Player, "livin' man",
      smallFont, &(SDL_Point){0, Player1Diff->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player1Diff_p->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       Player1Diff_p->data.texture.constRect.w) /
      2;

  // easy diff
  RenderObject* Player1Diff_bE = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1Easy, "BOT: kid",
      smallFont, &(SDL_Point){0, Player1Diff_p->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player1Diff_bE->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       Player1Diff_bE->data.texture.constRect.w) /
      2;

  // normal diff
  RenderObject* Player1Diff_bN = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1Normal, "BOT: normal",
      smallFont, &(SDL_Point){0, Player1Diff_bE->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player1Diff_bN->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       Player1Diff_bN->data.texture.constRect.w) /
      2;

  // hard diff
  RenderObject* Player1Diff_bH = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P1Hard, "BOT: Bring 'em on!",
      smallFont, &(SDL_Point){0, Player1Diff_bN->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player1Diff_bH->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX / 2 -
       Player1Diff_bH->data.texture.constRect.w) /
      2;

  // player2 difficulty choice
  RenderObject* Player2Diff = createRenderObject(
      app->renderer, TEXT, 1, b_NONE, "Player 2:", smallFont,
      &(SDL_Point){0, difficultyText->data.texture.constRect.y + 100},
      &(SDL_Color){128, 128, 128, 255});

  Player2Diff->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       Player2Diff->data.texture.constRect.w) /
      2;

  // player
  RenderObject* Player2Diff_p = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2Player, "livin' man",
      smallFont, &(SDL_Point){0, Player2Diff->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player2Diff_p->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       Player2Diff_p->data.texture.constRect.w) /
      2;

  // easy diff
  RenderObject* Player2Diff_bE = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2Easy, "BOT: kid",
      smallFont, &(SDL_Point){0, Player2Diff_p->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player2Diff_bE->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       Player2Diff_bE->data.texture.constRect.w) /
      2;

  // normal diff
  RenderObject* Player2Diff_bN = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2Normal, "BOT: normal",
      smallFont, &(SDL_Point){0, Player2Diff_bE->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player2Diff_bN->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       Player2Diff_bN->data.texture.constRect.w) /
      2;

  // hard diff
  RenderObject* Player2Diff_bH = createRenderObject(
      app->renderer, TEXT | CAN_BE_TRIGGERED, 1, b_P2Hard, "BOT: Bring 'em on!",
      smallFont, &(SDL_Point){0, Player2Diff_bN->data.texture.constRect.y + 50},
      &(SDL_Color){230, 230, 230, 255}, &(SDL_Color){230, 25, 25, 255});

  Player2Diff_bH->data.texture.constRect.x =
      (app->screenWidth / app->scalingFactorX * 3 / 2 -
       Player2Diff_bH->data.texture.constRect.w) /
      2;

  RenderObject* objectsArr[] = {
      difficultyText, Player1Diff,    Player1Diff_p,  Player1Diff_bE,
      Player1Diff_bN, Player1Diff_bH, Player2Diff,    Player2Diff_p,
      Player2Diff_bE, Player2Diff_bN, Player2Diff_bH, seedText,
      seedInput,      loadTextButton, startTextButton};

  while (app->currState == PREGAME_SETTING) {
    pollAllEvents(app);
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);

    // lines in the middle of the screen
    SDL_SetRenderDrawColor(app->renderer, 128, 128, 128, 255);
    SDL_RenderDrawLine(
        app->renderer, 0,
        (difficultyText->data.texture.constRect.y - 10) * app->scalingFactorY,
        app->screenWidth,
        (difficultyText->data.texture.constRect.y - 10) * app->scalingFactorY);
    SDL_RenderDrawLine(
        app->renderer, 0,
        (difficultyText->data.texture.constRect.y + 90) * app->scalingFactorY,
        app->screenWidth,
        (difficultyText->data.texture.constRect.y + 90) * app->scalingFactorY);
    SDL_RenderDrawLine(
        app->renderer, app->screenWidth / 2,
        (difficultyText->data.texture.constRect.y + 90) * app->scalingFactorY,
        app->screenWidth / 2,
        (loadTextButton->data.texture.constRect.y - 10) * app->scalingFactorY);
    SDL_RenderDrawLine(
        app->renderer, 0,
        (loadTextButton->data.texture.constRect.y - 10) * app->scalingFactorY,
        app->screenWidth,
        (loadTextButton->data.texture.constRect.y - 10) * app->scalingFactorY);
    //

    renderTextures(app, objectsArr, sizeof(objectsArr) / sizeof(*objectsArr),
                   SDL_TRUE);

    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);
  }

  memcpy(temp, seedInput->data.textInputLine.savedText,
         seedInput->data.textInputLine.maxInputChars);
  SDL_DestroyTexture(seedInput->data.textInputLine.textTexture);
  TTF_CloseFont(smallFont);
  TTF_CloseFont(mainFont);
  freeRenderObject(seedText);
  freeRenderObject(startTextButton);
  freeRenderObject(loadTextButton);
  freeRenderObject(Player1Diff);
  freeRenderObject(Player1Diff_p);
  freeRenderObject(Player1Diff_bE);
  freeRenderObject(Player1Diff_bN);
  freeRenderObject(Player1Diff_bH);
  freeRenderObject(Player2Diff);
  freeRenderObject(Player2Diff_p);
  freeRenderObject(Player2Diff_bE);
  freeRenderObject(Player2Diff_bN);
  freeRenderObject(Player2Diff_bH);
  freeRenderObject(difficultyText);
  free(seedInput);

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
