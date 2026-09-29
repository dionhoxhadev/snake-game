# Snake

A terminal recreation of the classic phone Snake game, written in C++17 with no external dependencies.

```
SNAKE   Score: 30   Best: 30
+------------------------------------------------+
|                                                |
|            ()                                  |
|                    ##@@                        |
|                    ##                          |
|                    ####                        |
+------------------------------------------------+
Arrows/WASD: move   P: pause   Q: quit
```

## Features

- Classic rules: eat food, grow longer, don't hit the walls or yourself
- Speeds up as you eat
- Pause, restart, and per-session high score
- Works on Windows, macOS, and Linux
- Single source file, about 300 lines

## Controls

| Key | Action |
| --- | --- |
| Arrow keys / W A S D | Move |
| P | Pause / resume |
| Enter or Space | Play again (after game over) |
| Q | Quit |

## Build and run

You need a C++17 compiler (g++, clang++, or MSVC).

**macOS / Linux**

```bash
make
./snake
```

**Windows (MinGW / MSYS2)**

```bash
g++ -std=c++17 -O2 -o snake.exe main.cpp
snake.exe
```

**Windows (Visual Studio Developer Prompt)**

```bat
cl /std:c++17 /O2 /EHsc main.cpp /Fe:snake.exe
snake.exe
```

Use a terminal that supports ANSI colors (Windows Terminal, PowerShell 5+, or any modern macOS/Linux terminal).

## How it works

- **Game logic:** the snake is a `std::deque<Point>`. Each tick pushes a new head and pops the tail, unless food was eaten.
- **Input:** non-blocking keyboard polling (`select` + `termios` on POSIX, `_kbhit`/`_getch` on Windows), so the game keeps running between key presses.
- **Rendering:** each frame is built as one string with ANSI escape codes and printed in a single write to avoid flicker. Cells are two characters wide so the grid looks square.
- **Difficulty:** the tick delay drops from 150 ms toward 60 ms as you eat.

## Ideas for extending

- Save the high score to a file
- Wrap-around walls mode
- Obstacles and levels
- Bonus food that disappears after a few seconds

## License

MIT, see [LICENSE](LICENSE).
