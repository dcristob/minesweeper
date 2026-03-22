#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// --- Constants ---
#define CELL_SIZE 32
#define CELL_GAP 2
#define CELL_STRIDE (CELL_SIZE + CELL_GAP)
#define PADDING 20
#define HEADER_HEIGHT 48
#define MAX_COLS 30
#define MAX_ROWS 16
#define MAX_CELLS (MAX_COLS * MAX_ROWS)
#define MAX_LEADERBOARD 10
#define NAME_LEN 3

// --- Colors (GNOME Mines Light Theme) ---
#define COL_WINDOW_BG    CLITERAL(Color){ 0xf6, 0xf5, 0xf4, 0xff }
#define COL_BOARD_BG     CLITERAL(Color){ 0xe8, 0xe4, 0xe0, 0xff }
#define COL_CELL_HIDDEN  CLITERAL(Color){ 0xf5, 0xf3, 0xf0, 0xff }
#define COL_CELL_REVEALED CLITERAL(Color){ 0xd6, 0xd1, 0xcc, 0xff }
#define COL_CELL_EMPTY   CLITERAL(Color){ 0xc0, 0xbb, 0xb5, 0xff }
#define COL_HEADER_BG    CLITERAL(Color){ 0xd6, 0xd1, 0xcc, 0xff }
#define COL_MINE_HIT     CLITERAL(Color){ 0xe0, 0x40, 0x40, 0xff }
#define COL_TEXT         CLITERAL(Color){ 0x3d, 0x38, 0x46, 0xff }
#define COL_TEXT_DIM     CLITERAL(Color){ 0x5e, 0x5c, 0x5a, 0xff }
#define COL_OVERLAY      CLITERAL(Color){ 0x00, 0x00, 0x00, 0x99 }

static const Color NUMBER_COLORS[9] = {
    {0},
    { 0x35, 0x84, 0xe4, 0xff },
    { 0x2e, 0xc2, 0x7e, 0xff },
    { 0xe5, 0xa5, 0x0a, 0xff },
    { 0xc6, 0x46, 0x00, 0xff },
    { 0xa3, 0x47, 0xba, 0xff },
    { 0x26, 0xa2, 0x69, 0xff },
    { 0x3d, 0x38, 0x46, 0xff },
    { 0x9a, 0x99, 0x96, 0xff },
};

typedef enum { CELL_HIDDEN, CELL_REVEALED, CELL_FLAGGED } CellState;
typedef enum { SCREEN_MENU, SCREEN_PLAYING, SCREEN_PAUSED, SCREEN_GAME_OVER,
               SCREEN_GAME_WON, SCREEN_NAME_ENTRY, SCREEN_LEADERBOARD } Screen;
typedef enum { DIFF_BEGINNER, DIFF_INTERMEDIATE, DIFF_EXPERT, DIFF_COUNT } Difficulty;

typedef struct { int rows; int cols; int mines; const char *name; } DiffConfig;
static const DiffConfig DIFFS[DIFF_COUNT] = {
    { 9,  9,  10, "BEGINNER" },
    { 16, 16, 40, "INTERMEDIATE" },
    { 16, 30, 99, "EXPERT" },
};

typedef struct {
    bool is_mine;
    int adjacent;
    CellState state;
} Cell;

typedef struct {
    char name[NAME_LEN + 1];
    int time_secs;
} LeaderEntry;

typedef struct {
    Screen screen;
    Difficulty difficulty;
    Cell cells[MAX_CELLS];
    int rows, cols, mines;
    int flags_placed;
    bool first_click;
    float elapsed;
    bool timer_running;
    int cursor_row, cursor_col;
    bool cursor_visible;
    bool prev_both_down;
    int triggered_mine;
    char entry_name[NAME_LEN + 1];
    int entry_len;
    LeaderEntry leaderboard[DIFF_COUNT][MAX_LEADERBOARD];
    int lb_count[DIFF_COUNT];
    Difficulty lb_view_diff;
    int win_w, win_h;
    int board_x, board_y;
} Game;

static Game game = {0};

static void calc_window_size(void) {
    const DiffConfig *d = &DIFFS[game.difficulty];
    game.rows = d->rows;
    game.cols = d->cols;
    game.mines = d->mines;
    game.win_w = d->cols * CELL_STRIDE + 2 * PADDING;
    game.win_h = d->rows * CELL_STRIDE + HEADER_HEIGHT + 2 * PADDING;
    game.board_x = PADDING;
    game.board_y = PADDING + HEADER_HEIGHT;
}

static void board_init(void) {
    const DiffConfig *d = &DIFFS[game.difficulty];
    game.rows = d->rows;
    game.cols = d->cols;
    game.mines = d->mines;
    game.flags_placed = 0;
    game.first_click = true;
    game.elapsed = 0.0f;
    game.timer_running = false;
    game.cursor_row = 0;
    game.cursor_col = 0;
    game.cursor_visible = false;
    game.prev_both_down = false;
    game.triggered_mine = -1;
    game.entry_len = 0;
    memset(game.entry_name, 0, sizeof(game.entry_name));
    for (int i = 0; i < game.rows * game.cols; i++) {
        game.cells[i] = (Cell){0};
    }
}

static void place_mines(int safe_row, int safe_col) {
    int total = game.rows * game.cols;
    int placed = 0;
    srand((unsigned)time(NULL));
    while (placed < game.mines) {
        int idx = rand() % total;
        int r = idx / game.cols;
        int c = idx % game.cols;
        if (game.cells[idx].is_mine) continue;
        if (abs(r - safe_row) <= 1 && abs(c - safe_col) <= 1) continue;
        game.cells[idx].is_mine = true;
        placed++;
    }
    for (int r = 0; r < game.rows; r++) {
        for (int c = 0; c < game.cols; c++) {
            if (game.cells[r * game.cols + c].is_mine) continue;
            int count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < game.rows && nc >= 0 && nc < game.cols) {
                        if (game.cells[nr * game.cols + nc].is_mine) count++;
                    }
                }
            }
            game.cells[r * game.cols + c].adjacent = count;
        }
    }
    game.first_click = false;
}

static void reveal_cell(int row, int col) {
    int queue[MAX_CELLS];
    int head = 0, tail = 0;
    int idx = row * game.cols + col;
    if (game.cells[idx].state != CELL_HIDDEN) return;
    game.cells[idx].state = CELL_REVEALED;
    if (game.cells[idx].is_mine) {
        game.triggered_mine = idx;
        return;
    }
    if (game.cells[idx].adjacent == 0) {
        queue[tail++] = idx;
    }
    while (head < tail) {
        int cur = queue[head++];
        int cr = cur / game.cols, cc = cur % game.cols;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int nr = cr + dr, nc = cc + dc;
                if (nr < 0 || nr >= game.rows || nc < 0 || nc >= game.cols) continue;
                int ni = nr * game.cols + nc;
                if (game.cells[ni].state != CELL_HIDDEN || game.cells[ni].is_mine) continue;
                game.cells[ni].state = CELL_REVEALED;
                if (game.cells[ni].adjacent == 0) {
                    queue[tail++] = ni;
                }
            }
        }
    }
}

static bool check_win(void) {
    for (int i = 0; i < game.rows * game.cols; i++) {
        if (!game.cells[i].is_mine && game.cells[i].state != CELL_REVEALED) return false;
    }
    return true;
}

static void reveal_all_mines(void) {
    for (int i = 0; i < game.rows * game.cols; i++) {
        if (game.cells[i].is_mine) game.cells[i].state = CELL_REVEALED;
    }
}

static const char *get_leaderboard_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    snprintf(path, sizeof(path), "%s/.local/share/minesweeper/leaderboard.txt", home);
    return path;
}

static const char *diff_str(Difficulty d) {
    switch (d) {
        case DIFF_BEGINNER: return "beginner";
        case DIFF_INTERMEDIATE: return "intermediate";
        case DIFF_EXPERT: return "expert";
        default: return "unknown";
    }
}

static Difficulty parse_diff(const char *s) {
    if (strcmp(s, "beginner") == 0) return DIFF_BEGINNER;
    if (strcmp(s, "intermediate") == 0) return DIFF_INTERMEDIATE;
    if (strcmp(s, "expert") == 0) return DIFF_EXPERT;
    return DIFF_COUNT;
}

static void load_leaderboard(void) {
    for (int i = 0; i < DIFF_COUNT; i++) game.lb_count[i] = 0;
    FILE *f = fopen(get_leaderboard_path(), "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char diff_s[32], name[8];
        int secs;
        if (sscanf(line, "%31[^,],%7[^,],%d", diff_s, name, &secs) != 3) continue;
        Difficulty d = parse_diff(diff_s);
        if (d >= DIFF_COUNT) continue;
        if (game.lb_count[d] >= MAX_LEADERBOARD) continue;
        LeaderEntry *e = &game.leaderboard[d][game.lb_count[d]++];
        memcpy(e->name, name, NAME_LEN);
        e->name[NAME_LEN] = '\0';
        e->time_secs = secs;
    }
    fclose(f);
}

static void save_leaderboard(void) {
    const char *path = get_leaderboard_path();
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.local/share/minesweeper", getenv("HOME"));
    mkdir(dir, 0755);

    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int d = 0; d < DIFF_COUNT; d++) {
        for (int i = 0; i < game.lb_count[d]; i++) {
            fprintf(f, "%s,%s,%d\n", diff_str(d),
                    game.leaderboard[d][i].name,
                    game.leaderboard[d][i].time_secs);
        }
    }
    fclose(f);
}

static bool is_top_score(Difficulty d, int secs) {
    if (game.lb_count[d] < MAX_LEADERBOARD) return true;
    return secs < game.leaderboard[d][game.lb_count[d] - 1].time_secs;
}

static void insert_score(Difficulty d, const char *name, int secs) {
    int pos = game.lb_count[d];
    for (int i = 0; i < game.lb_count[d]; i++) {
        if (secs < game.leaderboard[d][i].time_secs) { pos = i; break; }
    }
    if (game.lb_count[d] < MAX_LEADERBOARD) game.lb_count[d]++;
    for (int i = game.lb_count[d] - 1; i > pos; i--) {
        game.leaderboard[d][i] = game.leaderboard[d][i - 1];
    }
    memcpy(game.leaderboard[d][pos].name, name, NAME_LEN);
    game.leaderboard[d][pos].name[NAME_LEN] = '\0';
    game.leaderboard[d][pos].time_secs = secs;
    save_leaderboard();
}

static void draw_cell(int row, int col) {
    int x = game.board_x + col * CELL_STRIDE;
    int y = game.board_y + row * CELL_STRIDE;
    Cell *cell = &game.cells[row * game.cols + col];
    Rectangle rect = { x, y, CELL_SIZE, CELL_SIZE };
    float roundness = 0.15f;

    switch (cell->state) {
    case CELL_HIDDEN:
        DrawRectangleRounded(rect, roundness, 4, COL_CELL_HIDDEN);
        break;
    case CELL_FLAGGED:
        DrawRectangleRounded(rect, roundness, 4, COL_CELL_HIDDEN);
        DrawLineEx((Vector2){x + 12, y + 6}, (Vector2){x + 12, y + 24}, 2, COL_TEXT);
        DrawTriangle(
            (Vector2){x + 13, y + 7},
            (Vector2){x + 13, y + 17},
            (Vector2){x + 24, y + 12},
            COL_MINE_HIT
        );
        break;
    case CELL_REVEALED:
        if (cell->is_mine) {
            int idx = row * game.cols + col;
            Color bg = (idx == game.triggered_mine) ? COL_MINE_HIT : COL_CELL_REVEALED;
            DrawRectangleRounded(rect, roundness, 4, bg);
            int cx = x + CELL_SIZE / 2, cy = y + CELL_SIZE / 2;
            DrawCircle(cx, cy, 8, COL_TEXT);
            DrawLineEx((Vector2){cx - 10, cy}, (Vector2){cx + 10, cy}, 2, COL_TEXT);
            DrawLineEx((Vector2){cx, cy - 10}, (Vector2){cx, cy + 10}, 2, COL_TEXT);
            DrawLineEx((Vector2){cx - 7, cy - 7}, (Vector2){cx + 7, cy + 7}, 2, COL_TEXT);
            DrawLineEx((Vector2){cx - 7, cy + 7}, (Vector2){cx + 7, cy - 7}, 2, COL_TEXT);
        } else if (cell->adjacent == 0) {
            DrawRectangleRounded(rect, roundness, 4, COL_CELL_EMPTY);
        } else {
            DrawRectangleRounded(rect, roundness, 4, COL_CELL_REVEALED);
            const char *num = TextFormat("%d", cell->adjacent);
            int fw = MeasureText(num, 20);
            DrawText(num, x + (CELL_SIZE - fw) / 2, y + 7, 20, NUMBER_COLORS[cell->adjacent]);
        }
        break;
    }
}

static void draw_board(void) {
    DrawRectangleRounded(
        (Rectangle){ game.board_x - 4, game.board_y - 4,
                     game.cols * CELL_STRIDE + 6, game.rows * CELL_STRIDE + 6 },
        0.02f, 4, COL_BOARD_BG
    );
    for (int r = 0; r < game.rows; r++) {
        for (int c = 0; c < game.cols; c++) {
            draw_cell(r, c);
        }
    }
    if (game.cursor_visible) {
        int x = game.board_x + game.cursor_col * CELL_STRIDE;
        int y = game.board_y + game.cursor_row * CELL_STRIDE;
        DrawRectangleRoundedLinesEx(
            (Rectangle){ x - 1, y - 1, CELL_SIZE + 2, CELL_SIZE + 2 },
            0.15f, 4, 2.0f, COL_TEXT
        );
    }
}

static void draw_header(void) {
    Rectangle hdr = { PADDING, PADDING, game.cols * CELL_STRIDE - 2, HEADER_HEIGHT - 8 };
    DrawRectangleRounded(hdr, 0.15f, 4, COL_HEADER_BG);

    int flags_left = game.mines - game.flags_placed;
    const char *flag_text = TextFormat("F: %d", flags_left);
    DrawText(flag_text, PADDING + 12, PADDING + 10, 20, COL_TEXT);

    const char *diff_name = DIFFS[game.difficulty].name;
    int dw = MeasureText(diff_name, 16);
    DrawText(diff_name, PADDING + (game.cols * CELL_STRIDE - 2 - dw) / 2, PADDING + 12, 16, COL_TEXT_DIM);

    int secs = (int)game.elapsed;
    int mins = secs / 60;
    secs %= 60;
    const char *time_text = TextFormat("%02d:%02d", mins, secs);
    int tw = MeasureText(time_text, 20);
    DrawText(time_text, PADDING + game.cols * CELL_STRIDE - 2 - tw - 12, PADDING + 10, 20, COL_TEXT);
}

static bool draw_button(const char *text, int x, int y, int w, int h) {
    Rectangle rect = { x, y, w, h };
    bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    Color bg = hover ? COL_CELL_HIDDEN : COL_HEADER_BG;
    DrawRectangleRounded(rect, 0.3f, 4, bg);
    int tw = MeasureText(text, 20);
    DrawText(text, x + (w - tw) / 2, y + (h - 20) / 2, 20, COL_TEXT);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void start_new_game(void) {
    calc_window_size();
    board_init();
    game.screen = SCREEN_PLAYING;
}

static void draw_game_over_overlay(void) {
    DrawRectangle(0, 0, game.win_w, game.win_h, COL_OVERLAY);
    const char *title = "Game Over";
    int tw = MeasureText(title, 40);
    DrawText(title, (game.win_w - tw) / 2, game.win_h / 2 - 60, 40, WHITE);

    int bw = 140, bh = 40, gap = 20;
    int bx = (game.win_w - bw * 2 - gap) / 2;
    int by = game.win_h / 2 + 10;

    if (draw_button("New Game", bx, by, bw, bh) || IsKeyPressed(KEY_ENTER)) {
        start_new_game();
    }
    if (draw_button("Menu", bx + bw + gap, by, bw, bh) || IsKeyPressed(KEY_ESCAPE)) {
        game.screen = SCREEN_MENU;
    }
}

static void draw_game_won_overlay(void) {
    DrawRectangle(0, 0, game.win_w, game.win_h, COL_OVERLAY);
    const char *title = "You Win!";
    int tw = MeasureText(title, 40);
    DrawText(title, (game.win_w - tw) / 2, game.win_h / 2 - 80, 40, WHITE);

    int secs = (int)game.elapsed;
    int mins = secs / 60;
    secs %= 60;
    const char *time_text = TextFormat("Time: %02d:%02d", mins, secs);
    int ttw = MeasureText(time_text, 24);
    DrawText(time_text, (game.win_w - ttw) / 2, game.win_h / 2 - 30, 24, WHITE);

    int bw = 140, bh = 40, gap = 20;
    int bx = (game.win_w - bw * 2 - gap) / 2;
    int by = game.win_h / 2 + 20;

    if (draw_button("New Game", bx, by, bw, bh) || IsKeyPressed(KEY_ENTER)) {
        start_new_game();
    }
    if (draw_button("Menu", bx + bw + gap, by, bw, bh) || IsKeyPressed(KEY_ESCAPE)) {
        game.screen = SCREEN_MENU;
    }
}

static void draw_paused_overlay(void) {
    DrawRectangle(0, 0, game.win_w, game.win_h, COL_OVERLAY);
    const char *title = "Paused";
    int tw = MeasureText(title, 40);
    DrawText(title, (game.win_w - tw) / 2, game.win_h / 2 - 20, 40, WHITE);
}

static void draw_name_entry(void) {
    draw_board();
    draw_header();

    DrawRectangle(0, 0, game.win_w, game.win_h, COL_OVERLAY);

    const char *title = "New High Score!";
    int tw = MeasureText(title, 32);
    DrawText(title, (game.win_w - tw) / 2, game.win_h / 2 - 80, 32, WHITE);

    int secs = (int)game.elapsed;
    int mins = secs / 60;
    secs %= 60;
    const char *time_text = TextFormat("Time: %02d:%02d", mins, secs);
    int ttw = MeasureText(time_text, 24);
    DrawText(time_text, (game.win_w - ttw) / 2, game.win_h / 2 - 40, 24, WHITE);

    const char *prompt = "Enter your name:";
    int pw = MeasureText(prompt, 20);
    DrawText(prompt, (game.win_w - pw) / 2, game.win_h / 2, 20, WHITE);

    int box_size = 36, box_gap = 8;
    int total_w = NAME_LEN * box_size + (NAME_LEN - 1) * box_gap;
    int sx = (game.win_w - total_w) / 2;
    int sy = game.win_h / 2 + 32;
    for (int i = 0; i < NAME_LEN; i++) {
        int bx = sx + i * (box_size + box_gap);
        DrawRectangleRounded((Rectangle){bx, sy, box_size, box_size}, 0.2f, 4, COL_CELL_HIDDEN);
        if (i < game.entry_len) {
            char ch[2] = { game.entry_name[i], '\0' };
            int cw = MeasureText(ch, 24);
            DrawText(ch, bx + (box_size - cw) / 2, sy + 7, 24, COL_TEXT);
        } else if (i == game.entry_len) {
            if (((int)(GetTime() * 2)) % 2 == 0) {
                DrawLineEx((Vector2){bx + box_size / 2 - 6, sy + box_size - 6},
                           (Vector2){bx + box_size / 2 + 6, sy + box_size - 6}, 2, COL_TEXT);
            }
        }
    }

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 'a' && key <= 'z') key -= 32;
        if (key >= 'A' && key <= 'Z' && game.entry_len < NAME_LEN) {
            game.entry_name[game.entry_len++] = (char)key;
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && game.entry_len > 0) {
        game.entry_len--;
        game.entry_name[game.entry_len] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER) && game.entry_len == NAME_LEN) {
        insert_score(game.difficulty, game.entry_name, (int)game.elapsed);
        game.screen = SCREEN_GAME_WON;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        game.screen = SCREEN_GAME_WON;
    }
}

static void draw_menu(void) {
    const char *title = "Minesweeper";
    int tw = MeasureText(title, 40);
    DrawText(title, (game.win_w - tw) / 2, 60, 40, COL_TEXT);

    int bw = 200, bh = 44, gap = 12;
    int bx = (game.win_w - bw) / 2;
    int by = 140;

    if (draw_button("Beginner (9x9)", bx, by, bw, bh)) {
        game.difficulty = DIFF_BEGINNER;
        calc_window_size();
        SetWindowSize(game.win_w, game.win_h);
        start_new_game();
    }
    by += bh + gap;
    if (draw_button("Intermediate (16x16)", bx, by, bw, bh)) {
        game.difficulty = DIFF_INTERMEDIATE;
        calc_window_size();
        SetWindowSize(game.win_w, game.win_h);
        start_new_game();
    }
    by += bh + gap;
    if (draw_button("Expert (30x16)", bx, by, bw, bh)) {
        game.difficulty = DIFF_EXPERT;
        calc_window_size();
        SetWindowSize(game.win_w, game.win_h);
        start_new_game();
    }
    by += bh + gap + 12;
    if (draw_button("Leaderboard", bx, by, bw, bh)) {
        game.lb_view_diff = game.difficulty;
        game.screen = SCREEN_LEADERBOARD;
    }
    by += bh + gap;
    if (draw_button("Quit", bx, by, bw, bh) || IsKeyPressed(KEY_ESCAPE)) {
        CloseWindow();
        exit(0);
    }
}

static void draw_leaderboard_screen(void) {
    const char *title = "Leaderboard";
    int tw = MeasureText(title, 32);
    DrawText(title, (game.win_w - tw) / 2, 20, 32, COL_TEXT);

    int tab_w = 130, tab_h = 32, tab_gap = 8;
    int tabs_total = DIFF_COUNT * tab_w + (DIFF_COUNT - 1) * tab_gap;
    int tx = (game.win_w - tabs_total) / 2;
    int ty = 64;
    for (int d = 0; d < DIFF_COUNT; d++) {
        int bx = tx + d * (tab_w + tab_gap);
        Rectangle rect = { bx, ty, tab_w, tab_h };
        bool selected = ((Difficulty)d == game.lb_view_diff);
        bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
        Color bg = selected ? COL_TEXT : (hover ? COL_CELL_HIDDEN : COL_HEADER_BG);
        Color fg = selected ? WHITE : COL_TEXT;
        DrawRectangleRounded(rect, 0.3f, 4, bg);
        int dw = MeasureText(DIFFS[d].name, 14);
        DrawText(DIFFS[d].name, bx + (tab_w - dw) / 2, ty + 9, 14, fg);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            game.lb_view_diff = d;
        }
    }

    int table_x = (game.win_w - 300) / 2;
    int table_y = ty + tab_h + 20;
    DrawText("Rank", table_x, table_y, 16, COL_TEXT_DIM);
    DrawText("Name", table_x + 70, table_y, 16, COL_TEXT_DIM);
    DrawText("Time", table_x + 200, table_y, 16, COL_TEXT_DIM);
    table_y += 28;

    Difficulty d = game.lb_view_diff;
    for (int i = 0; i < game.lb_count[d]; i++) {
        int secs = game.leaderboard[d][i].time_secs;
        int mins = secs / 60;
        secs %= 60;
        DrawText(TextFormat("%d.", i + 1), table_x, table_y, 20, COL_TEXT);
        DrawText(game.leaderboard[d][i].name, table_x + 70, table_y, 20, COL_TEXT);
        DrawText(TextFormat("%02d:%02d", mins, secs), table_x + 200, table_y, 20, COL_TEXT);
        table_y += 28;
    }
    if (game.lb_count[d] == 0) {
        const char *empty = "No records yet";
        int ew = MeasureText(empty, 18);
        DrawText(empty, (game.win_w - ew) / 2, table_y + 20, 18, COL_TEXT_DIM);
    }

    int bw = 120, bh = 40;
    int bx = (game.win_w - bw) / 2;
    int by = game.win_h - 70;
    if (draw_button("Back", bx, by, bw, bh) || IsKeyPressed(KEY_ESCAPE)) {
        game.screen = SCREEN_MENU;
    }

    if (IsKeyPressed(KEY_LEFT) && game.lb_view_diff > 0) game.lb_view_diff--;
    if (IsKeyPressed(KEY_RIGHT) && game.lb_view_diff < DIFF_COUNT - 1) game.lb_view_diff++;
}

static void chord_cell(int row, int col) {
    Cell *cell = &game.cells[row * game.cols + col];
    if (cell->state != CELL_REVEALED || cell->adjacent == 0) return;
    int flag_count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int nr = row + dr, nc = col + dc;
            if (nr < 0 || nr >= game.rows || nc < 0 || nc >= game.cols) continue;
            if (game.cells[nr * game.cols + nc].state == CELL_FLAGGED) flag_count++;
        }
    }
    if (flag_count != cell->adjacent) return;
    bool hit_mine = false;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int nr = row + dr, nc = col + dc;
            if (nr < 0 || nr >= game.rows || nc < 0 || nc >= game.cols) continue;
            int ni = nr * game.cols + nc;
            if (game.cells[ni].state == CELL_HIDDEN) {
                reveal_cell(nr, nc);
                if (game.cells[ni].is_mine) hit_mine = true;
            }
        }
    }
    if (hit_mine) {
        reveal_all_mines();
        game.timer_running = false;
        game.screen = SCREEN_GAME_OVER;
    } else if (check_win()) {
        game.timer_running = false;
        if (is_top_score(game.difficulty, (int)game.elapsed)) {
            game.entry_len = 0;
            memset(game.entry_name, 0, sizeof(game.entry_name));
            game.screen = SCREEN_NAME_ENTRY;
        } else {
            game.screen = SCREEN_GAME_WON;
        }
    }
}

static bool mouse_to_cell(int *row, int *col) {
    int mx = GetMouseX() - game.board_x;
    int my = GetMouseY() - game.board_y;
    if (mx < 0 || my < 0) return false;
    int c = mx / CELL_STRIDE;
    int r = my / CELL_STRIDE;
    if (c >= game.cols || r >= game.rows) return false;
    if (mx % CELL_STRIDE >= CELL_SIZE || my % CELL_STRIDE >= CELL_SIZE) return false;
    *row = r;
    *col = c;
    return true;
}

static void handle_playing_input(void) {
    int row, col;

    // Hide cursor on mouse movement
    if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0) {
        game.cursor_visible = false;
    }

    // Left click - reveal
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        game.cursor_visible = false;
        if (mouse_to_cell(&row, &col)) {
            Cell *cell = &game.cells[row * game.cols + col];
            if (cell->state == CELL_HIDDEN) {
                if (game.first_click) {
                    place_mines(row, col);
                    game.timer_running = true;
                }
                reveal_cell(row, col);
                if (cell->is_mine) {
                    reveal_all_mines();
                    game.timer_running = false;
                    game.screen = SCREEN_GAME_OVER;
                } else if (check_win()) {
                    game.timer_running = false;
                    if (is_top_score(game.difficulty, (int)game.elapsed)) {
                        game.entry_len = 0;
                        memset(game.entry_name, 0, sizeof(game.entry_name));
                        game.screen = SCREEN_NAME_ENTRY;
                    } else {
                        game.screen = SCREEN_GAME_WON;
                    }
                }
            }
        }
    }

    // Right click - flag
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        game.cursor_visible = false;
        if (mouse_to_cell(&row, &col)) {
            Cell *cell = &game.cells[row * game.cols + col];
            if (cell->state == CELL_HIDDEN) {
                cell->state = CELL_FLAGGED;
                game.flags_placed++;
            } else if (cell->state == CELL_FLAGGED) {
                cell->state = CELL_HIDDEN;
                game.flags_placed--;
            }
        }
    }

    // Middle click chord
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        game.cursor_visible = false;
        if (mouse_to_cell(&row, &col)) chord_cell(row, col);
    }

    // Left+right chord (edge-triggered)
    bool both_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (both_down && !game.prev_both_down) {
        game.cursor_visible = false;
        if (mouse_to_cell(&row, &col)) chord_cell(row, col);
    }
    game.prev_both_down = both_down;

    // Arrow keys
    if (IsKeyPressed(KEY_UP))    { game.cursor_visible = true; if (game.cursor_row > 0) game.cursor_row--; }
    if (IsKeyPressed(KEY_DOWN))  { game.cursor_visible = true; if (game.cursor_row < game.rows - 1) game.cursor_row++; }
    if (IsKeyPressed(KEY_LEFT))  { game.cursor_visible = true; if (game.cursor_col > 0) game.cursor_col--; }
    if (IsKeyPressed(KEY_RIGHT)) { game.cursor_visible = true; if (game.cursor_col < game.cols - 1) game.cursor_col++; }

    // Space/Enter - reveal at cursor
    if (game.cursor_visible && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
        Cell *cell = &game.cells[game.cursor_row * game.cols + game.cursor_col];
        if (cell->state == CELL_HIDDEN) {
            if (game.first_click) {
                place_mines(game.cursor_row, game.cursor_col);
                game.timer_running = true;
            }
            reveal_cell(game.cursor_row, game.cursor_col);
            if (cell->is_mine) {
                reveal_all_mines();
                game.timer_running = false;
                game.screen = SCREEN_GAME_OVER;
            } else if (check_win()) {
                game.timer_running = false;
                if (is_top_score(game.difficulty, (int)game.elapsed)) {
                    game.entry_len = 0;
                    memset(game.entry_name, 0, sizeof(game.entry_name));
                    game.screen = SCREEN_NAME_ENTRY;
                } else {
                    game.screen = SCREEN_GAME_WON;
                }
            }
        }
    }

    // F - flag at cursor
    if (game.cursor_visible && IsKeyPressed(KEY_F)) {
        Cell *cell = &game.cells[game.cursor_row * game.cols + game.cursor_col];
        if (cell->state == CELL_HIDDEN) {
            cell->state = CELL_FLAGGED;
            game.flags_placed++;
        } else if (cell->state == CELL_FLAGGED) {
            cell->state = CELL_HIDDEN;
            game.flags_placed--;
        }
    }

    // Escape - return to menu
    if (IsKeyPressed(KEY_ESCAPE)) {
        game.timer_running = false;
        game.screen = SCREEN_MENU;
    }
}

int main(void) {
    game.difficulty = DIFF_INTERMEDIATE;
    calc_window_size();
    load_leaderboard();

    InitWindow(game.win_w, game.win_h, "Minesweeper");
    SetTargetFPS(60);

    game.screen = SCREEN_MENU;

    while (!WindowShouldClose()) {
        // Auto-pause check
        if (game.screen == SCREEN_PLAYING && !IsWindowFocused()) {
            game.timer_running = false;
            game.screen = SCREEN_PAUSED;
        }
        if (game.screen == SCREEN_PAUSED && IsWindowFocused()) {
            if (!game.first_click) game.timer_running = true;
            game.screen = SCREEN_PLAYING;
        }

        // Timer
        if (game.screen == SCREEN_PLAYING && game.timer_running) {
            game.elapsed += GetFrameTime();
        }

        // Input
        if (game.screen == SCREEN_PLAYING) handle_playing_input();

        // Render
        BeginDrawing();
        ClearBackground(COL_WINDOW_BG);
        switch (game.screen) {
            case SCREEN_MENU:        draw_menu(); break;
            case SCREEN_PLAYING:     draw_header(); draw_board(); break;
            case SCREEN_PAUSED:      draw_header(); draw_paused_overlay(); break;
            case SCREEN_GAME_OVER:   draw_header(); draw_board(); draw_game_over_overlay(); break;
            case SCREEN_GAME_WON:    draw_header(); draw_board(); draw_game_won_overlay(); break;
            case SCREEN_NAME_ENTRY:  draw_name_entry(); break;
            case SCREEN_LEADERBOARD: draw_leaderboard_screen(); break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
