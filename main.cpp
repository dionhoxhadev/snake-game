// Snake - a terminal recreation of the classic phone game.
// Cross-platform: Windows, macOS, Linux. Requires C++17.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

constexpr int kWidth = 24;   // playfield width in cells
constexpr int kHeight = 16;  // playfield height in cells
constexpr int kStartDelayMs = 150;
constexpr int kMinDelayMs = 60;

volatile std::sig_atomic_t g_interrupted = 0;
void onSignal(int) { g_interrupted = 1; }

enum class Key { None, Up, Down, Left, Right, Pause, Quit, Enter };

struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

// ---------------------------------------------------------------- terminal --

class Terminal {
public:
    Terminal() {
#ifdef _WIN32
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(out, &mode))
            SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        tcgetattr(STDIN_FILENO, &saved_);
        termios raw = saved_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
        std::cout << "\x1b[?1049h\x1b[?25l\x1b[2J" << std::flush;  // alt screen, hide cursor
    }

    ~Terminal() {
        std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
#endif
    }

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    // Non-blocking: returns Key::None if nothing was pressed.
    Key poll() {
#ifdef _WIN32
        if (!_kbhit()) return Key::None;
        int c = _getch();
        if (c == 0 || c == 224) {
            switch (_getch()) {
                case 72: return Key::Up;
                case 80: return Key::Down;
                case 75: return Key::Left;
                case 77: return Key::Right;
                default: return Key::None;
            }
        }
        return translate(c);
#else
        if (!readyToRead()) return Key::None;
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return Key::None;
        if (c == 27) {  // escape sequence (arrow keys)
            unsigned char seq[2];
            if (!readyToRead() || read(STDIN_FILENO, &seq[0], 1) != 1) return Key::Quit;
            if (!readyToRead() || read(STDIN_FILENO, &seq[1], 1) != 1) return Key::None;
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return Key::Up;
                    case 'B': return Key::Down;
                    case 'C': return Key::Right;
                    case 'D': return Key::Left;
                }
            }
            return Key::None;
        }
        return translate(c);
#endif
    }

private:
    static Key translate(int c) {
        switch (c) {
            case 'w': case 'W': return Key::Up;
            case 's': case 'S': return Key::Down;
            case 'a': case 'A': return Key::Left;
            case 'd': case 'D': return Key::Right;
            case 'p': case 'P': return Key::Pause;
            case 'q': case 'Q': return Key::Quit;
            case '\r': case '\n': case ' ': return Key::Enter;
            default: return Key::None;
        }
    }

#ifndef _WIN32
    static bool readyToRead() {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval tv{0, 0};
        return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
    }
    termios saved_{};
#endif
};

// -------------------------------------------------------------------- game --

class Game {
public:
    Game() : rng_(std::random_device{}()) { reset(); }

    void reset() {
        snake_.clear();
        snake_.push_back({kWidth / 2, kHeight / 2});
        snake_.push_back({kWidth / 2 - 1, kHeight / 2});
        snake_.push_back({kWidth / 2 - 2, kHeight / 2});
        dir_ = next_ = {1, 0};
        score_ = 0;
        eaten_ = 0;
        alive_ = true;
        won_ = false;
        placeFood();
    }

    void steer(Key k) {
        Point want = next_;
        if (k == Key::Up) want = {0, -1};
        else if (k == Key::Down) want = {0, 1};
        else if (k == Key::Left) want = {-1, 0};
        else if (k == Key::Right) want = {1, 0};
        // Can't reverse directly into yourself.
        if (want.x != -dir_.x || want.y != -dir_.y) next_ = want;
    }

    void step() {
        dir_ = next_;
        Point head{snake_.front().x + dir_.x, snake_.front().y + dir_.y};

        if (head.x < 0 || head.x >= kWidth || head.y < 0 || head.y >= kHeight) {
            alive_ = false;
            return;
        }

        bool grow = (head == food_);
        // The tail moves away this tick unless we're growing.
        auto end = grow ? snake_.end() : snake_.end() - 1;
        if (std::find(snake_.begin(), end, head) != end) {
            alive_ = false;
            return;
        }

        snake_.push_front(head);
        if (grow) {
            score_ += 10;
            ++eaten_;
            if (snake_.size() == static_cast<size_t>(kWidth * kHeight)) {
                won_ = true;
                alive_ = false;
                return;
            }
            placeFood();
        } else {
            snake_.pop_back();
        }
    }

    int delayMs() const { return std::max(kMinDelayMs, kStartDelayMs - eaten_ * 4); }
    bool alive() const { return alive_; }
    bool won() const { return won_; }
    int score() const { return score_; }

    std::string render(int highScore, bool paused) const {
        std::string out = "\x1b[H";  // cursor home
        out += "\x1b[1mSNAKE\x1b[0m   Score: " + std::to_string(score_) +
               "   Best: " + std::to_string(highScore) + "\x1b[K\n";

        const std::string border = "\x1b[36m+" + std::string(kWidth * 2, '-') + "+\x1b[0m\x1b[K\n";
        out += border;
        for (int y = 0; y < kHeight; ++y) {
            out += "\x1b[36m|\x1b[0m";
            for (int x = 0; x < kWidth; ++x) {
                Point p{x, y};
                if (p == snake_.front()) out += "\x1b[92m@@\x1b[0m";
                else if (std::find(snake_.begin(), snake_.end(), p) != snake_.end())
                    out += "\x1b[32m##\x1b[0m";
                else if (p == food_) out += "\x1b[91m()\x1b[0m";
                else out += "  ";
            }
            out += "\x1b[36m|\x1b[0m\x1b[K\n";
        }
        out += border;
        out += paused ? "\x1b[33mPAUSED - press P to resume\x1b[0m\x1b[K\n"
                      : "Arrows/WASD: move   P: pause   Q: quit\x1b[K\n";
        return out;
    }

private:
    void placeFood() {
        std::uniform_int_distribution<int> dx(0, kWidth - 1), dy(0, kHeight - 1);
        do {
            food_ = {dx(rng_), dy(rng_)};
        } while (std::find(snake_.begin(), snake_.end(), food_) != snake_.end());
    }

    std::deque<Point> snake_;
    Point dir_{1, 0}, next_{1, 0}, food_{0, 0};
    int score_ = 0, eaten_ = 0;
    bool alive_ = true, won_ = false;
    std::mt19937 rng_;
};

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

}  // namespace

// -------------------------------------------------------------------- main --

int main() {
    std::signal(SIGINT, onSignal);

    Terminal term;
    Game game;
    int highScore = 0;
    bool quit = false;

    while (!quit && !g_interrupted) {
        game.reset();
        bool paused = false;
        std::cout << game.render(highScore, paused) << std::flush;

        while (game.alive() && !quit && !g_interrupted) {
            // Wait for one tick while staying responsive to key presses.
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(game.delayMs());
            while (std::chrono::steady_clock::now() < deadline) {
                Key k = term.poll();
                if (k == Key::Quit) { quit = true; break; }
                if (k == Key::Pause) {
                    paused = !paused;
                    std::cout << game.render(highScore, paused) << std::flush;
                } else if (!paused) {
                    game.steer(k);
                }
                sleepMs(5);
            }
            if (quit || paused) { if (paused) sleepMs(20); continue; }

            game.step();
            highScore = std::max(highScore, game.score());
            std::cout << game.render(highScore, false) << std::flush;
        }
        if (quit || g_interrupted) break;

        std::cout << "\n\x1b[1;91m" << (game.won() ? "YOU WIN!" : "GAME OVER")
                  << "\x1b[0m  Final score: " << game.score()
                  << "\x1b[K\nPress Enter to play again, Q to quit.\x1b[K" << std::flush;

        while (!g_interrupted) {
            Key k = term.poll();
            if (k == Key::Quit) { quit = true; break; }
            if (k == Key::Enter) break;
            sleepMs(20);
        }
        std::cout << "\x1b[2J" << std::flush;
    }
    return 0;
}
