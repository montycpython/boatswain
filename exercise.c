#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <string.h>
#include <time.h>

typedef struct {
    char user_name[50];
    int start_day_pushups;
    int current_day_overall;
    time_t woTime;
} UserProgress;

void sp(UserProgress up) {
    FILE *f = fopen("progress.dat", "wb");
    if (f) {
        fwrite(&up, sizeof(UserProgress), 1, f);
        fclose(f);
    }
}

UserProgress load_progress() {
    UserProgress up = {"Saitama", 1, 1, 0};
    FILE *f = fopen("progress.dat", "rb");
    if (f) {
        fread(&up, sizeof(UserProgress), 1, f);
        fclose(f);
    }
    return up;
}

int main(void){
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    UserProgress up = load_progress();
    int col = getmaxx(stdscr);
    //getmaxyx(stdscr, row, col);
    char tBuffer[80];
    struct tm *info = localtime(&up.woTime);
    if (info) {
        strftime(tBuffer, sizeof(tBuffer), "%A @%I:%M %p", info);
    } else {
        snprintf(tBuffer, sizeof(tBuffer), "No record available.");
    }
    int pushups = (up.start_day_pushups > 100) ? 100 : up.start_day_pushups;
    int lunge_progression = ((up.current_day_overall + 1) / 2) * 2;
    int lunges = (lunge_progression > 100) ? 100 : lunge_progression;
    int hang_time = (up.current_day_overall > 112) ? 224 : up.current_day_overall * 2;

    mvprintw(1, (col - 30) / 2, "--- WORKOUT TRACKER ---");
//mvprintw(2, (col - 30) / 2, "---- Last Check-in %ld----", up.woTime);
    mvprintw(2, (col - 30) / 2, "Last Session: %s", tBuffer);

    mvprintw(4, (col - 30) / 2, "Push-up Day %d: %d reps", up.start_day_pushups, pushups);
    mvprintw(5, (col - 30) / 2, "Lunge Day %d:   %d reps (each)", up.current_day_overall, lunges);
    mvprintw(6, (col - 30) / 2, "Hang Time:      %d seconds", hang_time);
    mvprintw(7, (col - 30) / 2, "Finish with:    1 Pull-up");
    mvprintw(10, (col - 34) / 2, "Press 's' to start the timer...");
    mvprintw(11, (col - 34) / 2, "Press 'q' to postpone...");
    mvprintw(12, (col - 34) / 2, "Press 'r' to reset the program...");
    mvprintw(14, (col - strlen(up.user_name)) / 2, "%s...", up.user_name);
    refresh();
    flash();
    flushinp();

    int ch;
    while ((ch = getch()) != 's') {
        if (ch == 'q') {
            endwin();
            return 0;
        }
        else if (ch == 'r') {
            int aborted = 0;
            halfdelay(1);
            for (int i = 15; i >= 0; i--) {
                for (int j = 0; j < 10; j++) {
                    flash();
                    mvprintw(15, 0, "Regimen Destruction in %d seconds", i);
                    printw(" 🫡🤔😢🥷🥶😭");
                    refresh();
                    int ac = getch();
                    if (ac == 'a' || ac == 'w' || ac == 'h' || ac == 's') {
                        aborted = 1;
                        move(15, 0); clrtoeol();
                        mvprintw(15, 0, "Continue, 🫡🌞😆🤑💯💰💪🏾 \nYou decided to persevere.");
                        refresh();
                        break;
                    }
                }
                if (aborted) break;
                move(13, 0);
                clrtoeol();
                mvprintw(13, (col - 30) / 2, "PRESS 'A', 'W', 'S', or 'H' TO ABORT 'R'");
                refresh();
            }
            cbreak();
            if (aborted) {
                move(15, 0); 
                clrtoeol();
                move(13, 0); clrtoeol();
                mvprintw(15, (col - 20) / 2, "ABORTED. REGIMEN SAFE.");
                refresh();
                sleep(2);
                move(15, 0); clrtoeol();
                refresh();
            } else {
                up.start_day_pushups = 1;
                up.current_day_overall = 1;
                sp(up);
                move(15, 0); clrtoeol();
                mvprintw(15, (col - 30) / 2, "SYSTEM RESET. RESTART APP.");
                refresh();
                sleep(2);
                endwin();
                exit(0);
            }
        }
    }
    move(10, 0);    clrtoeol();
    move(11, 0);    clrtoeol();
    move(12, 0);    clrtoeol();
    move(13, 0);    clrtoeol();
    mvprintw(10, (col - 30) / 2, "START LUNGES: %d Reps Total", lunges * 2);
refresh();
    sleep(5);
    for (int i = 1; i <= (lunges * 2); i++) {
    move(12, 0);    clrtoeol();
    mvprintw(12, (col - 20) / 2, "Lunge Rep: %d", i);
    mvprintw(13, 0, "🥷⛴️⌚💪🏾💯");    
    refresh();
    sleep(2); // The 2-second cadence
    flash();
}
flash();

    move(13, 0);    clrtoeol();
    mvprintw(13, (col-30)/2, "Get ready to hang!");
    flash();
    refresh();
    sleep(5);
    cbreak();
    for (int i = hang_time; i >= 0; i--) {
        move(12, 0);    clrtoeol();
        mvprintw(12, (col - 30) / 2, "TIME REMAINING: %d seconds", i);
        refresh();
        if (i > 0) sleep(1);
    }
    flash();
    mvprintw(14, (col - 30) / 2, "TIME UP! PERFORM PULL-UP NOW!");
    mvprintw(16, (col - 32) / 2, "Press 'x' to log and exit.");
    up.start_day_pushups++;
    up.current_day_overall++;
    up.woTime = time(NULL);
    sp(up);
    refresh();
    flushinp();
    while (getch() != 'x');
    endwin();
    return 0;
}

