/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Martino Pilia, 2014
 */

/*!
 * \file pong.c
 * \brief Pong game
 *
 * This is a clone of the pong game, implemented in c with ncurses interface.
 *
 * The game main thread act as a controller, receiving data from three 
 * children threads: one for the keyboard input handling, one controlling the
 * ball position and one for the ai moves. Another thread is used as signal
 * listener, handling kill/int/term and terminal resize signals. Signals are 
 * blocked during program initialization and then managed with a signal file 
 * descriptor and a poll from the kernel. Thread comunication is provided 
 * with a pipe.
 *
 * Note that ncurses is not thread safe, so operations on the window
 * must be inside a critical zone secured with a mutex.
 *
 * Note: the program uses a system call to change the keyboard settings for a
 * smooth playing, and previous settings are restored before game exit.
 * System keyboard settings are managed throug xset command, so the game
 * requires to run into a X session.
 * 
 * @author Martino Pilia
 * @date 2014-11-23
 */

#include <ncurses.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include "support.h"

/* global variables for keyboard delay and rate settings */
char del[4];
char rate[3];

int main(int argc, char *argv[])
{
    char buf[TAG_SIZE + 1]; /* buffer to take tags from the pipe in*/
    pthread_t keyboard_handler_thread; /* thread for keyboard handling */
    pthread_t ball_handler_thread; /* thread for ball position generation */
    pthread_t ai_handler_thread; /* thread for ai position handling */
    pthread_t signal_thread; /* thread for signal listening */
    FILE *sett[2]; /* pipes to read xorg key settings */
    game_data data; /* game data shared between threads */
    sigset_t sigset; /* signal set */

    srand(getpid());

    /* create signal set containing resize and kill/int/term signals */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGWINCH);
    sigaddset(&sigset, SIGKILL);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT);

    /* block signals */
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    /* create pipe for signal handling */
    data.signal_fd = signalfd(-1, &sigset, 0); 

    /* read typematic settings (repeat delay and rate) from system 
     * and save them into global variables */
    sett[0] =  
        popen("xset q | grep 'auto repeat delay:' |"
                " egrep -o '([0-9])+' | sed -n '1p'", "r");
    sett[1] =
        popen("xset q | grep 'auto repeat delay:' |"
                " egrep -o '([0-9])+' | sed -n '2p'", "r");
    fgets(del, 4, sett[0]);
    fgets(rate, 3, sett[1]);

    /* change key delay and rate (for smoother playing) */
    system("xset r rate 100 30");

    /* init game data */
    data.exit_flag = 0;
    data.play_flag = 0;
    pthread_mutex_init(&data.mut, NULL);
    if (pipe(data.pipedes) == -1)
    {
        perror("Pipe creation error\n");
        exit(EXIT_FAILURE);
    }

    /* ncurses init */
    initscr();   /* init screen */
    noecho();    /* no keyboard echo on screen */
    curs_set(0); /* hide cursor */
    keypad(stdscr, TRUE); /* enable special keys */
    timeout(0);  /* non-blocking input */

    /* get bottom row of the field */
    data.bottom_row = getmaxy(stdscr) - 1; 

    /* check for color capability */
    if (has_colors() == FALSE)
    {
        printw("Your terminal does not support color.\n");
        exit(EXIT_FAILURE);
    }
    start_color();

    /* set color pair (foreground/background) for paddle drawing */
    init_pair(PADDLE_COLOR, COLOR_WHITE, COLOR_BLUE);

    /* set color pair for ball */
    init_pair(BALL_COLOR, COLOR_RED, COLOR_BLACK);

    /* set color pair for title */
    init_pair(TITLE_COLOR, COLOR_GREEN, COLOR_BLACK);

    /* set color pair for ai */
    init_pair(AI_COLOR, COLOR_WHITE, COLOR_YELLOW);

    /* create thread for signal listening */
    pthread_create(
            &signal_thread,
            NULL,
            signal_listener,
            &data);

    print_intro_menu(stdscr);

    /* each iteration is a single game */
    do {
        char c;
        /* wait until the user press space (game start) or q (quit) */
        do { 
            c = getch();
            if (c == QUIT_KEY)
                /* safe because threads have not been created yet */
                termination_handler(); 
        } while (c != ' ');

        /* play status on */
        data.play_flag = 1;
        data.termination_flag = 0; /* zero to run, non-zero to terminate */

        /* clear screen */
        clear();

        /* init player paddle */
        data.paddle_pos = (PADDLE_WIDTH / 2 
                + data.bottom_row - PADDLE_WIDTH / 2) / 2;
        data.paddle_col = getmaxx(stdscr) - 1;
        draw_paddle(&data, KBD_TAG);

        /* init ai paddle */
        data.ai_paddle_pos = data.paddle_pos;
        data.ai_paddle_col = 1;
        draw_paddle(&data, AI_TAG);

        /* init ball */
        data.ball_x_old = data.ball_x = data.paddle_col - 1;
        data.ball_y_old = data.ball_y = data.paddle_pos;
        data.ball_dirx = -1;
        data.ball_diry = (rand() % 2 == 0 ? 1 : -1);
        draw_ball(&data);

        /* create thread for keyboard handling */
        pthread_create(
                &keyboard_handler_thread,
                NULL,
                keyboard_handler,
                &data);

        /* create thread for ai */
        pthread_create(
                &ai_handler_thread,
                NULL,
                ai_handler,
                &data);

        /* create thread for ball movement */
        pthread_create(
                &ball_handler_thread,
                NULL,
                ball_handler,
                &data);

        /* manage screen update */
        while (!data.exit_flag && data.play_flag)
        {
            read(data.pipedes[0], buf, TAG_SIZE);

            /* critical section */
            pthread_mutex_lock(&data.mut);
            if (!strcmp(buf, KBD_TAG)) /* data from keyboard */
            {
                delete_paddle(&data, KBD_TAG);
                draw_paddle(&data, KBD_TAG);
            }
            if (!strcmp(buf, AI_TAG)) /* data from ai */
            {
                delete_paddle(&data, AI_TAG);
                draw_paddle(&data, AI_TAG);
            }
            if (!strcmp(buf, BALL_TAG)) /* data from ball */
            {
                delete_ball(&data);
                draw_ball(&data);
            }
            refresh();
            pthread_mutex_unlock(&data.mut);
        }

        /* allow termination of other threads */
        data.termination_flag = 1; 

        /* print endgame message in superimpression (critical section) */
        if (!data.exit_flag)
        {
            pthread_mutex_lock(&data.mut);
            print_intra_menu(
                    stdscr,
                    (data.winner ? "GAME LOST" : "GAME WON"));
            pthread_mutex_unlock(&data.mut);
        }

    } while (!data.exit_flag);

    /* wait for remaining children threads termination 
     * (ball thread terminated itself yet) */
    pthread_join(ai_handler_thread, NULL);
    pthread_join(keyboard_handler_thread, NULL);

    endwin(); /* close ncurses window */

    restore_key_rate(); /* restore keyboard settings */

    return 0;
}
