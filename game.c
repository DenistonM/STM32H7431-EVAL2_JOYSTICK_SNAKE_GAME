#include "main.h"
#include "game.h"
#include <stdlib.h>

#define CELL      16
#define HUD_H     24
#define MAX_CELLS 1200

typedef enum { ST_MENU, ST_PLAY, ST_PAUSE, ST_OVER } State_t;
typedef enum { D_UP, D_DOWN, D_LEFT, D_RIGHT, D_NONE } Dir_t;

extern volatile uint8_t TamperPressed;

static uint32_t W, H;
static int gw, gh;                       /* grid size in cells */
static struct { int x, y; } snake[MAX_CELLS];
static int len, fx, fy, score, best;
static Dir_t dir, nextDir;
static State_t state;
static uint32_t lastStep, stepMs;

/* own touch handles */
static TS_Init_t  gTS;
static TS_State_t gTSState;
static uint8_t    touchWasDown = 0;

/* ---------- drawing helpers ---------- */
static void DrawCell(int x, int y, uint32_t color)
{
  UTIL_LCD_FillRect(x * CELL, HUD_H + y * CELL, CELL - 1, CELL - 1, color);
}

static void DrawHud(void)
{
  char buf[32];
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLUE);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
  sprintf(buf, "Score:%-3d  Best:%-3d ", score, best);
  UTIL_LCD_DisplayStringAt(8, 4, (uint8_t *)buf, LEFT_MODE);
}

static void DrawCentered(const char *l1, const char *l2)
{
  UTIL_LCD_SetFont(&Font24);
  UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLACK);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_YELLOW);
  UTIL_LCD_DisplayStringAt(0, H / 2 - 30, (uint8_t *)l1, CENTER_MODE);
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
  UTIL_LCD_DisplayStringAt(0, H / 2 + 5, (uint8_t *)l2, CENTER_MODE);
}

static void DrawPlayfield(void)
{
  int i;
  UTIL_LCD_Clear(UTIL_LCD_COLOR_BLACK);
  UTIL_LCD_FillRect(0, 0, W, HUD_H, UTIL_LCD_COLOR_BLUE);
  DrawHud();
  DrawCell(fx, fy, UTIL_LCD_COLOR_RED);
  for (i = 0; i < len; i++)
    DrawCell(snake[i].x, snake[i].y, (i == 0) ? UTIL_LCD_COLOR_YELLOW : UTIL_LCD_COLOR_GREEN);
}

/* ---------- game logic ---------- */
static void PlaceFood(void)
{
  int ok, i;
  do {
    fx = rand() % gw;
    fy = rand() % gh;
    ok = 1;
    for (i = 0; i < len; i++)
      if (snake[i].x == fx && snake[i].y == fy) { ok = 0; break; }
  } while (!ok);
}

static void NewGame(void)
{
  len = 3;
  snake[0].x = gw / 2;     snake[0].y = gh / 2;
  snake[1].x = gw / 2 - 1; snake[1].y = gh / 2;
  snake[2].x = gw / 2 - 2; snake[2].y = gh / 2;
  dir = nextDir = D_RIGHT;
  score = 0;
  stepMs = 180;
  PlaceFood();
  DrawPlayfield();
  lastStep = HAL_GetTick();
  state = ST_PLAY;
}

static void Step(void)
{
  int i, nx, ny, grow;

  dir = nextDir;
  nx = snake[0].x; ny = snake[0].y;
  if (dir == D_UP) ny--; else if (dir == D_DOWN) ny++;
  else if (dir == D_LEFT) nx--; else nx++;

  /* wall collision */
  if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) goto dead;
  /* self collision (the tail cell is free if we are not growing) */
  grow = (nx == fx && ny == fy);
  for (i = 0; i < len - (grow ? 0 : 1); i++)
    if (snake[i].x == nx && snake[i].y == ny) goto dead;

  if (!grow)
    DrawCell(snake[len - 1].x, snake[len - 1].y, UTIL_LCD_COLOR_BLACK);  /* erase tail */
  else if (len < MAX_CELLS)
    len++;

  for (i = len - 1; i > 0; i--) snake[i] = snake[i - 1];
  snake[0].x = nx; snake[0].y = ny;

  DrawCell(snake[1].x, snake[1].y, UTIL_LCD_COLOR_GREEN);   /* old head -> body */
  DrawCell(nx, ny, UTIL_LCD_COLOR_YELLOW);                  /* new head */

  if (grow)
  {
    score++;
    if (score > best) best = score;
    if (stepMs > 60) stepMs -= 5;       /* speed up */
    PlaceFood();
    DrawCell(fx, fy, UTIL_LCD_COLOR_RED);
    DrawHud();
    BSP_LED_Toggle(LED_GREEN);
  }
  return;

dead:
  BSP_LED_On(LED_RED);
  DrawCentered("GAME OVER", "Joystick SEL or tap to restart");
  state = ST_OVER;
}

/* ---------- inputs ---------- */
/* CHECK against Joystick_demo() in your example: BSP_JOY_GetState() and the
   JOY_xxx names can differ between BSP versions. */
static Dir_t ReadJoystick(uint8_t *sel)
{
  uint32_t j = BSP_JOY_GetState(JOY1);   /* one argument in your BSP (see joystick.c) */
  *sel = 0;
  switch (j)
  {
    case JOY_UP:    return D_UP;
    case JOY_DOWN:  return D_DOWN;
    case JOY_LEFT:  return D_LEFT;
    case JOY_RIGHT: return D_RIGHT;
    case JOY_SEL:   *sel = 1; return D_NONE;
    default:        return D_NONE;
  }
}

/* Returns 1 on a new touch (edge), 2 while finger held, 0 if none */
static uint8_t ReadTouch(uint32_t *tx, uint32_t *ty)
{
  BSP_TS_GetState(0, &gTSState);
  if (gTSState.TouchDetected)
  {
    *tx = gTSState.TouchX;
    *ty = gTSState.TouchY;
    if (!touchWasDown) { touchWasDown = 1; return 1; }
    return 2;
  }
  touchWasDown = 0;
  return 0;
}

static void Steer(Dir_t d)
{
  /* ignore a 180-degree reversal */
  if ((d == D_UP && dir == D_DOWN) || (d == D_DOWN && dir == D_UP) ||
      (d == D_LEFT && dir == D_RIGHT) || (d == D_RIGHT && dir == D_LEFT)) return;
  if (d != D_NONE) nextDir = d;
}

static void SteerTowardTouch(uint32_t tx, uint32_t ty)
{
  int hx = snake[0].x * CELL + CELL / 2;
  int hy = HUD_H + snake[0].y * CELL + CELL / 2;
  int dx = (int)tx - hx, dy = (int)ty - hy;
  if (abs(dx) > abs(dy)) Steer(dx > 0 ? D_RIGHT : D_LEFT);
  else                   Steer(dy > 0 ? D_DOWN  : D_UP);
}

/* ---------- public ---------- */
void Game_Init(void)
{
  BSP_LCD_GetXSize(0, &W);
  BSP_LCD_GetYSize(0, &H);
  gw = W / CELL;
  gh = (H - HUD_H) / CELL;

  /* Polling mode. If the joystick does not respond, try JOY_MODE_EXTI as in joystick.c */
  BSP_JOY_Init(JOY1, JOY_MODE_GPIO, JOY_ALL);

  gTS.Width       = W;
  gTS.Height      = H;
  gTS.Orientation = TS_SWAP_NONE;               /* same as touchscreen.c */
  gTS.Accuracy    = 5;
  BSP_TS_Init(0, &gTS);

  srand(HAL_GetTick());
  best = 0;
  state = ST_MENU;
  UTIL_LCD_Clear(UTIL_LCD_COLOR_BLACK);
  DrawCentered("SNAKE", "Joystick SEL or tap to start");
}

void Game_Run(void)
{
  uint8_t sel;
  uint32_t tx = 0, ty = 0;
  Dir_t jd = ReadJoystick(&sel);
  uint8_t t = ReadTouch(&tx, &ty);

  /* TAMPER = pause / resume */
  if (TamperPressed)
  {
    TamperPressed = 0;
    if (state == ST_PLAY)       { state = ST_PAUSE; DrawCentered("PAUSED", "Press TAMPER to resume"); }
    else if (state == ST_PAUSE) { DrawPlayfield(); lastStep = HAL_GetTick(); state = ST_PLAY; }
  }

  switch (state)
  {
    case ST_MENU:
    case ST_OVER:
      if (sel || t == 1) { BSP_LED_Off(LED_RED); NewGame(); }
      break;

    case ST_PLAY:
      Steer(jd);
      if (t) SteerTowardTouch(tx, ty);
      if (HAL_GetTick() - lastStep >= stepMs)
      {
        lastStep = HAL_GetTick();
        Step();
      }
      break;

    case ST_PAUSE:
    default:
      break;
  }
}
