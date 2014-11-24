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
 * \file support.h
 * 
 * \brief This file provides support routines, definitions and declarations 
 * for the game.
 * 
 * @author Martino Pilia
 * @date 2014-11-23
 */

#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#define TIME_GAP_BALL 25000 /*!< time in us between ball position updates */
#define TIME_GAP_AI 25000 /*!< time in us between ai position updates */
#define FIELD_TOP 0 /*!< top row for the playing field */
#define FIELD_BOT getmaxy(stdscr) - 1 /*!< bottom row for the playing field */
#define AI_COL 1 /*!< column for the ai paddle */
#define PADDLE_WIDTH 5 /*!< width of the paddles, must be an odd number */
#define PADDLE_TOP PADDLE_WIDTH / 2 /*!< top pos. reachable by the paddle */
#define PADDLE_BOT getmaxy(stdscr) - PADDLE_WIDTH / 2 - 1 /*!< bottom pos. */
#define PADDLE_COLOR 1 /*!< color pair identifier for player paddle */
#define BALL_COLOR 2 /*!< color pair identifier for ball */
#define AI_COLOR 3 /*!< color pair identifier for ai paddle */
#define TITLE_COLOR 4 /*!< color pair identifier for title writing */
#define KBD_TAG "k" /*!< tag describing data from keyboadrd thread */
#define AI_TAG "a" /*!< tag describing data from ai thread */
#define BALL_TAG "b" /*!< tag describing data from ball thread */
#define QUIT_TAG "q" /*!< tag describing quit request */
#define TAG_SIZE sizeof "k"  /*!< size of tags */
#define QUIT_KEY 'q' /*!< key for game termination */
#define PLAY_KEY ' ' /*!< key for game start */

/* global variables for keyboard delay and rate settings */
extern char del[4]; /*!< delay time for repetition after key press */
extern char rate[3]; /*!< rate (press/s) for a repeated key */

/*!
 * Game data shared between threads 
 */
typedef struct {
    int paddle_pos; /*!< player paddle's current vertical position */
    int paddle_col; /*!< player paddle's column */
    int ai_paddle_pos; /*!< ai paddle's current position */
    int ai_paddle_col; /*!< ai paddle's column */
    int ball_x; /*!< current ball x (column) coord */
    int ball_y; /*!< current ball y (row) coord */
    int paddle_pos_old; /*!< player paddle's last vertical position */
    int ai_paddle_pos_old; /*!< ai paddle's last vertical position */
    int ball_x_old; /*!< last ball x coord */
    int ball_y_old; /*!< last ball y coord */
    int exit_flag; /*!< allow game termination */
    int play_flag; /*!< allow game prosecution */
    int ball_dirx; /*!< current ball x speed component */
    int ball_diry; /*!< current ball y speed component */
    int pipedes[2]; /*!< pipe file descriptors */
    pthread_mutex_t mut; /*!< mutex for ncurses actions */
    int termination_flag; /*!< request child threads termination */
    int winner; /*!< 0 for player, 1 for ai */
} game_data;


/*!
 * \brief Manage window resize.
 *
 * @param i dummy int parameter
 */
void resize_handler(int);

/*!
 * \brief Thread function for keyboard input handling.
 *
 * The thread terminates itself when the termination_flag into game_data
 * structure is set to non-zero.
 *
 * @param d shared game_data structure
 */
void *keyboard_handler(void*);

/*!
 * \brief Thread function for ball position handling.
 *
 * The thread terminates itself when the ball reaches an invalid 
 * position 
 *
 * @param d shared game_data structure
 */
void *ball_handler(void*);

/*!
 * \brief Thread function for ball position generation.
 *
 * The thread terminates itself when the termination_flag into game_data
 * structure is set to non-zero.
 *
 * @param d shared game_data structure
 */
void *ai_handler(void*);

/*!
 * \brief Delete the paddle from the old position described in the shared
 * game_data structure.
 *
 * @param data shared game_data structure
 * @param tag tag identifier to determine whic paddle will be deleted
 */
void delete_paddle(game_data*, char *tag);

/*!
 * \brief Draw the paddle in the current position described in the shared
 * game_data structure.
 *
 * @param data shared game_data structure
 * @param tag tag identifier to determine whic paddle will be deleted
 */
void draw_paddle(game_data*, char *tag);

/*!
 * \brief Delete ball from old position described in the game_data
 * structure.
 *
 * @param data game_data structure.
 */
void delete_ball(game_data*);

/*!
 * \brief Draw ball in the current position described in the game_data
 * structure.
 *
 * @param data game_data structure.
 */
void draw_ball(game_data*);

/*!
 * \brief Restore the key settings of the system before the game start.
 */
void restore_key_rate();

/*!
 * \brief Handle termination of the program, restoring key settings and
 * closing ncurses window before exit.
 *
 * @param i dummy integer parameter
 */
void termination_handler(int);

/*!
 * \brief Print the introductive menu into the selected ncurses window.
 *
 * @param win ncurses window
 */
void print_intro_menu(WINDOW *win);

/*!
 * \brief Print menu after game end into the selected ncurses window.
 *
 * @param win ncurses window
 * @param msg message to print in the sceen
 */
void print_intra_menu(WINDOW *win, const char *msg);
