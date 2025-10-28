#include "SDL_render.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "../math/math.h"
#include "event_handlers.h"
#include "log/log.h"

int32_t ANGLE_MUSHROOM_MODE = 0;

/*  This function creates renderObject with different internals and accepts different amount of args
 *  1) if flags & TEXT it will accept args in this order:
 *      a. const char* textToInsert, TTF_Font* font, SDL_Point* position, SDL_Color* normalColor
 *      b. if flags & CAN_BE_TRIGGERED it will accept one more argument SDL_Color* triggeredColor
 *  2) if flags & TRIANGLE it will accept args in this order:
 *      a. SDL_Point* v1, SDL_Point* v2, SDL_Point* v3, SDL_Color* normalColor --> v(?) stands for vertex coords
 *      b. if flags & CAN_BE_TRIGGERED it will accept one more argument SDL_Color* triggeredColor
 *  3) if flags & TEXTURE it will accept args in this order:
 *      a. const char* path, SDL_Point* position
 *      b. if flags & EXTENDED_RENDER it will accept 5 more argument double angle, double angleAlt, SDL_RendererFlip flip, SDL_Point* centerRot, SDL_Point* centerRot_Alt
 *  4) if flags & SLIDER it will accept args in this order:
 *      a. SDL_Rect* rect
 *  5) if flags & TEXT_INPUT it will accept args in this order:
 *      a. SDL_Rect* rect, int32_t maxInputChars, int32_t charTypes, TTF_Font* font
 *  6) if flags & GIF:
 *      a. const char* pathToFolder, SDL_Point* pos, int32_t framesCnt
 *  7) if flags & EMPTY (create texture for rendering):
 *      a. int32_t w, int32_t h 
 */
RenderObject* createRenderObject(SDL_Renderer* render,
                                 enum RenderObjectMode flags, int32_t zPos,
                                 enum Button buttonName, ...) {
  va_list args;
  va_start(args, buttonName);

  RenderObject* renderObj = (RenderObject*)malloc(sizeof(RenderObject));

  if (renderObj == NULL) {
    log_fatal("error while allocating mem for RenderObject");

    exit(-1);
  }

  memset(renderObj, 0x00, sizeof(RenderObject));

  if (flags & TEXT) {
    // getting args
    const char* textToInsert = va_arg(args, const char*);
    TTF_Font* font = va_arg(args, TTF_Font*);
    SDL_Point* position = va_arg(args, SDL_Point*);

    SDL_Point tempPoint = {0, 0};

    // if pos wasn't set, setting it up
    if (position == NULL) position = &tempPoint;

    SDL_Color* normalColor = va_arg(args, SDL_Color*);

    SDL_Color* triggeredColor = normalColor;
    if (flags & CAN_BE_TRIGGERED) triggeredColor = va_arg(args, SDL_Color*);

    SDL_Texture* normalTextTexture =
        createTextTexture(render, font, textToInsert, normalColor->r,
                          normalColor->g, normalColor->b, normalColor->a);

    SDL_Texture* triggeredTextTexture = NULL;
    if (flags & CAN_BE_TRIGGERED)
      triggeredTextTexture = createTextTexture(
          render, font, textToInsert, triggeredColor->r, triggeredColor->g,
          triggeredColor->b, triggeredColor->a);

    SDL_Rect textRect = {
        .x = position->x,
        .y = position->y,
    };

    SDL_QueryTexture(normalTextTexture, NULL, NULL, &textRect.w, &textRect.h);

    // filling up renderObj
    renderObj->buttonName = buttonName;
    renderObj->canBeTriggered = flags & CAN_BE_TRIGGERED;
    renderObj->Zpos = zPos;
    renderObj->normalColor = *normalColor;
    renderObj->triggeredColor = *triggeredColor;
    renderObj->data.texture.constRect = textRect;
    renderObj->data.texture.scaleRect = textRect;
    renderObj->data.texture.texture = normalTextTexture;
    renderObj->data.texture.triggeredTexture = triggeredTextTexture;
  } else if (flags & TEXTURE) {
    // getting args
    const char* path = va_arg(args, const char*);
    SDL_Point* position = va_arg(args, SDL_Point*);

    // args for extended rendering
    if (flags & EXTENDED) {
      renderObj->data.texture.angle = va_arg(args, double);
      if (flags & DOUBLE_EXTENDED) {
        renderObj->data.texture.angleAlt = va_arg(args, double);
      } else {
        renderObj->data.texture.angleAlt = 0.0;
      }
      renderObj->data.texture.flipFlag = va_arg(args, SDL_RendererFlip);
      renderObj->data.texture.centerRot = va_arg(args, SDL_Point*);
      if (flags & DOUBLE_EXTENDED) {
        renderObj->data.texture.centerRot_Alt = va_arg(args, SDL_Point*);
      } else {
        renderObj->data.texture.centerRot_Alt = NULL;
      }
    }

    SDL_Point tempPoint = {0, 0};

    // if pos wasn't set, setting it up
    if (position == NULL) position = &tempPoint;

    // getting absoulte path
    char temp[256];
    char* basePath = SDL_GetBasePath();
    sprintf(temp, "%s%s", basePath, path);
    free(basePath);

    SDL_Texture* imgTexture = createImgTexture(render, temp);

    SDL_Rect imgRect = {
        .x = position->x,
        .y = position->y,
    };

    SDL_QueryTexture(imgTexture, NULL, NULL, &imgRect.w, &imgRect.h);

    renderObj->buttonName = buttonName;
    renderObj->canBeTriggered = flags & CAN_BE_TRIGGERED;
    renderObj->Zpos = zPos;
    renderObj->data.texture.constRect = imgRect;
    renderObj->data.texture.scaleRect = imgRect;
    renderObj->data.texture.texture = imgTexture;
    renderObj->data.texture.triggeredTexture = imgTexture;
  } else if (flags & TRIANGLE) {
    SDL_Point v1 = *va_arg(args, SDL_Point*);
    SDL_Point v2 = *va_arg(args, SDL_Point*);
    SDL_Point v3 = *va_arg(args, SDL_Point*);

    SDL_Color* normalColor = va_arg(args, SDL_Color*);
    SDL_Color* triggeredColor = normalColor;
    if (flags & CAN_BE_TRIGGERED) triggeredColor = va_arg(args, SDL_Color*);

    renderObj->canBeTriggered = flags & CAN_BE_TRIGGERED;
    renderObj->normalColor = *normalColor;
    renderObj->triggeredColor = *triggeredColor;
    renderObj->Zpos = zPos;
    renderObj->data.triangle.p1C = v1;
    renderObj->data.triangle.p2C = v2;
    renderObj->data.triangle.p3C = v3;
    renderObj->data.triangle.p1S = v1;
    renderObj->data.triangle.p2S = v2;
    renderObj->data.triangle.p3S = v3;
    renderObj->buttonName = buttonName;
  } else if (flags & SLIDER) {
    SDL_Rect sliderRect = *va_arg(args, SDL_Rect*);

    renderObj->data.texture.constRect = sliderRect;
    renderObj->data.texture.scaleRect = sliderRect;
    renderObj->buttonName = buttonName;

  } else if (flags & TEXT_INPUT) {
    SDL_Rect textInputRect = *va_arg(args, SDL_Rect*);
    int32_t maxInputChars = va_arg(args, int32_t);
    int32_t charTypes = va_arg(args, int32_t);
    TTF_Font* font = va_arg(args, TTF_Font*);

    renderObj->data.textInputLine.constRect = textInputRect;
    renderObj->data.textInputLine.scaleRect = textInputRect;
    renderObj->data.textInputLine.charTypes = charTypes;
    renderObj->data.textInputLine.maxInputChars = maxInputChars;
    renderObj->data.textInputLine.font = font;
    renderObj->buttonName = buttonName;
  } else if (flags & GIF) {
    const char* path = va_arg(args, char*);
    SDL_Point* position = va_arg(args, SDL_Point*);
    int32_t framesCnt = va_arg(args, int32_t);
    int32_t framesWaitCoeff = va_arg(args, int32_t);
    SDL_bool singleShot = va_arg(args, SDL_bool);

    char temp[256];
    char* basePath = SDL_GetBasePath();
    sprintf(temp, "%s%s", basePath, path);

    SDL_Texture* texture = createImgTexture(render, temp);

    SDL_Rect imgRect = {
        .x = position->x,
        .y = position->y,
    };

    SDL_QueryTexture(texture, NULL, NULL, &imgRect.w, &imgRect.h);

    imgRect.w /= framesCnt;

    renderObj->buttonName = buttonName;
    renderObj->canBeTriggered = flags & CAN_BE_TRIGGERED;
    renderObj->Zpos = zPos;
    renderObj->data.texture.texture = texture;
    renderObj->data.texture.constRect = imgRect;
    renderObj->data.texture.scaleRect = imgRect;
    renderObj->data.texture.fixedHeight = imgRect.h;
    renderObj->data.texture.fixedWidth = imgRect.w;
    renderObj->data.texture.maxFrames = framesCnt;
    renderObj->data.texture.framesWaitCoeff = framesWaitCoeff;
    renderObj->data.texture.singleShot = singleShot;
  } else if (flags & EMPTY) {
    int32_t w = va_arg(args, int32_t);
    int32_t h = va_arg(args, int32_t);

    renderObj->data.texture.texture = SDL_CreateTexture(
        render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    renderObj->data.texture.constRect.w = w;
    renderObj->data.texture.constRect.h = h;
    renderObj->data.texture.triggeredTexture = renderObj->data.texture.texture;
    SDL_SetTextureBlendMode(renderObj->data.texture.texture,
                            SDL_BLENDMODE_BLEND);
  }

  va_end(args);

  return renderObj;
}

// function returning SDL_Texture* that contains the img from str
SDL_Texture* createImgTexture(SDL_Renderer* renderer, const char* str) {
  // loading img as a texture
  SDL_Texture* texture = IMG_LoadTexture(renderer, str);

  if (!texture) {
    // if it fails
    log_error("error while loading \"%s\" as texture: %s", str, IMG_GetError());

    return NULL;
  }

  return texture;
}

// function returning TTF_Font* that contains loaded font from str with a size fo ptSize
TTF_Font* loadFont(const char* str, int32_t ptSize) {
  // loading TTF file for a font
  TTF_Font* font = TTF_OpenFont(str, ptSize);

  if (!font) {
    log_error("error while loading \"%s\" as a font: %s", str, TTF_GetError());

    return NULL;
  }

  return font;
}

// function returning a SDL_Texture* that contains the text
SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font,
                               const char* text, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t a) {
  SDL_Color textColor = {.r = r, .g = g, .b = b, .a = a};

  // creating surface from a text with a spec. font
  SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, textColor);
  if (!surface) {
    // if it fails
    log_error("error while creating surface for a font: %s", TTF_GetError());

    return NULL;
  }

  // creating texture from a surface
  SDL_Texture* textImageTexture =
      SDL_CreateTextureFromSurface(renderer, surface);
  // freeing the surface, cuz we dont need it anymore
  SDL_FreeSurface(surface);

  if (!textImageTexture) {
    // if it fails
    log_error("error while creating texture for a surface: %s", SDL_GetError());

    return NULL;
  }

  return textImageTexture;
}

// scale rects without any proportions
static void scaleObjects(App* app, RenderObject* objectsArr[],
                         int32_t objectArrSize) {
  for (int32_t i = 0; i != objectArrSize; ++i) {
    if (objectsArr[i] == NULL) {
      continue;
    }
    // if it is a text input line
    if (objectsArr[i]->buttonName >= 1500 && objectsArr[i]->buttonName < 2000) {
      objectsArr[i]->data.textInputLine.scaleRect.x =
          objectsArr[i]->data.textInputLine.constRect.x * app->scalingFactorX;
      objectsArr[i]->data.textInputLine.scaleRect.y =
          objectsArr[i]->data.textInputLine.constRect.y * app->scalingFactorY;
      objectsArr[i]->data.textInputLine.scaleRect.w =
          objectsArr[i]->data.textInputLine.constRect.w * app->scalingFactorX;
      objectsArr[i]->data.textInputLine.scaleRect.h =
          objectsArr[i]->data.textInputLine.constRect.h * app->scalingFactorY;
    }
    // if objectsArr.data contains rects
    else if (objectsArr[i]->buttonName < 500 ||
             objectsArr[i]->buttonName > 999) {
      objectsArr[i]->data.texture.scaleRect.x =
          objectsArr[i]->data.texture.constRect.x * app->scalingFactorX;
      objectsArr[i]->data.texture.scaleRect.y =
          objectsArr[i]->data.texture.constRect.y * app->scalingFactorY;
      objectsArr[i]->data.texture.scaleRect.w =
          objectsArr[i]->data.texture.constRect.w * app->scalingFactorX;
      objectsArr[i]->data.texture.scaleRect.h =
          objectsArr[i]->data.texture.constRect.h * app->scalingFactorY;
    }
    // if objectsArr.data contains points
    else if (objectsArr[i]->buttonName >= 500) {
      objectsArr[i]->data.triangle.p1S.x =
          objectsArr[i]->data.triangle.p1C.x * app->scalingFactorX;
      objectsArr[i]->data.triangle.p1S.y =
          objectsArr[i]->data.triangle.p1C.y * app->scalingFactorY;
      objectsArr[i]->data.triangle.p2S.x =
          objectsArr[i]->data.triangle.p2C.x * app->scalingFactorX;
      objectsArr[i]->data.triangle.p2S.y =
          objectsArr[i]->data.triangle.p2C.y * app->scalingFactorY;
      objectsArr[i]->data.triangle.p3S.x =
          objectsArr[i]->data.triangle.p3C.x * app->scalingFactorX;
      objectsArr[i]->data.triangle.p3S.y =
          objectsArr[i]->data.triangle.p3C.y * app->scalingFactorY;
    }
  }
}

// draw thick rects
static void drawThickRect(SDL_Renderer* renderer, SDL_Rect rect,
                          int32_t thickness) {
  for (int32_t i = 0; i < thickness; i++) {
    SDL_RenderDrawRect(renderer, &rect);
    rect.x++;
    rect.y++;
    rect.w -= 2;
    rect.h -= 2;
  }
}

// draw fiiled triangle using 3 points (vertexes)
static void drawFilledTriangle(SDL_Renderer* renderer, const SDL_Point* p1,
                               const SDL_Point* p2, const SDL_Point* p3,
                               SDL_Color color) {
  SDL_Vertex vertexes[] = {
      {{p1->x, p1->y}, color, {1, 1}},
      {{p2->x, p2->y}, color, {1, 1}},
      {{p3->x, p3->y}, color, {1, 1}},
  };

  SDL_RenderGeometry(renderer, NULL, vertexes, 3, NULL, 0);
}

// render textures objects with/without scaling
void renderTextures(App* app, RenderObject* objectsArr[],
                    int32_t objectsArrSize, int32_t isScaling) {
  // scale objects
  if (isScaling) {
    scaleObjects(app, objectsArr, objectsArrSize);
  }

  SDL_Rect temp;

  // getitng curr mouse position
  int32_t x, y;
  SDL_GetMouseState(&x, &y);

  // mouse trigger state
  int32_t mouseShouldBeTriggered = SDL_FALSE;

  // rendering objects
  for (int32_t i = 0; i != objectsArrSize; ++i) {
    // skipping if this object shouldn't be rendered or it doesn't exist
    if (objectsArr[i] == NULL || objectsArr[i]->disableRendering) {
      continue;
    }

    RenderObject currRenderObject = *objectsArr[i];

    // if it is a button
    if (currRenderObject.buttonName < 500) {
      // if texture can be triggered
      if (currRenderObject.canBeTriggered &&
          SDL_PointInRect(&(SDL_Point){x, y},
                          &currRenderObject.data.texture.scaleRect)) {
        SDL_RenderCopyEx(
            app->renderer, currRenderObject.data.texture.triggeredTexture, NULL,
            &currRenderObject.data.texture.scaleRect,
            currRenderObject.data.texture.angle + ANGLE_MUSHROOM_MODE,
            currRenderObject.data.texture.centerRot,
            currRenderObject.data.texture.flipFlag);
        mouseShouldBeTriggered = SDL_TRUE;
        app->buttonPosTriggered = currRenderObject.buttonName;
      } else {
        // if its a gif object
        if (currRenderObject.data.texture.maxFrames != 0) {
          int32_t currFrame = (currRenderObject.data.texture.currFrame /
                               currRenderObject.data.texture.framesWaitCoeff) %
                              currRenderObject.data.texture.maxFrames;
          SDL_Rect source = {
              .x = currFrame * currRenderObject.data.texture.fixedWidth,
              .y = 0,
              .h = currRenderObject.data.texture.fixedHeight,
              .w = currRenderObject.data.texture.fixedWidth,
          };

          SDL_RenderCopyEx(
              app->renderer, currRenderObject.data.texture.texture, &source,
              &currRenderObject.data.texture.scaleRect,
              currRenderObject.data.texture.angle + ANGLE_MUSHROOM_MODE,
              currRenderObject.data.texture.centerRot,
              currRenderObject.data.texture.flipFlag);
          objectsArr[i]->data.texture.currFrame++;

          // hiding gif if its shouldn't render rn (was rendered)
          if (objectsArr[i]->data.texture.singleShot &&
              objectsArr[i]->data.texture.currFrame ==
                  currRenderObject.data.texture.maxFrames) {
            objectsArr[i]->disableRendering = SDL_TRUE;
          }
        }
        // if its not
        else {
          SDL_RenderCopyEx(
              app->renderer, currRenderObject.data.texture.texture, NULL,
              &currRenderObject.data.texture.scaleRect,
              currRenderObject.data.texture.angle + ANGLE_MUSHROOM_MODE++,
              currRenderObject.data.texture.centerRot,
              currRenderObject.data.texture.flipFlag);
        }
      }
    }
    // if it is a triangle button
    else if (currRenderObject.buttonName < 1000) {
      if (currRenderObject.canBeTriggered &&
          isInTriangle(x, y, currRenderObject.data.triangle.p1S,
                       currRenderObject.data.triangle.p2S,
                       currRenderObject.data.triangle.p3S)) {
        drawFilledTriangle(app->renderer, &currRenderObject.data.triangle.p1S,
                           &currRenderObject.data.triangle.p2S,
                           &currRenderObject.data.triangle.p3S,
                           (SDL_Color){230, 25, 25, 255});
        mouseShouldBeTriggered = SDL_TRUE;
        app->buttonPosTriggered = currRenderObject.buttonName;
        // if mouse is pressed
        if (app->isMouseDragging) {
          proceedShiftedButtons(app, currRenderObject.buttonName);
        }
      } else {
        drawFilledTriangle(app->renderer, &currRenderObject.data.triangle.p1S,
                           &currRenderObject.data.triangle.p2S,
                           &currRenderObject.data.triangle.p3S,
                           (SDL_Color){128, 128, 128, 255});
      }
    }
    // if it is a slider
    else if (currRenderObject.buttonName < 1500) {
      int32_t sliderPosX = 0;
      // if user is dragging mouse in the ! external ! rect
      if (app->isMouseDragging &&
          SDL_PointInRect(&(SDL_Point){x, y},
                          &currRenderObject.data.texture.scaleRect)) {
        mouseShouldBeTriggered = SDL_TRUE;
        app->buttonPosTriggered = currRenderObject.buttonName;
        // calculating position
        // X = currRenderObject.scaleRect.w - is a 100%, so
        // to calculate current percent
        // (x - currRenderObject.scaleRect.x) / currRenderObject.scaleRect.w
        sliderPosX = (x - currRenderObject.data.texture.scaleRect.x) * 101 /
                     currRenderObject.data.texture.scaleRect.w;
        proceedSlider(app, sliderPosX);
      } else {
        // setting up initial or current pos of a slider
        // so it will be set without dragging on it
        switch (currRenderObject.buttonName) {
          case s_VOLUME:
            sliderPosX = app->settings.currentVolume;
            break;
          default:
            break;
        }
      }
      SDL_SetRenderDrawColor(app->renderer, 128, 128, 128, 255);
      // drawing external rect
      drawThickRect(app->renderer, currRenderObject.data.texture.scaleRect, 4);
      temp = (SDL_Rect){
          .x = currRenderObject.data.texture.scaleRect.x,
          .y = currRenderObject.data.texture.scaleRect.y,
          .w = sliderPosX * currRenderObject.data.texture.scaleRect.w / 100,
          .h = currRenderObject.data.texture.scaleRect.h};
      SDL_RenderFillRect(app->renderer, &temp);
      SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 0);
    }
    // if it is a text input line
    else if (currRenderObject.buttonName < 2000) {
      if (SDL_PointInRect(&(SDL_Point){x, y},
                          &currRenderObject.data.textInputLine.scaleRect)) {
        app->buttonPosTriggered = currRenderObject.buttonName;
        mouseShouldBeTriggered = SDL_TRUE;
      }

      // if this object was triggered by pressing LMB
      if (app->currentTriggeredObject == currRenderObject.buttonName) {
        // red
        SDL_SetRenderDrawColor(app->renderer, 230, 25, 25, 255);
      } else {
        // gray
        SDL_SetRenderDrawColor(app->renderer, 128, 128, 128, 255);
      }

      // rendering outline box
      drawThickRect(app->renderer,
                    currRenderObject.data.textInputLine.scaleRect, 4);

      proceedTextInputLine(app, objectsArr[i]);

      SDL_Rect textRect = {
          .x = currRenderObject.data.textInputLine.scaleRect.x + 10,
          .y = currRenderObject.data.textInputLine.scaleRect.y + 5,
      };

      SDL_QueryTexture(objectsArr[i]->data.textInputLine.textTexture, NULL,
                       NULL, &textRect.w, &textRect.h);

      textRect.w *= app->scalingFactorX;
      textRect.h *= app->scalingFactorY;

      SDL_RenderCopy(app->renderer,
                     objectsArr[i]->data.textInputLine.textTexture, NULL,
                     &textRect);
    }
    // left tank
    else if (currRenderObject.buttonName == 2000) {
      SDL_Point rotatedPoint1 = getPixelScreenPosition(
          (SDL_Point){currRenderObject.data.texture.scaleRect.x,
                      currRenderObject.data.texture.scaleRect.y},
          (SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY},
          currRenderObject.data.texture.angle,
          (SDL_Point){24 * app->scalingFactorX, 7 * app->scalingFactorY});

      currRenderObject.data.texture.scaleRect.x = rotatedPoint1.x;
      currRenderObject.data.texture.scaleRect.y = rotatedPoint1.y;

      SDL_RenderCopyEx(app->renderer, currRenderObject.data.texture.texture,
                       NULL, &currRenderObject.data.texture.scaleRect,
                       currRenderObject.data.texture.angle +
                           currRenderObject.data.texture.angleAlt,
                       &(SDL_Point){0, 1}, SDL_FLIP_NONE);
    }
    // right tank
    else if (currRenderObject.buttonName == 2001) {
      SDL_Point rotatedPoint1 = getPixelScreenPosition(
          (SDL_Point){currRenderObject.data.texture.scaleRect.x,
                      currRenderObject.data.texture.scaleRect.y},
          (SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY},
          currRenderObject.data.texture.angle,
          (SDL_Point){24 * app->scalingFactorX, 7 * app->scalingFactorY});

      currRenderObject.data.texture.scaleRect.x = rotatedPoint1.x;
      currRenderObject.data.texture.scaleRect.y = rotatedPoint1.y;

      SDL_RenderCopyEx(app->renderer, currRenderObject.data.texture.texture,
                       NULL, &currRenderObject.data.texture.scaleRect,
                       currRenderObject.data.texture.angle + 180 -
                           currRenderObject.data.texture.angleAlt,
                       &(SDL_Point){0, 1}, SDL_FLIP_NONE);
    }
    // difficulty buttons
    else if (currRenderObject.buttonName < 2200) {
      if (SDL_PointInRect(&(SDL_Point){x, y},
                          &currRenderObject.data.texture.scaleRect)) {
        mouseShouldBeTriggered = SDL_TRUE;
        app->buttonPosTriggered = currRenderObject.buttonName;
      }
      if (currRenderObject.buttonName == app->p1Diff ||
          currRenderObject.buttonName == app->p2Diff) {
        SDL_RenderCopy(app->renderer,
                       currRenderObject.data.texture.triggeredTexture, NULL,
                       &currRenderObject.data.texture.scaleRect);
      } else {
        SDL_RenderCopy(app->renderer, currRenderObject.data.texture.texture,
                       NULL, &currRenderObject.data.texture.scaleRect);
      }
    }
    // weapon buttons
    else if (currRenderObject.buttonName < 2204) {
      if (SDL_PointInRect(&(SDL_Point){x, y},
                          &currRenderObject.data.texture.scaleRect)) {
        mouseShouldBeTriggered = SDL_TRUE;
        app->buttonPosTriggered = currRenderObject.buttonName;
      }
      if (app->settings.weaponsAllowed[currRenderObject.buttonName - 2200] ==
          SDL_TRUE) {
        SDL_RenderCopy(app->renderer,
                       currRenderObject.data.texture.triggeredTexture, NULL,
                       &currRenderObject.data.texture.scaleRect);
      } else {
        SDL_RenderCopy(app->renderer, currRenderObject.data.texture.texture,
                       NULL, &currRenderObject.data.texture.scaleRect);
      }
    }
  }

  if (mouseShouldBeTriggered) {
    app->isMouseTriggered = SDL_TRUE;
  } else {
    app->isMouseTriggered = SDL_FALSE;
    app->buttonPosTriggered = b_NONE;
  }
}

void renderBulletPath(App* app, RenderObject* bulletPath) {
  SDL_SetRenderTarget(app->renderer, bulletPath->data.texture.texture);
  SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 0);
  SDL_RenderClear(app->renderer);

  // calculating starting point for the path (aka gun basis)
  SDL_Point gunEdgeCoord;
  SDL_Color currColor = {0, 0, 0, 255};

  // for 2nd player
  if (app->currPlayer->tankGunObj->data.texture.flipFlag ==
      SDL_FLIP_HORIZONTAL) {
    gunEdgeCoord = getPixelScreenPosition(
        (SDL_Point){
            app->currPlayer->tankObj->data.texture.constRect.x *
                app->scalingFactorX,
            app->currPlayer->tankObj->data.texture.constRect.y *
                app->scalingFactorY,
        },
        (SDL_Point){8 * app->scalingFactorX, 27 * app->scalingFactorY},
        app->currPlayer->tankObj->data.texture.angle,
        (SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});

    bulletPath->data.texture.constRect.x =
        gunEdgeCoord.x / app->scalingFactorX -
        bulletPath->data.texture.constRect.w;
    bulletPath->data.texture.constRect.y =
        gunEdgeCoord.y / app->scalingFactorY -
        bulletPath->data.texture.constRect.h / 1.5f;

    currColor = (SDL_Color){255, 50, 0, 255};
  } else {
    // for 1st player
    gunEdgeCoord = getPixelScreenPosition(
        (SDL_Point){
            app->currPlayer->tankObj->data.texture.constRect.x *
                app->scalingFactorX,
            app->currPlayer->tankObj->data.texture.constRect.y *
                app->scalingFactorY,
        },
        (SDL_Point){5 * app->scalingFactorX, 27 * app->scalingFactorY},
        app->currPlayer->tankObj->data.texture.angle,
        (SDL_Point){24 * app->scalingFactorX, 8 * app->scalingFactorY});

    bulletPath->data.texture.constRect.x = gunEdgeCoord.x / app->scalingFactorX;
    bulletPath->data.texture.constRect.y =
        gunEdgeCoord.y / app->scalingFactorY -
        bulletPath->data.texture.constRect.h / 1.5f;
  }

  // that angle is clockwise
  double currGunAngle = app->currPlayer->tankGunObj->data.texture.angle;

  // calculating angle specifically for a current player
  // if currPlayer is a second player
  if (app->currPlayer->tankGunObj->data.texture.flipFlag ==
      SDL_FLIP_HORIZONTAL) {
    currGunAngle += 180 - app->currPlayer->tankGunObj->data.texture.angleAlt;
  } else {
    //  if currPlayer is a first player
    currGunAngle += app->currPlayer->tankGunObj->data.texture.angleAlt;
  }

  // normalizing just to be sure its in [0; 2pi) and now its counterclockwise
  currGunAngle = 360 - normalizeAngle(currGunAngle);

  double currGunCos = cos(DEGTORAD(currGunAngle));
  double currGunSin = sin(DEGTORAD(currGunAngle));

  double dt = 1. / 500;
  SDL_FPoint relativePos = {0.f, 0.f};

  // finding init vel for a spec. weapon
  double currVel = 0.0;
  switch (app->currWeapon) {
    // small bullet
    case 0:
      currVel = app->currPlayer->firingPower * 2;
      break;
    // BIG BULLET
    case 1:
      currVel = app->currPlayer->firingPower * 1.75;
      break;
    // small boom
    case 2:
      currVel = app->currPlayer->firingPower * 1.25;
      break;
    // BIG BOOM
    case 3:
      currVel = app->currPlayer->firingPower * 1;
      break;
    default:
      currVel = app->currPlayer->firingPower;
      break;
  }

  SDL_SetRenderDrawColor(app->renderer, currColor.r, currColor.g, currColor.b,
                         currColor.a);

  double windAngleRad = DEGTORAD(normalizeAngle(
      360 - app->globalConditions.wind.directionIcon->data.texture.angle));
  double windStrength = app->globalConditions.wind.windStrength;
  double windStrengthX = windStrength * cos(windAngleRad);
  double windStrengthY = windStrength * sin(windAngleRad);

  double vx = currVel * cos(DEGTORAD(currGunAngle));
  double vy = currVel * sin(DEGTORAD(currGunAngle));

  for (double currTime = 0.0; currTime <= 1.5; currTime += dt) {
    getPositionAtSpecTime(&relativePos, vx, vy, windStrengthX, windStrengthY,
                          currTime);
    if (app->currPlayer->tankGunObj->data.texture.flipFlag ==
        SDL_FLIP_HORIZONTAL) {
      int32_t currX = relativePos.x + 15 * currGunCos +
                      bulletPath->data.texture.constRect.w;
      int32_t currY = -relativePos.y +
                      bulletPath->data.texture.constRect.h / 1.5 -
                      18 * currGunSin;
      SDL_RenderDrawPoint(app->renderer, currX, currY);
    } else {
      int32_t currX = relativePos.x + 20 * currGunCos;
      int32_t currY = -relativePos.y +
                      bulletPath->data.texture.constRect.h / 1.5 -
                      18 * currGunSin;
      SDL_RenderDrawPoint(app->renderer, currX, currY);
    }
  }

  // enable rendering back
  SDL_SetRenderTarget(app->renderer, NULL);
}
// clearing sdl_textures* arr
void freeTexturesArr(SDL_Texture** arr, int32_t size) {
  for (int32_t i = 0; i != size; ++i) {
    SDL_DestroyTexture(arr[i]);
    log_info("deleting texture located in %p", arr[i]);
  }
  log_info("deleting arr located in %p", arr);

  free(arr);
}

// free render object
void freeRenderObject(RenderObject* object) {
  if (object) {
    SDL_DestroyTexture(object->data.texture.texture);
    SDL_DestroyTexture(object->data.texture.triggeredTexture);

    free(object);
  }
}