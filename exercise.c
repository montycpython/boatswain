#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <string.h>
#include <time.h>
/*Program attempts to solve the issue of
*Calesthetics to build and maintain 
*Physical Work Capacity...
 *01 Nervous - Bridge - CEO & Strategy
 *02 Cardiovascular - Propulsion - Sales
 *03 Respiratory - The Deck - Fulfillment
 *04 Digestive - Starboard - Supply Chain
 *05 Excretory - Portside - Accounting
 *06 Endocrine - Quarter Deck - Infrastructure
 *07 Skeletal - The Keel - Mission / HR
 *08 Muscular - The Bulkheads - Operations
 *09 Sensory - The Watch - Marketing/ MNGMT
 *10 Immuno-Lymphatic - The Ballast & Rudder
 *11 Reproductive - Harbor Master's Office/R&D
 *12 Integumentary - The Hull - Compliance && Legal
*TUI View for The Deck
*For Personal Maintenance/Warm-Up
*For Delivery/ Fulfillment / Respiratory fun
*Modifiable from the Bridge
*
*
*
*
*
*
*
* 
*/
typedef struct {
    char user_name[50];
    int start_day_pushups;
    int current_day_overall;
    time_t woTime;
    int session_failed;
    int failed_exercise;
    int failed_reps_remaining;
    int failed_time_remaining;
    time_t fail_time;
} UserProgress;
void sp(UserProgress up) {
    FILE *f = fopen("progress.dat", "wb");
    if (f) {
        fwrite(&up, sizeof(UserProgress), 1, f);
        fclose(f);
    }
}

UserProgress load_progress() {
    UserProgress up = {"Saitama", 1, 1, 0, 0, 0, 0, 0, 0};
    FILE *f = fopen("progress.dat", "rb");
    if (f) {
        size_t read = fread(&up, sizeof(UserProgress), 1, f);
        fclose(f);
        if (read != 1) {
            up.session_failed = 0;
            up.failed_exercise = 0;
            up.failed_reps_remaining = 0;
            up.failed_time_remaining = 0;
            up.fail_time = 0;
        }
    }
    return up;
}
void custom_flash(short pair_index, int rate) {
    bkgd(COLOR_PAIR(pair_index));
    refresh();
    napms(rate); //50 - 80
    bkgd(COLOR_PAIR(0));
    refresh();
}
void center_print(int row, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    mvprintw(row, (getmaxx(stdscr) - strlen(buf)) / 2, "%s", buf);
}
void display_menu(UserProgress up, int day, int glute_bridges, int dead_bugs, int lunges, int pushups, int plank_time, int inverted_rows, int pullups, int hang_time) {
    int col = getmaxx(stdscr);
    char tBuffer[80];
    struct tm *info = localtime(&up.woTime);
    if (info) {
        strftime(tBuffer, sizeof(tBuffer), "%A @%I:%M %p", info);
    } else {
        snprintf(tBuffer, sizeof(tBuffer), "No record available.");
    }
    int start_col = (col - 40) / 2;
    center_print(1, "--- WORKOUT NUMBER ONE ---");
    center_print(2, "Last Session: %s", tBuffer);
    center_print(3, "Day %d of 100", day);
//start
    mvprintw(5, start_col, "1. Glute Bridges: %3d reps", glute_bridges);
    mvprintw(6, start_col, "2. Dead Bugs:     %3d reps", dead_bugs);
    mvprintw(7, start_col, "3. Lunges:        %3d reps (each leg)", lunges / 2);
    mvprintw(8, start_col, "4. Push-ups:      %3d reps", pushups);
    mvprintw(9, start_col, "5. Plank:         %3d seconds", plank_time);
    mvprintw(10, start_col, "6. Inverted Rows: %3d reps", inverted_rows);
    mvprintw(11, start_col, "7. Pull-ups:      %3d reps", pullups);
    mvprintw(12, start_col, "8. Bar Hang:      %3d seconds", hang_time);
/*
//finish Exercise Preview
*/
    center_print(14, "Press 's' to start the timer...");
    center_print(15, "Press 'q' to postpone...");
    center_print(16, "Press 'r' to reset the program...");
    center_print(18, "%s...", up.user_name);
}
// Returns 1 if failed, 0 if completed
int do_static_hold(UserProgress *up, int exercise_num, const char *name, int seconds, const char *emoji) {
    center_print(10, "START: %s %d seconds", name, seconds);
    refresh();
    sleep(5);
    halfdelay(1);
    for (int i = seconds; i >= 0; i--) {
        move(12, 0);
        clrtoeol();
        center_print(12, "%s TIME REMAINING: %d seconds", name, i);
        center_print(13, "%s", emoji);
        custom_flash(4, 200);
        int key = getch();
        if (key == 'f') {
            // Save failure and return to menu
            up->session_failed = 1;
            up->failed_exercise = exercise_num;
            up->failed_time_remaining = i;
            up->failed_reps_remaining = 0;
            up->fail_time = time(NULL);
            sp(*up);
            return 1;
        }
        if (i > 0) sleep(1);
    }
    custom_flash(2, 150);
    move(10, 0);
    clrtoeol();
    move(12, 0);
    clrtoeol();
    move(13, 0);
    clrtoeol();
    return 0;
}
// Returns 1 if failed, 0 if completed
int do_reps(UserProgress *up, int exercise_num, const char *name, int reps, const char *emoji, const char *rep_label) {
    center_print(10, "Get ready for %s!", name);
    custom_flash(2, 200);
    sleep(5);
    move(10, 0);
    clrtoeol();
    halfdelay(1);
    for (int i = 1; i <= reps; i++) {
        move(12, 0);
        clrtoeol();
        center_print(12, "%s Rep: %d", rep_label, i);
        center_print(13, "%s", emoji);
        refresh();
        int key = getch();
        if (key == 'f') {
            // Save failure and return to menu
            up->session_failed = 1;
            up->failed_exercise = exercise_num;
            up->failed_reps_remaining = reps - i + 1; // Remaining including current
            up->failed_time_remaining = 0;
            up->fail_time = time(NULL);
            sp(*up);
            return 1;
        }
        custom_flash(4, 150); 
        sleep(2);
    }
    flash();
    move(12, 0);
    clrtoeol();
    move(13, 0);
    clrtoeol();
    return 0;
}
void run_workout(UserProgress *up, int col, int day, int glute_bridges, int dead_bugs, 
int lunges, int pushups, int plank_time, int inverted_rows, int pullups, int hang_time) {
    int start_exercise = 1;
    int adjusted_bridges = glute_bridges;
    int adjusted_dead_bugs = dead_bugs;
    int adjusted_lunges = lunges;
    int adjusted_pushups = pushups;
    int adjusted_plank = plank_time;
    int adjusted_rows = inverted_rows;
    int adjusted_pullups = pullups;
    int adjusted_hang = hang_time;
    // Check if resuming failed session
    if (up->session_failed && up->failed_exercise > 0) {
        start_exercise = up->failed_exercise;
        // Restore remaining amounts
        if (up->failed_exercise == 1) adjusted_bridges = up->failed_reps_remaining;
        else if (up->failed_exercise == 2) adjusted_dead_bugs = up->failed_reps_remaining;
        else if (up->failed_exercise == 3) adjusted_lunges = up->failed_reps_remaining;
        else if (up->failed_exercise == 4) adjusted_pushups = up->failed_reps_remaining;
        else if (up->failed_exercise == 5) adjusted_rows = up->failed_reps_remaining;
        else if (up->failed_exercise == 6) adjusted_plank = up->failed_time_remaining;
        else if (up->failed_exercise == 7) adjusted_pullups = up->failed_reps_remaining;
        else if (up->failed_exercise == 8) adjusted_hang = up->failed_time_remaining;
        // Clear failure flag since resuming
        up->session_failed = 0;
        up->failed_exercise = 0;
    }
    // Exercise 1: Glute Bridges
    if (start_exercise <= 1) {
        if (do_reps(up, 1, "Glute Bridges", adjusted_bridges, "🥷🍑⌚💪🏾💯", "Bridge")) return;
    }
    // Exercise 2: Dead Bugs
    if (start_exercise <= 2) {
        if (do_reps(up, 2, "Dead Bugs", adjusted_dead_bugs, "🥷🐛⌚💪🏾💯", "Dead Bug")) return;
    }
    // Exercise 3: Lunges
    if (start_exercise <= 3) {
        if (do_reps(up, 3, "Lunges", adjusted_lunges, "🥷🦵⌚💪🏾💯", "Lunge")) return;
    }
    // Exercise 4: Push-ups
    if (start_exercise <= 4) {
        if (do_reps(up, 4, "Push-ups", adjusted_pushups, "🥷💪⌚💪🏾💯", "Push-up")) return;
    }
    // Exercise 5: Plank
    if (start_exercise <= 5) {
        if (do_static_hold(up, 5, "Plank", adjusted_plank, "🥷📏⌚💪🏾💯")) return;
    }
    // Exercise 6: Inverted Rows
    if (start_exercise <= 6) {
        if (do_reps(up, 6, "Inverted Rows", adjusted_rows, "🥷🚣⌚💪🏾💯", "Row")) return;
    }
    // Exercise 7: Pull-ups
    if (start_exercise <= 7) {
        if (do_reps(up, 7, "Pull-ups", adjusted_pullups, "🥷🆙⌚💪🏾💯", "Pull-up")) return;
    }
    // Exercise 8: Bar Hang
    if (start_exercise <= 8) {
        if (do_static_hold(up, 8, "Bar Hang", adjusted_hang, "🥷⛴️⌚💪🏾💯")) return;
    }
    // All complete
    center_print(10, "WORKOUT COMPLETE! GREAT JOB!");
    center_print(12, "Press 'x' to log and exit.");
    up->start_day_pushups++;
    up->current_day_overall++;
    up->woTime = time(NULL);
    sp(*up); refresh();
    flushinp();
    while (getch() != 'x');
}
int main(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    curs_set(0);
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
    UserProgress up = load_progress();
    int col = getmaxx(stdscr);
    int day = up.current_day_overall;
    int pushups = (day > 100) ? 100 : day;
    int lunge_progression = 2 + (((day - 1) / 2) * 2);
    int lunges = (lunge_progression > 100) ? 100 : lunge_progression;
    int hang_time = 2 + ((day - 1) * 2);
    hang_time = (hang_time > 200) ? 200 : hang_time;
    int plank_time = 2 + ((day - 1) * 2);
    plank_time = (plank_time > 200) ? 200 : plank_time;
    int dead_bugs = (day > 100) ? 100 : day;
    int glute_bridges = (day > 100) ? 100 : day;
    int row_progression = 1 + ((day - 1) / 2);
    int inverted_rows = (row_progression > 50) ? 50 : row_progression;
    int pullup_progression = 1 + ((day - 1) / 3);
    int pullups = (pullup_progression > 34) ? 34 : pullup_progression;
    while (1) {
        clear();
        display_menu(up, day, glute_bridges, dead_bugs, lunges, pushups,
             plank_time, inverted_rows, pullups, hang_time);
        custom_flash(3, 250);
        flushinp();
        int ch;
        while ((ch = getch()) != 's') {
            if (ch == 'q') {
                endwin();
                return 0;
            } else if (ch == 'r') {
                int aborted = 0;
                halfdelay(1);
                for (int i = 15; i >= 0; i--) {
                    for (int j = 0; j < 10; j++) {
                        custom_flash(1, 50);
                        mvprintw(19, 0, "Regimen Destruction in %d seconds", i);
                        printw(" 🫡🤔😢🥷🥶😭");
                        custom_flash(3, 50);
                        int ac = getch();
                        if (ac == 'a' || ac == 'w' || ac == 'h' || ac == 's') {
                            aborted = 1;
                            move(19, 0);
                            clrtoeol();
                            mvprintw(19, 0, "Continue, 🫡🌞😆🤑💯💰💪🏾 \nYou decided to persevere.");
                            refresh();
                            break;
                        }
                    }
                    if (aborted) break;
                    move(17, 0);
                    clrtoeol();
                    center_print(17, "PRESS 'A', 'W', 'S', or 'H' TO ABORT 'R'");
                    refresh();
                }
                cbreak();
                if (aborted) {
                    move(19, 0);
                    clrtoeol();
                    move(17, 0);
                    clrtoeol();
                    center_print(19, "ABORTED. REGIMEN SAFE.");
                    refresh();
                    sleep(2);
                } else {
                    up.start_day_pushups = 1;
                    up.current_day_overall = 1;
                    up.session_failed = 0;
                    up.failed_exercise = 0;
                    sp(up);
                    center_print(19, "SYSTEM RESET. RESTART APP.");
                    refresh();
                    sleep(2);
                    endwin();
                    exit(0);
                }
            }
        }
        // Start workout
        clear();
        refresh();
        run_workout(&up, col, day, glute_bridges, dead_bugs, lunges,
                    pushups, plank_time, inverted_rows, pullups, hang_time);
        //Reload day in case of failure (same day) or completion (next day)
        day = up.current_day_overall;
        //Recalculate progressions for new day
        pushups = (day > 100) ? 100 : day;
        lunge_progression = 2 + (((day - 1) / 2) * 2);
        lunges = (lunge_progression > 100) ? 100 : lunge_progression;
        hang_time = 2 + ((day - 1) * 2);
        hang_time = (hang_time > 200) ? 200 : hang_time;
        plank_time = 2 + ((day - 1) * 2);
        plank_time = (plank_time > 200) ? 200 : plank_time;
        dead_bugs = (day > 100) ? 100 : day;
        glute_bridges = (day > 100) ? 100 : day;
        row_progression = 1 + ((day - 1) / 2);
        inverted_rows = (row_progression > 50) ? 50 : row_progression;
        pullup_progression = 1 + ((day - 1) / 3);
        pullups = (pullup_progression > 34) ? 34 : pullup_progression;
    }
    endwin();
    return 0;
}
