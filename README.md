# Snake Game on STM32H743I-EVAL

A classic Snake game running on the **STM32H743I-EVAL** evaluation board, written in C with the STM32 HAL and BSP drivers. Steer with the joystick or the touchscreen, and pause with the TAMPER button.

<!-- Add a photo or GIF of the board running the game: -->
<!-- ![Snake on STM32H743I-EVAL](docs/snake.jpg) -->

## Features

- Runs on the board's LCD in landscape mode, using a 16x16 pixel cell grid with a score bar at the top
- Two control options: joystick or touchscreen
- Score and best score shown on screen
- The snake speeds up each time it eats (180 ms per step down to a 60 ms minimum)
- Pause and resume with the TAMPER button (interrupt driven)
- LED feedback: green LED (LED1) toggles on every food eaten, red LED (LED3) turns on at game over
- Flicker-free drawing: only the old tail, old head, new head and food are redrawn each step

## Hardware

| Item | Details |
|---|---|
| Board | STM32H743I-EVAL (STM32H743XIH6 MCU) |
| Display | On-board LCD, landscape |
| Input | Joystick (JOY1), touchscreen, TAMPER button |
| Debugger | On-board ST-LINK (USB) |

## Software requirements

- STM32CubeIDE
- STM32H7 HAL and BSP drivers (included in the project's `Drivers` folder)

## Getting started

1. Clone or download this repository.
2. Open STM32CubeIDE and choose **File -> Import -> General -> Existing Projects into Workspace**.
3. Select the `SnakeGame/STM32CubeIDE` folder and click **Finish**.
4. Build the project (hammer icon). You want 0 errors.
5. Connect the board to your PC with the ST-LINK USB cable.
6. Click **Debug** (bug icon), then **Resume (F8)**.

## How to play

| Action | Control |
|---|---|
| Start or restart | Joystick centre (SEL), or tap the screen |
| Steer | Joystick up/down/left/right |
| Steer by touch | Touch the screen: the snake turns toward your finger |
| Pause / resume | TAMPER button |

Eat the red food to grow and score. The game ends if you hit a wall or your own body.

## Project structure

```
SnakeGame/
├── Inc/
│   ├── game.h              # Game_Init(), Game_Run()
│   ├── main.h              # BSP includes, LED names
│   └── ...                 # HAL and BSP configuration headers
├── Src/
│   ├── main.c              # Clock, MPU, cache, LED, button and LCD setup; main loop
│   ├── game.c              # Game logic, drawing and input
│   ├── stm32h7xx_it.c      # SysTick and TAMPER (EXTI) interrupt handlers
│   └── system_stm32h7xx.c  # System init
└── STM32CubeIDE/
    ├── STM32H743XIHX_FLASH.ld        # Linker script
    ├── Example/Startup/              # Startup assembly file
    ├── Drivers/                      # HAL, BSP, CMSIS
    └── SnakeGame.launch              # Debug configuration
```

Only `game.c` and `game.h` are original game code. The rest comes from ST's BSP example, adapted for this project.

## How it works

**Startup (`main.c`):** configure the MPU and caches, start the HAL, set the CPU to 400 MHz, initialise the LEDs, the TAMPER button (EXTI) and the LCD, call `Game_Init()`, then loop on `Game_Run()` every 5 ms.

**Game loop (`game.c`):** a state machine with four states: menu, play, pause and game over. Inputs are read every loop, but the snake moves only when `stepMs` milliseconds have passed (`HAL_GetTick()`), so controls feel instant while speed stays controlled.

**One step:**
1. Compute the new head cell from the current direction.
2. Check wall and self collision.
3. Erase the old tail unless the snake ate.
4. Shift the body array and draw the new head.
5. On eating: grow, add a score point, speed up, place new food and toggle the green LED.

**TAMPER button:** the interrupt only sets a flag (`TamperPressed`). The game loop handles it, which keeps the interrupt short.

## Troubleshooting

| Problem | Fix |
|---|---|
| `undefined reference to Game_Init` | Make sure `game.c` is not excluded from the build (Properties -> C/C++ Build). |
| `multiple definition of SystemInit` | `system_stm32h7xx.c` is in the build twice. Remove one copy. |
| `No rule to make target ...` | A deleted file is still listed in the project. Remove it from the project tree. |
| Joystick does not respond | In `Game_Init()`, change `JOY_MODE_GPIO` to `JOY_MODE_EXTI`. |


## Author

Deniston
