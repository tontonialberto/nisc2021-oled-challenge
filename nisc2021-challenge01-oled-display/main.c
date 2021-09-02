/*
    NeaPolis Innovation Summer Campus 2021 Examples
    Copyright (C) 2021 Domenico Rega [dodorega@gmail.com]
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 */

/*
 * [SSD1306] SSD1306 OLED Display Example
 * A simple example with the SSD1306 OLED display
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "ssd1306.h"
#include "stdio.h"

#define PLAYER_X_START              32
#define PLAYER_Y_START              7
#define PLAYER_X_STEP               3
#define PLAYER_RADIUS               5
#define BULLET_WIDTH                1
#define BULLET_HEIGHT               4
#define BULLET_Y_STEP               5
#define ENEMY_X_START               32
#define ENEMY_Y_START               40
#define ENEMY_STEP                  4
#define ENEMY_WIDTH                 20
#define ENEMY_HEIGHT                20
#define ENEMY_X_MIN                 15
#define ENEMY_X_MAX                 116
#define SCREEN_X_MIN                8
#define SCREEN_X_MAX                127
#define SCREEN_Y_MAX                63
#define SCREEN_MS_REFRESH_RATE      20
#define N_MAX_GAME_OBJECTS 7

#define STEP_TOWARDS_MAX(step) step > 0 ? step : -step
#define STEP_TOWARDS_MIN(step) step < 0 ? step : -step

typedef struct GameObject {
  int32_t xCurrent, yCurrent; // current x, y position
  int32_t xStep, yStep; // x and y "speed" ie. how much their value gets changed on each frame
  _Bool horizontal, vertical; // whether object can move horizontally/vertically

  // this callback can manipulate the corresponding GameObject if it goes out of screen boundaries
  // and return whether, after the end of the function, the object is still out of boundaries
  _Bool (*cbOutOfBoundaries)(struct GameObject *thisGO);

  // This function will be invoked when some caller wants to draw the GameObject
  void (*fnDraw)(struct GameObject *thisGO);
} GameObject;

_Bool cbPlayerOutOfBoundaries(GameObject *);
_Bool cbBulletOutOfBoundaries(GameObject *);
_Bool cbEnemyOutOfBoundaries(GameObject *);
void fnDrawPlayer(GameObject *);
void fnDrawBullet(GameObject *);
void fnDrawEnemy(GameObject *);

static GameObject player = {
                            .xCurrent = PLAYER_X_START,
                            .yCurrent = PLAYER_Y_START,
                            .xStep = PLAYER_X_STEP,
                            .yStep = -1, // NOT INITIALIZED
                            .horizontal = true,
                            .vertical = false,
                            .cbOutOfBoundaries = cbPlayerOutOfBoundaries,
                            .fnDraw = fnDrawPlayer
};

static GameObject bullet = {
                            .xCurrent = 0, // NOT INITIALIZED
                            .yCurrent = 0, // NOT INITIALIZED
                            .xStep = -1, // NOT INITIALIZED
                            .yStep = BULLET_Y_STEP,
                            .horizontal = false,
                            .vertical = true,
                            .cbOutOfBoundaries = cbBulletOutOfBoundaries,
                            .fnDraw = fnDrawBullet
};

static GameObject enemy = {
                           .xCurrent = ENEMY_X_START,
                           .yCurrent = ENEMY_Y_START,
                           .xStep = ENEMY_STEP,
                           .horizontal = true,
                           .vertical = false,
                           .cbOutOfBoundaries = cbEnemyOutOfBoundaries,
                           .fnDraw = fnDrawEnemy
};

// Game objects to be rendered on screen
static GameObject *gameObjects[N_MAX_GAME_OBJECTS] = { 0 };

static bool enemyHit = false; // Whether the enemy has been recently hit.
static uint8_t nEnemyLivesLeft = 3; // When it comes to zero, simulation ends.
static ssd1306_color_t enemyColor = SSD1306_COLOR_WHITE; // You can set this variable to change the color used to display enemy on screen.

static const I2CConfig i2ccfg = {
                                 OPMODE_I2C,
                                 400000,
                                 FAST_DUTY_CYCLE_2,
};

static const SSD1306Config ssd1306cfg = {
                                         &I2CD1,
                                         &i2ccfg,
                                         SSD1306_SAD_0X78,
};

static SSD1306Driver SSD1306D1;

void restoreOnHorizontalBounds(GameObject *obj, int xMin, int xMax, int width) {
  // If  x position goes under the negative boundary,
  // reposition x into the boundaries and the next step will move the object
  // towards the positive boundary.
  if(obj->xCurrent <= xMin) {
    obj->xCurrent = xMin + 1;
    obj->xStep = STEP_TOWARDS_MAX(obj->xStep);
  }
  // If it's the opposite case, ie. object goes over max position,
  // reposition x inside boundaries and the next step will move the object
  // towards the negative boundary.
  if((obj->xCurrent + width) > xMax) {
    obj->xCurrent = xMax - width - 1;
    obj->xStep = STEP_TOWARDS_MIN(obj->xStep);
  }
}

// Reposition the player inside the screen boundaries if it goes out of them.
// This function always returns false.
_Bool cbPlayerOutOfBoundaries(GameObject *player) {
  restoreOnHorizontalBounds(player, SCREEN_X_MIN, SCREEN_X_MAX, PLAYER_RADIUS * 2);

  return false;
}

// Returns true if the bullet is over the max screen y.
// This function does not reposition the bullet.
_Bool cbBulletOutOfBoundaries(struct GameObject *bullet) {

  // Check if a bullet has hit the enemy.
  _Bool overlapsWithEnemy =
      (bullet->xCurrent > (enemy.xCurrent + 2)) && (bullet->xCurrent < (enemy.xCurrent + ENEMY_WIDTH - 2))
      && (bullet->yCurrent > (enemy.yCurrent - ENEMY_HEIGHT)) && (bullet->yCurrent < enemy.yCurrent);

  if(overlapsWithEnemy) {
    enemyHit = true;
    nEnemyLivesLeft --;
    return true;
  }
  return SCREEN_Y_MAX < bullet->yCurrent - (BULLET_HEIGHT / 2);
}

_Bool cbEnemyOutOfBoundaries(struct GameObject *enemy) {

  restoreOnHorizontalBounds(enemy, ENEMY_X_MIN, ENEMY_X_MAX, ENEMY_WIDTH);

  return false;
}

void fnDrawPlayer(struct GameObject *player) {
  ssd1306DrawCircleFill(&SSD1306D1, player->xCurrent, player->yCurrent, PLAYER_RADIUS, SSD1306_COLOR_WHITE);
}

void fnDrawBullet(struct GameObject *bullet) {
  ssd1306DrawRectangleFill(&SSD1306D1, bullet->xCurrent, bullet->yCurrent, BULLET_WIDTH, BULLET_HEIGHT, SSD1306_COLOR_WHITE);
}

void fnDrawEnemy(struct GameObject *enemy) {
    ssd1306DrawRectangleFill(&SSD1306D1, enemy->xCurrent, enemy->yCurrent, ENEMY_WIDTH, ENEMY_HEIGHT, enemyColor);
}

// Append a game object to the list of game objects.
// If the list is full, don't do anything.
_Bool addToRenderable(GameObject *obj) {
  _Bool added = false;

  for(int i=0; i<N_MAX_GAME_OBJECTS; i++) {
    if(gameObjects[i] == NULL) {
      gameObjects[i] = obj;
      added = true;
      break;
    }
  }

  return added;
}

static THD_WORKING_AREA(waThreadBulletSpawner, 128);
static THD_FUNCTION(ThreadBulletSpawner, arg) {
  (void)arg;

  while(true) {

    // spawn bullet near to the player
    bullet.xCurrent = player.xCurrent + (PLAYER_RADIUS / 2) - (BULLET_WIDTH / 2);
    bullet.yCurrent = player.yCurrent + (PLAYER_RADIUS / 2) + (BULLET_HEIGHT / 2);
    addToRenderable(&bullet);

    chThdSleepMilliseconds(2000);
  }
}

static THD_WORKING_AREA(waThreadRenderer, 512);
static THD_FUNCTION(ThreadRenderer, arg) {
  (void)arg;

  static char buffer[50];

  chRegSetThreadName("thdOledDisplay");

  ssd1306ObjectInit(&SSD1306D1);

  ssd1306Start(&SSD1306D1, &ssd1306cfg);

  // clear screen
  ssd1306FillScreen(&SSD1306D1, SSD1306_COLOR_BLACK);
  ssd1306UpdateScreen(&SSD1306D1);

  while(true) {

    if(nEnemyLivesLeft <= 0) {
      thread_reference_t trp = NULL;
      chThdSuspendS(&trp);
    }

    ssd1306FillScreen(&SSD1306D1, SSD1306_COLOR_BLACK);

    // Draw enemy lives left on screen
    ssd1306GotoXy(&SSD1306D1, 0, 25);
    chsnprintf(buffer, sizeof(buffer), "Enemy Lives: %u ", nEnemyLivesLeft);
    ssd1306Puts(&SSD1306D1, buffer, &ssd1306_font_7x10, SSD1306_COLOR_WHITE);

    for(int i=0; i<N_MAX_GAME_OBJECTS; i++) {
      if(gameObjects[i] == NULL) {
        continue;
      }

      if(gameObjects[i]->cbOutOfBoundaries(gameObjects[i])) {
        gameObjects[i] = NULL;
      }
      else {
        if(gameObjects[i]->vertical) {
          gameObjects[i]->yCurrent += gameObjects[i]->yStep;
        }
        if(gameObjects[i]->horizontal) {
          gameObjects[i]->xCurrent += gameObjects[i]->xStep;
        }

        gameObjects[i]->fnDraw(gameObjects[i]);
      }
    }

    ssd1306UpdateScreen(&SSD1306D1);

    chThdSleepMilliseconds(SCREEN_MS_REFRESH_RATE);
  }
}

static THD_WORKING_AREA(waThreadEnemyBlinker, 128);
static THD_FUNCTION(ThreadEnemyBlinker, arg) {
  (void)arg;

  while(true) {
    if(!enemyHit) {
      continue;
    }

    for(int i=0; i<10; i++) {

      enemyColor = SSD1306_COLOR_BLACK;
      chThdSleepMilliseconds(100);

      enemyColor = SSD1306_COLOR_WHITE;
      chThdSleepMilliseconds(70);
    }

    enemyHit = false;
  }
}

//%--------------------------------------------------------


int main(void) {

  halInit();
  chSysInit();

  /* Configuring I2C related PINs */
  palSetLineMode(LINE_ARD_D15, PAL_MODE_ALTERNATE(4) |
                 PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST |
                 PAL_STM32_PUPDR_PULLUP);
  palSetLineMode(LINE_ARD_D14, PAL_MODE_ALTERNATE(4) |
                 PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST |
                 PAL_STM32_PUPDR_PULLUP);

  chThdCreateStatic(waThreadRenderer, sizeof(waThreadRenderer), NORMALPRIO, ThreadRenderer, NULL);
  chThdCreateStatic(waThreadBulletSpawner, sizeof(waThreadBulletSpawner), NORMALPRIO, ThreadBulletSpawner, NULL);
  chThdCreateStatic(waThreadEnemyBlinker, sizeof(waThreadEnemyBlinker), NORMALPRIO - 1, ThreadEnemyBlinker, NULL);

  addToRenderable(&player);
  addToRenderable(&enemy);

  /*Infinite loop*/
  while (true) {

    if(nEnemyLivesLeft <= 0) {
      ssd1306FillScreen(&SSD1306D1, SSD1306_COLOR_BLACK);
      ssd1306GotoXy(&SSD1306D1, 20, 30);
      ssd1306Puts(&SSD1306D1, "You Won!", &ssd1306_font_11x18, SSD1306_COLOR_WHITE);
      ssd1306UpdateScreen(&SSD1306D1);
    }

    chThdSleepMilliseconds(200);
  }
}


