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
 * \file support.c
 * 
 * \brief This file implements support routines declared in support.h.
 * 
 * @author Martino Pilia
 * @date 2014-11-23
 */

#include "support.h"

/*!
 * This procedure is an handler for window resize.
 */
void resize_handler(int i)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); /* get terminal size */
    wresize(stdscr, ws.ws_row, ws.ws_col); /* resize ncurses window */
    /* TODO */
};

/*!
 * This procedure is a listener for keyboard input during the game.
 * When a player press a key, the input triggers the related action and a 
 * message to the game main thread is sent throug the pipe.
 */
void *keyboard_handler(void *d)
{
    game_data *data = (game_data*) d;
    while (!data->termination_flag)
    {
        int ch;
        
        /* get user input (critical section) */
        pthread_mutex_lock(&data->mut);
        ch = getch(); 
        pthread_mutex_unlock(&data->mut);

        switch (ch)
        {
            case KEY_UP:
                /* move pad up when possible */
                data->paddle_pos_old = data->paddle_pos;
                if (data->paddle_pos > PADDLE_TOP)
                    data->paddle_pos--;
                write(data->pipedes[1], KBD_TAG, TAG_SIZE);
                break;

            case KEY_DOWN:
                /* move pad down when possible */
                data->paddle_pos_old = data->paddle_pos;
                if (data->paddle_pos < PADDLE_BOT)
                    data->paddle_pos++;
                write(data->pipedes[1], KBD_TAG, TAG_SIZE);
                break;

            case PLAY_KEY:
                /* set flag to play a new game */
                data->play_flag = 1;
                break;

            case QUIT_KEY:
                /* set flag asking for game termination */
                data->exit_flag = 1;
                /* dummy write to unlock the controller thread, waiting
                 * at the other pipe end */
                write(data->pipedes[1], QUIT_TAG, TAG_SIZE);
                break;

            default: 
                break;
        }
    }
};

/*!
 * This procedure is responsible for ball movement. The ball position is 
 * updated every TIME_GAP_BALL microseconds, and then a message to the 
 * game main thread is sent throug the pipe.
 */
void *ball_handler(void *d)
{
    game_data *data = (game_data*) d;

    while (1)
    {
        /* update ball coordinates */
        data->ball_y_old = data->ball_y;
        data->ball_x_old = data->ball_x;
        data->ball_y += data->ball_diry;
        data->ball_x += data->ball_dirx;

        /* reflect ball on field top and bottom */
        if (data->ball_y < FIELD_TOP || data->ball_y > FIELD_BOT) 
        {
            data->ball_diry *= -1;
            data->ball_y += 2 * data->ball_diry;
        }

        /* reflect ball on player pad */
        if (data->ball_x == data->paddle_col)
        {
            if (abs(data->paddle_pos - data->ball_y - -data->ball_diry) 
                    <= PADDLE_WIDTH / 2)
            {
                /* ball is above the pad; consider one extra on length
                 * because the ball is moving diagonally */
                data->ball_dirx *= -1;
                data->ball_x += 2 * data->ball_dirx;
            } else {
                /* ball is out */
                data->play_flag = 0;

                /* player loses, ai wins */
                data->winner = 1;

                /* dummy write to unlock the controller waiting
                 * at the other pipe end */
                write(data->pipedes[1], QUIT_TAG, TAG_SIZE);

                /* thread termination */
                return;
            }
        }

        /* reflect ball on AI pad */
        if (data->ball_x + 1 == data->ai_paddle_col)
        {
            if (abs(data->ai_paddle_pos - data->ball_y - -data->ball_diry) 
                    <= PADDLE_WIDTH / 2)
            {
                /* ball is above the pad; consider one extra on length
                 * because the ball is moving diagonally */
                data->ball_dirx *= -1;
                data->ball_x += 2 * data->ball_dirx;
            } else {
                /* ball is out */
                data->play_flag = 0;

                /* ai loses, player wins */
                data->winner = 0;

                /* dummy write to unlock the controller waiting
                 * at the other pipe end */

                write(data->pipedes[1], QUIT_TAG, TAG_SIZE);

                /* thread termination */
                return;
            }
        }

        write(data->pipedes[1], BALL_TAG, TAG_SIZE);

        usleep(TIME_GAP_BALL);
    }
};

/*!
 * This procedure controls the ai pad. Movements are generated every 
 * TIME_GAP_AI microseconds, and then a message is sent to the game main
 * thread throug the pipe.
 */
void *ai_handler(void *d)
{
    game_data *data = (game_data*) d;
    
    while (!data->termination_flag)
    {
        int diff = data->ball_y - data->ai_paddle_pos;
        int new = data->ai_paddle_pos + diff / (diff == 0 ? 1 : abs(diff));

        data->ai_paddle_pos_old = data->ai_paddle_pos;

        if (new >= PADDLE_TOP && new <= PADDLE_BOT)
            data->ai_paddle_pos = new;

        write(data->pipedes[1], AI_TAG, TAG_SIZE);
        
        usleep(TIME_GAP_AI);
    }
};

/*!
 * This procedure cancels the pad from the previous position according
 * to the shared game_data structure. The second parameter permits to 
 * choose which pad (ai or player) will be deleted.
 */
void delete_paddle(game_data *data, char *tag)
{
    int i;
    int type = !strcmp(tag, KBD_TAG); /* 1 for player, 0 for ai */
    int row = (type ? data->paddle_pos_old : data->ai_paddle_pos_old)
        - PADDLE_WIDTH / 2; /* base row */

    /* delete all points from base row for all the paddle length */
    for (i = 0; i < PADDLE_WIDTH ; ++i)
        mvaddch(
               row + i,
               type ? data->paddle_col : data->ai_paddle_col,
               ' ');
};

/*!
 * This procedure draws the paddle in the current position provided by 
 * the shared game_data structure. The second parameter determines which pad 
 * will be drawn.
 */
void draw_paddle(game_data *data, char *tag)
{
    int i;
    int type = !strcmp(tag, KBD_TAG); /* 1 for player, 0 for ai */
    int row = (type ? data->paddle_pos : data->ai_paddle_pos) 
        - PADDLE_WIDTH / 2; /* base row */

    /* delete all points from base row for all the paddle length */
    for (i = 0; i < PADDLE_WIDTH ; ++i)
    {
        attron(COLOR_PAIR(type ? PADDLE_COLOR : AI_COLOR));
        mvaddch(
                row + i,
                type ? data->paddle_col : data->ai_paddle_col,
                ' ');
        attroff(COLOR_PAIR(type ? PADDLE_COLOR : AI_COLOR));
    }
};

void delete_ball(game_data *data)
{
    mvaddch(data->ball_y_old, data->ball_x_old, ' ');  
};

void draw_ball(game_data *data)
{
    attron(COLOR_PAIR(BALL_COLOR));
    mvaddch(data->ball_y, data->ball_x, 'o');
    attroff(COLOR_PAIR(BALL_COLOR));
};

/*!
 * This procedure restores the system keyboard settings as they were 
 * before the game start.
 */
void restore_key_rate()
{
    char command[25]; /* string to hold system commands */
    sprintf(command, "xset r rate %s %s", del, rate);
    system(command);
};

/*!
 * This procedure permits to handle signals for program kill or termination,
 * ensuring the keyboard system settings are restored and the ncurses
 * window is terminated before the program exit.
 */
void termination_handler(int i)
{
    restore_key_rate();
    endwin();
    exit(1);
};

void print_intro_menu(WINDOW *win)
{
    /* print in the center of the window */
    int y = getmaxy(win) / 2;
    int x = getmaxx(win) / 2;
    const char *msg = "PONG";
    const char *msg2 = "use up and down arrow keys to control the pad";
    const char *msg3 = "press space to start, q to quit";

    attron(COLOR_PAIR(TITLE_COLOR));
    mvwaddstr(
            win,
            y,
            x - strlen(msg) / 2,
            msg);
    y++; /* newline */
    mvwaddstr(
            win,
            y,
            x - strlen(msg2) / 2,
            msg2);
    y++; /* newline */
    mvwaddstr(
            win,
            y,
            x - strlen(msg3) / 2,
            msg3);
    attroff(COLOR_PAIR(TITLE_COLOR));

    refresh();
};

void print_intra_menu(WINDOW *win, const char *msg)
{
    /* print in the center of the window */
    int x = getmaxx(win) / 2;
    int y = getmaxy(win) / 2;
    const char *msg2 = "press space to restart, q to quit";
    attron(COLOR_PAIR(TITLE_COLOR));
    mvwaddstr(
            win,
            y,
            x - strlen(msg) / 2,
            msg);
    y++; /* newline */
    mvwaddstr(
            win,
            y,
            x - strlen(msg2) / 2,
            msg2);
    attroff(COLOR_PAIR(TITLE_COLOR));
};
