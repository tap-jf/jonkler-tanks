#ifndef PLAY_RENDER_H
#define PLAY_RENDER_H

#include "../App.h"

struct UpdateConditions {
  SDL_bool updateWind;
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
  SDL_bool* hideBulletPath;
  SDL_bool* isSpinning;
  struct UpdateConditions* updateConditions;
  uint32_t mapSeed;
};

struct playMainObjects {
  TTF_Font* smallFont;
  uint32_t mapSeed;
  int32_t* heightMap;
  int32_t* basedMap;
  Player firstPlayer;
  Player secondPlayer;
  SDL_Texture* gameMap;
  RenderObject* Player1Tank;
  RenderObject* Player1Gun;
  RenderObject* Player2Tank;
  RenderObject* Player2Gun;
  RenderObject* betmentAvatar;
  RenderObject* emoji;
  RenderObject* jonklerAvatar;
  RenderObject* explosionObj;
  RenderObject* arrow;
  RenderObject* projectile;
  RenderObject* bulletPath;
  RenderObject* spreadArea;
  RenderObject* speedLabelObject;
  RenderObject* directionIconObject;

  struct UpdateConditions updateConditions;
  SDL_bool regenMap;
  SDL_bool hideBulletPath;
  SDL_bool recalcBulletPath;
  struct paramsStruct playerMove_Params;

  RenderObject* tree1;
  RenderObject* tree2;
  RenderObject* tree3;
  RenderObject* tree4;
  RenderObject* tree5;
  RenderObject* cloud1;
  RenderObject* cloud2;
  RenderObject* cloud3;
  RenderObject* cloud4;
  RenderObject* cloud5;
  RenderObject* cloud6;
  RenderObject* cloud7;
  RenderObject* cloud8;
  RenderObject* cloud9;
  RenderObject* cloud10;
  RenderObject* cloud11;
  RenderObject* cloud12;
  RenderObject* cloud13;
  RenderObject* cloud14;
  RenderObject* stone1;
  RenderObject* stone2;

  SDL_Thread* playerMoveThread;

  // old values
  double oldAngle;
  int32_t oldMovesLeft;
  int32_t oldFiringPower;
  int32_t oldX;
  int32_t oldScorePlayer1;
  int32_t oldScorePlayer2;
  int32_t oldWeapon;

  RenderObject* currentPlayerInfo;
  RenderObject* playerScore1;
  RenderObject* playerScore2;
  RenderObject* p1DoubleDmgIcon;
  RenderObject* p1ShieldIcon;
  RenderObject* p2DoubleDmgIcon;
  RenderObject* p2ShieldIcon;

  RenderObject* eventSpinText;
  SDL_bool showEventSpin;
  SDL_bool resultShown;
  SDL_bool isSpinning;
  uint32_t eventSpinStartTime;
  Player* oldCurrPlayer;
};

typedef struct PreGameMainObjects {
  TTF_Font* smallFont;
  TTF_Font* mainFont;

  RenderObject* seedText;
  RenderObject* seedInput;
  RenderObject* loadTextButton;
  RenderObject* startTextButton;
  RenderObject* difficultyText;

  RenderObject* Player1DiffLabel;
  RenderObject* Player1Diff_p;
  RenderObject* Player1Diff_bE;
  RenderObject* Player1Diff_bN;
  RenderObject* Player1Diff_bH;

  RenderObject* Player2DiffLabel;
  RenderObject* Player2Diff_p;
  RenderObject* Player2Diff_bE;
  RenderObject* Player2Diff_bN;
  RenderObject* Player2Diff_bH;

} PreGameMainObjects;

void preGameMain(App* app);

#endif