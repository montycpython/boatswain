#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>

#define QUAD_1 0
#define QUAD_2 1
#define QUAD_3 2
#define QUAD_4 3

typedef struct Task {
    char text[256];
    struct Task *next;
} Task;

typedef struct {
    Task *head;
    Task *selected;
    int top_index;
    int count;
} Quadrant;

// -------------------------------------------------------------------
// Escaping for file I/O
// -------------------------------------------------------------------
void escape_string(const char *src, char *dst, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '\\') {
            if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = '\\'; }
        } else if (src[i] == '\n') {
            if (j + 2 < dst_size) { dst[j++] = '\\'; dst[j++] = 'n'; }
        } else {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

void unescape_string(const char *src, char *dst, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '\\' && src[i+1] == 'n') {
            dst[j++] = '\n';
            i += 2;
        } else if (src[i] == '\\' && src[i+1] == '\\') {
            dst[j++] = '\\';
            i += 2;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

// -------------------------------------------------------------------
// File I/O
// -------------------------------------------------------------------
void load_tasks(Quadrant quads[4]) {
    FILE *f = fopen("tasks.dat", "r");
    if (!f) return;
    char line[512];
    int current_quad = -1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#') {
            sscanf(line, "# Quadrant %d", &current_quad);
            current_quad--;
        } else if (current_quad >= 0 && line[0] != '\0') {
            Task *newt = malloc(sizeof(Task));
            unescape_string(line, newt->text, sizeof(newt->text));
            newt->next = NULL;

            if (!quads[current_quad].head) {
                quads[current_quad].head = newt;
            } else {
                Task *last = quads[current_quad].head;
                while (last->next) last = last->next;
                last->next = newt;
            }
            quads[current_quad].count++;
        }
    }
    fclose(f);
    for (int i = 0; i < 4; i++) {
        quads[i].selected = quads[i].head;
        quads[i].top_index = 0;
    }
}

void save_tasks(Quadrant quads[4]) {
    FILE *f = fopen("tasks.dat", "w");
    if (!f) return;
    for (int q = 0; q < 4; q++) {
        fprintf(f, "# Quadrant %d\n", q + 1);
        for (Task *t = quads[q].head; t; t = t->next) {
            char escaped[512];
            escape_string(t->text, escaped, sizeof(escaped));
            fprintf(f, "%s\n", escaped);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

// ----------------------------------------------------------------
// Quadrant drawing (multi‑line, highlight only when active)
// ----------------------------------------------------------------
void draw_quad(WINDOW *win, char *label, int active, Quadrant *quad) {
    werase(win);
    if (active) {
        wattron(win, A_BOLD | A_REVERSE);
        box(win, 0, 0);
        wattroff(win, A_BOLD | A_REVERSE);
    } else {
        box(win, 0, 0);
    }
    mvwprintw(win, 1, 2, "%s", label);

    int maxy, maxx;
    getmaxyx(win, maxy, maxx);
    int inner_height = maxy - 3; // rows available for tasks
    if (inner_height < 1) return;

    int row = 0;
    Task *t = quad->head;
    // skip to the task at top_index
    for (int i = 0; i < quad->top_index && t; i++)
        t = t->next;

    while (t && row < inner_height) {
        char *p = t->text;
        char *line_start = p;

        // Process each line of the task (separated by \n)
        while (*p) {
            if (*p == '\n') {
                // Draw line from line_start to just before newline
                int line_len = p - line_start;
                if (line_len > maxx - 4) line_len = maxx - 4;
                if (row < inner_height) {
                    if (t == quad->selected && active)
                        wattron(win, A_REVERSE);
                    mvwprintw(win, 2 + row, 2, "%-.*s", line_len, line_start);
                    if (t == quad->selected && active)
                        wattroff(win, A_REVERSE);
                    row++;
                }
                p++; // skip the newline
                line_start = p;
            } else {
                p++;
            }
        }
        // Draw the last line (if any)
        if (line_start < p) {
            int line_len = p - line_start;
            if (line_len > maxx - 4) line_len = maxx - 4;
            if (row < inner_height) {
                if (t == quad->selected && active)
                    wattron(win, A_REVERSE);
                mvwprintw(win, 2 + row, 2, "%-.*s", line_len, line_start);
                if (t == quad->selected && active)
                    wattroff(win, A_REVERSE);
                row++;
            }
        }
        t = t->next;
    }
    wrefresh(win);
}
/*

*/
// ----------------------------------------------------------------
// Ensure selected task is visible (task‑based scrolling)
// ----------------------------------------------------------------
void ensure_visible(Quadrant *q, int win_height) {
    int idx = 0;
    Task *t = q->head;
    while (t && t != q->selected) { idx++; t = t->next; }
    if (!t) return;

    if (idx < q->top_index) {
        q->top_index = idx;
    } else if (idx >= q->top_index + win_height) {
        q->top_index = idx - win_height + 1;
    }
    if (q->top_index < 0) q->top_index = 0;
    if (q->count > win_height && q->top_index > q->count - win_height)
        q->top_index = q->count - win_height;
}

// ----------------------------------------------------------------
// Task management
// ----------------------------------------------------------------
void add_task(Quadrant *q, const char *initial_text) {
    Task *newt = malloc(sizeof(Task));
    strcpy(newt->text, initial_text);
    newt->next = NULL;

    if (!q->head) {
        q->head = newt;
    } else {
        Task *last = q->head;
        while (last->next) last = last->next;
        last->next = newt;
    }
    q->count++;
    q->selected = newt;
}

void delete_task(Quadrant *q) {
    if (!q->selected) return;
    Task *to_del = q->selected;
    if (to_del == q->head) {
        q->head = q->head->next;
        q->selected = q->head;
    } else {
        Task *prev = q->head;
        while (prev && prev->next != to_del) prev = prev->next;
        if (prev) {
            prev->next = to_del->next;
            q->selected = prev->next ? prev->next : prev;
        }
    }
    free(to_del);
    q->count--;
    if (!q->selected && q->head) q->selected = q->head;
}

void move_task(Quadrant *src, Quadrant *dst) {
    if (!src->selected) return;
    Task *t = src->selected;

    if (t == src->head) {
        src->head = src->head->next;
    } else {
        Task *prev = src->head;
        while (prev && prev->next != t) prev = prev->next;
        if (prev) prev->next = t->next;
    }
    src->count--;
    src->selected = src->head;

    t->next = NULL;
    if (!dst->head) {
        dst->head = t;
    } else {
        Task *last = dst->head;
        while (last->next) last = last->next;
        last->next = t;
    }
    dst->count++;
}

void move_all_tasks(Quadrant *src, Quadrant *dst) {
    if (!src->head || src == dst) return;
    if (!dst->head) {
        dst->head = src->head;
    } else {
        Task *last = dst->head;
        while (last->next) last = last->next;
        last->next = src->head;
    }
    dst->count += src->count;
    src->head = NULL;
    src->selected = NULL;
    src->count = 0;
    src->top_index = 0;
}

void free_tasks(Quadrant *q) {
    Task *t = q->head;
    while (t) {
        Task *next = t->next;
        free(t);
        t = next;
    }
    q->head = q->selected = NULL;
    q->count = q->top_index = 0;
}

// ----------------------------------------------------------------
// Helper: convert text index to screen (row, col)
// ----------------------------------------------------------------
static void index_to_screen(const char *s, int index, int *row, int *col) {
    int r = 0, c = 0;
    for (int i = 0; i < index && s[i]; i++) {
        if (s[i] == '\n') { r++; c = 0; }
        else c++;
    }
    *row = r;
    *col = c;
}

// ----------------------------------------------------------------
// Multi‑line editor with wrap and colon+! exit
// ----------------------------------------------------------------
void edit_task(WINDOW *win, Task *t) {
    scrollok(win, TRUE);
    int idx = (int)strlen(t->text);
    int ch;

    curs_set(1);
    noecho();

    while (1) {
        int maxy, maxx;
        getmaxyx(win, maxy, maxx);
        for (int y = 2; y < maxy - 1; y++)
            for (int x = 2; x < maxx - 2; x++)
                mvwaddch(win, y, x, ' ');

        int line = 0, col = 2;
        for (char *p = t->text; *p; p++) {
            if (line + 2 >= maxy - 1) break;
            if (*p == '\n') {
                line++;
                col = 2;
                continue;
            }
            if (col < maxx - 2) {
                mvwaddch(win, 2 + line, col, *p);
                col++;
            } else {
                // Wrap to next line
                line++;
                col = 2;
                if (line + 2 >= maxy - 1) break;
                mvwaddch(win, 2 + line, col, *p);
                col++;
            }
        }

        int cur_row, cur_col;
        index_to_screen(t->text, idx, &cur_row, &cur_col);
        if (2 + cur_row >= maxy - 1)
            wscrl(win, (2 + cur_row) - (maxy - 2) + 1);
        wmove(win, 2 + cur_row, 2 + cur_col);
        wrefresh(win);
        /*
        ch = wgetch(win);
        if (ch == ':') {
            int next = wgetch(win);
            waddch(win, ':'); 
            wrefresh(win); 
            if (next == '!') break; 

            waddch(win, '\b'); 
            if (strlen(t->text) < 255) {
                memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                t->text[idx++] = ':';
                if (next >= 32 && next <= 126 && strlen(t->text) < 255) {
                    memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                    t->text[idx++] = next;
                }
            }
        } else {
            if (next >= 32 && next <= 126 && strlen(t->text) < 255) {
                memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                t->text[idx++] = next;
            }
        }


        */
        ch = wgetch(win);
        if (ch == ':') {
            waddch(win, ':'); 
            wrefresh(win); 
            int next = wgetch(win);

            if (next == '!') {
                break; 
            } else {
                waddch(win, '\b'); 
                if (strlen(t->text) < 255) {
                    memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                    t->text[idx++] = ':';
                    if (next >= 32 && next <= 126 && strlen(t->text) < 255) {
                        memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                        t->text[idx++] = next;
                    }
                }
            }
        }

/*        if (ch == ':') {
            if (strlen(t->text) < 255) {
                memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                t->text[idx] = ':';
                idx++;
            }

            int next = wgetch(win);
            if (next == '!') {
                if (idx > 0) {
                    memmove(t->text + idx - 1, t->text + idx, strlen(t->text + idx) + 1);
                    idx--;
                }
                break; // Trigger save/exit
            } else {
                if (next >= 32 && next <= 126 && strlen(t->text) < 255) {
                    memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                    t->text[idx] = next;
                    idx++;
                }
            }
        }
*/
        else if (ch == KEY_LEFT) {
            if (idx > 0) idx--;
        }
        else if (ch == KEY_RIGHT) {
            if (t->text[idx] != '\0') idx++;
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (idx > 0) {
                memmove(t->text + idx - 1, t->text + idx, strlen(t->text + idx) + 1);
                idx--;
            }
        }
        else if (ch == '\n' || ch == KEY_ENTER) {
            if (strlen(t->text) < 255) {
                memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                t->text[idx] = '\n';
                idx++;
            }
        }
        else if (ch >= 32 && ch <= 126) {
            if (strlen(t->text) < 255) {
                memmove(t->text + idx + 1, t->text + idx, strlen(t->text + idx) + 1);
                t->text[idx] = ch;
                idx++;
            }
        }
    }

    curs_set(0);
    echo();
    scrollok(win, FALSE);
}

// -------------------------------------------------------------------
// Main
// -------------------------------------------------------------------
int eisen() {
    Quadrant quads[4] = {0};
    load_tasks(quads);

    int rows, cols, height, width;
    int activeQ = QUAD_1;
    int ch;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    refresh();

    getmaxyx(stdscr, rows, cols);
    height = rows / 2;
    width = cols / 2;

    WINDOW *q[4];
    q[QUAD_1] = newwin(height, width, 0, 0);
    q[QUAD_2] = newwin(height, width, 0, width);
    q[QUAD_3] = newwin(height, width, height, 0);
    q[QUAD_4] = newwin(height, width, height, width);
    for (int i = 0; i < 4; i++) {
        keypad(q[i], TRUE);
    }

    char *labels[] = {"1. DO/CONCEPT", "2. SCHEDULE/SKELETON",
                      "3. DELEGATE/FLESH", "4. DELETE/FINALIZE"};

    while ((ch = getch()) != 'q') {
        // Quadrant movement (h/j/k/l)
        switch (ch) {
            case 'h':
                if (activeQ == QUAD_2) activeQ = QUAD_1;
                else if (activeQ == QUAD_4) activeQ = QUAD_3;
                break;
            case 'l':
                if (activeQ == QUAD_1) activeQ = QUAD_2;
                else if (activeQ == QUAD_3) activeQ = QUAD_4;
                break;
            case 'k':
                if (activeQ == QUAD_3) activeQ = QUAD_1;
                else if (activeQ == QUAD_4) activeQ = QUAD_2;
                break;
            case 'j':
                if (activeQ == QUAD_1) activeQ = QUAD_3;
                else if (activeQ == QUAD_2) activeQ = QUAD_4;
                break;
        }

        Quadrant *cur = &quads[activeQ];

        // Selection movement (arrow keys)
        if (ch == KEY_UP) {
            if (!cur->selected) {
                cur->selected = cur->head;
            } else {
                Task *prev = NULL;
                for (Task *t = cur->head; t && t != cur->selected; t = t->next)
                    prev = t;
                if (prev) cur->selected = prev;
            }
            int maxy, maxx;
            getmaxyx(q[activeQ], maxy, maxx);
            ensure_visible(cur, maxy - 3);
        }
        else if (ch == KEY_DOWN) {
            if (cur->selected && cur->selected->next)
                cur->selected = cur->selected->next;
            else if (!cur->selected && cur->head)
                cur->selected = cur->head;
            int maxy, maxx;
            getmaxyx(q[activeQ], maxy, maxx);
            ensure_visible(cur, maxy - 3);
        }

        // Commands
        else if (ch == 'a') {
            add_task(cur, "");
            int maxy, maxx;
            getmaxyx(q[activeQ], maxy, maxx);
            ensure_visible(cur, maxy - 3);
            if (cur->selected) edit_task(q[activeQ], cur->selected);
        }
        else if (ch == 'd') {
            delete_task(cur);
            int maxy, maxx;
            getmaxyx(q[activeQ], maxy, maxx);
            ensure_visible(cur, maxy - 3);
        }
        else if (ch == 'm') {
            if (!cur->selected) continue;
            echo();
            curs_set(1);
            mvwprintw(q[activeQ], 2, 2, "Move to quadrant (1-4): ");
            wrefresh(q[activeQ]);
            int target = wgetch(q[activeQ]);
            noecho();
            curs_set(0);
            if (target >= '1' && target <= '4') {
                int dest = target - '1';
                if (dest != activeQ) {
                    move_task(cur, &quads[dest]);
                }
            }
        }
        else if (ch == 'x') {   // <-- LOWERCASE X moves entire quadrant
            if (cur->count == 0) continue;
            echo();
            curs_set(1);
            mvwprintw(q[activeQ], 2, 2, "Move ALL to quadrant (1-4): ");
            wrefresh(q[activeQ]);
            int target = wgetch(q[activeQ]);
            noecho();
            curs_set(0);
            if (target >= '1' && target <= '4') {
                int dest = target - '1';
                if (dest != activeQ) {
                    move_all_tasks(cur, &quads[dest]);
                }
            }
        }
        else if (ch == '\n' || ch == KEY_ENTER || ch == 'i') {
            if (cur->selected) edit_task(q[activeQ], cur->selected);
        }

        // Redraw all quadrants
        for (int i = 0; i < 4; i++) {
            draw_quad(q[i], labels[i], (i == activeQ), &quads[i]);
        }
    }

    save_tasks(quads);
    for (int i = 0; i < 4; i++) free_tasks(&quads[i]);
    endwin();
    return 0;
}

int main() {
    time_t startTime;
    struct tm *locTime;
    time(&startTime);
    locTime = localtime(&startTime);
    printf("\n\n\nBoatswain begins around: %s", asctime(locTime));
    printf("I give myself one year from now to build it.\n");
    eisen();
    return 0;
}
