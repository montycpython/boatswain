#include <stdio.h>
#include <time.h>
#include <ncurses.h>
#include <string.h>

#define QUAD_1 0
#define QUAD_2 1
#define QUAD_3 2
#define QUAD_4 3

void draw_quad(WINDOW *win, char *label, int active, char *txt) {
    werase(win);
    if (active) {
        wattron(win, A_BOLD | A_REVERSE);
        box(win, 0, 0);
        wattroff(win, A_BOLD | A_REVERSE);
    } else {
        box(win, 0, 0);
    }
    mvwprintw(win, 1, 2, "%s", label);
    mvwprintw(win, 2, 2, "%s", txt); // Display the text for this box
    wrefresh(win);
}
/*
*/
int eisen() {
    char buffers[4][256] = {"", "", "", ""}; // Stores text for each quadrant
    int rows, cols, height, width;
    int activeQ = QUAD_1;
    int ch;
    FILE *f = fopen("tasks.dat", "r");
if (f) {
    for (int i = 0; i < 4; i++) {
        fgets(buffers[i], 254, f);
        // Remove trailing newline added by fgets
        buffers[i][strcspn(buffers[i], "\n")] = 0;
    }
    fclose(f);
}

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    refresh(); // Push stdscr to background

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
    char *labels[] = {"1. DO/CONCEPT", "2. SCHEDULE/SKELETON", "3. DELEGATE/FLESH", "4. DELETE/FINALIZE"};

    while ((ch = getch()) != 'q') { // Press 'q' to quit
        // Update active quadrant based on Vim keys
        switch (ch) {
            case 'h': // Left
                if (activeQ == QUAD_2) activeQ = QUAD_1;
                else if (activeQ == QUAD_4) activeQ = QUAD_3;
                break;
            case 'l': // Right
                if (activeQ == QUAD_1) activeQ = QUAD_2;
                else if (activeQ == QUAD_3) activeQ = QUAD_4;
                break;
            case 'k': // Up
                if (activeQ == QUAD_3) activeQ = QUAD_1;
                else if (activeQ == QUAD_4) activeQ = QUAD_2;
                break;
            case 'j': // Down
                if (activeQ == QUAD_1) activeQ = QUAD_3;
                else if (activeQ == QUAD_2) activeQ = QUAD_4;
                break;
            case 'i':
                echo();
                curs_set(1);
                int pos = strlen(buffers[activeQ]); 
                int input_ch;
                int cursor_x = 2 + pos; 
                wmove(q[activeQ], 2, cursor_x);
                wrefresh(q[activeQ]);
    while (1) {
        input_ch = wgetch(q[activeQ]);

        if (input_ch == '!' && pos > 0 && buffers[activeQ][pos-1] == ':') {
            buffers[activeQ][--pos] = '\0'; 
            break; 
        }

        if (input_ch == KEY_BACKSPACE || input_ch == 127 || input_ch == '\b') {
            if (pos > 0) {
                pos--;
                buffers[activeQ][pos] = '\0';
                mvwaddch(q[activeQ], 2, 2 + pos, ' ');
                cursor_x = 2 + pos;
                wmove(q[activeQ], 2, cursor_x);
            }
        } 
        else if (input_ch == KEY_LEFT && cursor_x > 2) {
            cursor_x--;
            wmove(q[activeQ], 2, cursor_x);
        }
        else if (input_ch == KEY_RIGHT && cursor_x < 2 + pos) {
            cursor_x++;
            wmove(q[activeQ], 2, cursor_x);
        }
        else if (input_ch >= 32 && input_ch <= 126 && pos < 254) {
            buffers[activeQ][pos++] = input_ch;
            buffers[activeQ][pos] = '\0';
            cursor_x = 2 + pos;
        }
        wrefresh(q[activeQ]);
    }
        noecho();
        curs_set(0);
        draw_quad(q[activeQ], labels[activeQ], 1, buffers[activeQ]);
        FILE *f = fopen("tasks.dat", "w");
        if (f) {
            for (int i = 0; i < 4; i++) {
                fprintf(f, "%s\n", buffers[i]);
            } 
         fclose(f);
    }

    break;
        }
        // Redraw all quadrants with the new focus
        for (int i = 0; i < 4; i++) {
            draw_quad(q[i], labels[i], (i == activeQ), buffers[i]);
        }
    }
    endwin();
    return 0;
}
//---------->>>
int main() {
    time_t startTime;
    struct tm *locTime;
    // Get the current calendar time as a time_t object
    time(&startTime); 
    // Convert the time_t value to local time (fills the tm structure)
    locTime = localtime(&startTime); 
    // Convert the struct tm to a human-readable string and print it
    printf("\n\n\nBoatswain begins around: %s", asctime(locTime));
    printf("Boatswain begins around: %s", asctime(locTime));
    printf("I give myself one year from now to build it.\n");
    eisen();
    return 0;
}
