
#include<curses.h>
#include<string>
#include<vector>
#include "lurk_structs.h"
#include<stdexcept>
#include<map>

extern WINDOW *main_screen_border;
extern WINDOW *room_window_border;
extern WINDOW *input_bar_border;
extern WINDOW *room_window;
extern WINDOW *input_bar; 
extern WINDOW *main_screen;
extern WINDOW *chat_border;
extern WINDOW *chat_box;
extern WINDOW *player_window;
extern WINDOW *player_window_border;
extern WINDOW *status_bar_border;
extern WINDOW *status_bar;

extern character player; // keep track of the user's player information
extern pthread_mutex_t player_mutex;
extern std::map<int, std::string> room_list; // list of room connections
extern pthread_mutex_t room_mutex;
extern std::map<std::string, character*> character_map; // list of players in the current room
extern pthread_mutex_t character_map_mutex;

void refresh_main_pad(int amount_to_scroll = 0, bool resized = false);
void refresh_chat_pad(int amount_to_scroll = 0, bool resized = false);

void set_up_curses() {
    initscr(); // start curses
    start_color(); // allow the use of colors
    halfdelay(1); // enter halfdelay mode, which will made the main wgetch() loop non-blocking, and return ERR every .1 seconds. When ERR is returned, the program refreshes most of the windows to show anything the server may have sent
    
    init_color(COLOR_CYAN, 100, 100, 100); // initialize COLOR_CYAN as a gray color

    // initialize color pairs
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_CYAN);
    init_pair(6, COLOR_YELLOW, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_CYAN);
    init_pair(8, COLOR_WHITE, COLOR_CYAN);
    init_pair(9, COLOR_WHITE, COLOR_BLACK);

    int starty = 0; // where the window will start on the y axis
    int startx = 0; // where the window will start on the x axis
    int height = LINES - LINES / 5; // the height of the window
    int width = COLS - COLS / 4; // the width of the window

    starty = 0;
    startx = 0;
    height = LINES - LINES / 5;
    width = COLS - COLS / 3;
    main_screen_border = newwin(height, width, starty, startx); // create the border for the main window
    wattron(main_screen_border, COLOR_PAIR(2) | A_BOLD);
    box(main_screen_border, 0, 0);
    wattroff(main_screen_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(main_screen_border);
    main_screen = newpad(500, width-4);
    wmove(main_screen, 500 - height + 1, 0); // move the cursor to the correct location
    scrollok(main_screen, TRUE);

    starty = height;
    // startx stays the same
    height = LINES - height;
    width -= COLS / 5;
    input_bar_border = newwin(height, width, starty, startx);
    wattron(input_bar_border, COLOR_PAIR(2) | A_BOLD);
    box(input_bar_border, 0, 0);
    wattroff(input_bar_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(input_bar_border);
    input_bar = newwin(height - 2, width - 4, starty + 1, startx + 2);
    keypad(input_bar, TRUE);
    wrefresh(input_bar);

    starty = LINES / 2; // half the window height
    startx = COLS - COLS / 3;
    height = LINES / 4 - 3;
    width = COLS - startx;
    room_window_border = newwin(height, width, starty, startx);
    wattron(room_window_border, COLOR_PAIR(2) | A_BOLD);
    box(room_window_border, 0, 0);
    wattroff(room_window_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(room_window_border);
    room_window = newwin(height - 2, width - 4, starty + 1, startx + 2);
    int x = getmaxx(room_window);
    if (x > 16) { // if "CONNECTED ROOMS" can fit on one line
        wmove(room_window, 0, x / 2 - 8);
        wattron(room_window, COLOR_PAIR(2) | A_BOLD);
        waddstr(room_window, "CONNECTED ROOMS");
        wattroff(room_window, COLOR_PAIR(2) | A_BOLD);
    }
    wrefresh(room_window);


    starty = LINES / 2 + LINES / 4 - 3; // 3/4 the window height
    startx = COLS - COLS / 3;
    // do some calculations to determine the height of the player window
    height = LINES % 4 == 0 ? LINES / 4 : LINES % 4 == 1 ? LINES / 4 + 1 : LINES % 4 == 2 ? LINES / 4 + 1 : LINES / 4 + 2;
    height += 3;
    width = COLS - startx;
    player_window_border = newwin(height, width, starty, startx);
    wattron(player_window_border, COLOR_PAIR(2) | A_BOLD);
    box(player_window_border, 0, 0);
    wattroff(player_window_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(player_window_border);
    player_window = newwin(height - 2, width - 4, starty + 1, startx + 2);
    x = getmaxx(player_window);
    if (x > 19) { // if "PLAYERS / MONSTERS" can fit on one line
        wmove(player_window, 0, x / 2 - 9);
        wattron(player_window, COLOR_PAIR(2) | A_BOLD);
        waddstr(player_window, "PLAYERS / MONSTERS");
        wattroff(player_window, COLOR_PAIR(2) | A_BOLD);
    }
    wrefresh(player_window);

    starty = getbegy(input_bar_border);
    startx = getmaxx(input_bar_border);
    height = LINES - (LINES - LINES / 5);
    width = getbegx(player_window_border) - startx;
    status_bar_border = newwin(height, width, starty, startx);
    wattron(status_bar_border, COLOR_PAIR(2) | A_BOLD);
    box(status_bar_border, 0, 0);
    wattroff(status_bar_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(status_bar_border);
    status_bar = newwin(height - 2, width - 4, starty + 1, startx + 2);
    x = getmaxx(status_bar);
    if (x > 5) {
        wmove(status_bar, 0, x / 2 - 3);
        wattron(status_bar, COLOR_PAIR(2) | A_BOLD);
        waddstr(status_bar, "STATUS");
        wattroff(status_bar, COLOR_PAIR(2) | A_BOLD);
    }
    wrefresh(status_bar);


    starty = 0;
    startx = COLS - COLS / 3;
    height = LINES / 2;
    width = COLS - startx;
    chat_border = newwin(height, width, starty, startx);
    wattron(chat_border, COLOR_PAIR(2) | A_BOLD);
    box(chat_border, 0, 0);
    wattroff(chat_border, COLOR_PAIR(2) | A_BOLD);
    x = getmaxx(chat_border);
    wmove(chat_border, 1, x / 2 - 4);
    wattron(chat_border, COLOR_PAIR(2) | A_BOLD);
    waddstr(chat_border, "MESSAGES");
    wattroff(chat_border, COLOR_PAIR(2) | A_BOLD);
    wrefresh(chat_border);
    chat_box = newpad(200, width - 4);
    mvwin(chat_box, starty + 2, startx + 2);
    wmove(chat_box, 200 - LINES / 2 + 2, 0);
    scrollok(chat_box, TRUE);

    curs_set(0); // set the cursor to be invisible. May not work in all terminals
}

void refresh_main_pad(int amount_to_scroll, bool resized) {
    static int pad_starty = 1; // line the pad will start on
    static int pad_startx = 2; // column the pad will start on
    static int pad_end_y; // line the pad will end on
    static int pad_end_x; // column the pad will end on
    static int cur_pad_y = 500 - (LINES - LINES / 5 - 1), cur_pad_x = 0; // keep track of what section of the main output pad is being shown

    pad_end_y = LINES - LINES / 5 - 2;
    pad_end_x = COLS - COLS / 3 - 2;

    /*  
    if the terminal has been resized and cur_pad_y is not properly 
    offset to fill the display window, set it to the correct offset
    */
    if (resized && cur_pad_y != 500 - (LINES - LINES / 5 - 1)) 
        cur_pad_y = 500 - (LINES - LINES / 5 - 1);


    if (amount_to_scroll == -1 && cur_pad_y > 0) // if the user wishes to scroll up and the top line of the pad is not already being shown
        cur_pad_y--; // change the section of the pad that is shown in the box by -1 (scroll)

    if (amount_to_scroll == 1 && cur_pad_y <= 500 - (LINES - LINES / 5)) // if the user wishes to scroll down, ensure it does not scroll too far down
        cur_pad_y++; // change the section of the pad that is shown in the box by 1 (scroll)

    prefresh(main_screen, cur_pad_y, cur_pad_x, 1, 2, pad_end_y, pad_end_x); // refresh the pad to display the correct portion of the pad within the borders of the main window
}

void refresh_chat_pad(int amount_to_scroll, bool resized) { // very similar to refresh_main_pad(), just with different calculations
    static int pad_starty = 2; // what line the pad will start on
    static int pad_startx; // what column the pad will start on
    static int pad_end_y; // what line the pad will end on
    static int pad_end_x; // what column the pad will end on
    static int cur_pad_y = 200 - LINES / 2 + 2, cur_pad_x = 0; // keep track of what section of the main output pad is being shown

    pad_startx = COLS - COLS / 3 + 2;
    pad_end_y = LINES / 2 - 2;
    pad_end_x = COLS - 2;

    /*  
    if the terminal has been resized and cur_pad_y is not properly
    offset to fill the display window, set it to the correct offset
    */
    if (resized && cur_pad_y != 200 - LINES / 2 + 2)
        cur_pad_y = 200 - LINES / 2 + 2;

    if (amount_to_scroll == -1 && cur_pad_y > 0)
        cur_pad_y--; // change the section of the pad that is shown in the box by -1

    if (amount_to_scroll == 1 && cur_pad_y < 200 - LINES / 2 + 2)
        cur_pad_y++; // change the section of the pad that is shown in the box by 1

    prefresh(chat_box, cur_pad_y, cur_pad_x, pad_starty, pad_startx, pad_end_y, pad_end_x);
}

void resize_all_windows(int color) { // called when the user resizes the terminal or the color changes

    // erase the borders to be redrawn
    wclear(main_screen_border);
    wclear(input_bar_border);
    wclear(room_window_border);
    wclear(chat_border);
    wclear(player_window_border);
    wclear(status_bar_border);

    // resize the the windows and their borders to the new sizes
    wresize(input_bar_border, LINES / 5, COLS - COLS/3 - COLS / 5);
    wresize(input_bar, LINES / 5 - 2, COLS - COLS / 3 - COLS / 5 - 4);

    wresize(room_window_border, LINES / 4 - 3, COLS / 3);
    wresize(room_window, LINES / 4 - 2 - 3, COLS / 3 - 4);

    int p_border_height = LINES % 4 == 0 ? LINES / 4 : LINES % 4 == 1 ? LINES / 4 + 1 : LINES % 4 == 2 ? LINES / 4 + 1 : LINES / 4 + 2;
    p_border_height += 3;
    wresize(player_window_border, p_border_height, COLS / 3);
    wresize(player_window, p_border_height - 2, COLS / 3 - 4);

    wresize(chat_border, LINES / 2, COLS / 3);
    wresize(chat_box, 200, COLS / 3 - 4);

    wresize(main_screen_border, LINES - LINES / 5, COLS - COLS / 3);
    wresize(main_screen, 500, COLS - COLS / 3 - 4);

    // move windows, no need to move the main_screen pad because it will always start at 0,0
    mvwin(input_bar_border, LINES - LINES / 5, 0);
    mvwin(input_bar, LINES - LINES / 5 + 1, 2);

    mvwin(room_window_border, LINES / 2, COLS - COLS / 3);
    mvwin(room_window, LINES / 2 + 1, COLS - COLS / 3 + 2);

    mvwin(player_window_border, LINES / 2 + LINES / 4 - 3, COLS - COLS / 3);
    mvwin(player_window, LINES / 2 + LINES / 4 + 1 - 3, COLS - COLS / 3 + 2);

    mvwin(chat_border, 0, COLS - COLS / 3);
    mvwin(chat_box, 3, COLS - COLS / 3 + 2);

    wresize(status_bar_border, LINES / 5, getbegx(player_window_border) - getmaxx(input_bar_border));
    wresize(status_bar, LINES / 5 - 2, getmaxx(status_bar_border) - 4);

    mvwin(status_bar_border, LINES - LINES / 5, getmaxx(input_bar_border));
    mvwin(status_bar, LINES - LINES / 5 + 1, getmaxx(input_bar_border) + 2);

    // draw the borders
    wattron(input_bar_border, COLOR_PAIR(color) | A_BOLD);
    box(input_bar_border, 0, 0);
    wattroff(input_bar_border, COLOR_PAIR(color) | A_BOLD);

    wattron(room_window_border, COLOR_PAIR(color) | A_BOLD);
    box(room_window_border, 0, 0);
    wattroff(room_window_border, COLOR_PAIR(color) | A_BOLD);

    wattron(main_screen_border, COLOR_PAIR(color) | A_BOLD);
    box(main_screen_border, 0, 0);
    wattroff(main_screen_border, COLOR_PAIR(color) | A_BOLD);

    wattron(player_window_border, COLOR_PAIR(color) | A_BOLD);
    box(player_window_border, 0, 0);
    wattroff(player_window_border, COLOR_PAIR(color) | A_BOLD);

    wattron(status_bar_border, COLOR_PAIR(color) | A_BOLD);
    box(status_bar_border, 0, 0);
    wattroff(status_bar_border, COLOR_PAIR(color) | A_BOLD);


    // recenter the "MESSAGES" text at the top of the box
    wmove(chat_border, 1, 0); // move cursor to beginning of the line
    wclrtoeol(chat_border); // clear the line
    int x = getmaxx(chat_border); // get the size of the chat box
    if (x > 8) {
        wmove(chat_border, 1, x / 2 - 4); // move the cursor to the center - 4
        wattron(chat_border, COLOR_PAIR(color) | A_BOLD);
        waddstr(chat_border, "MESSAGES"); // add "MESSAGES" to the top of the box
        wattroff(chat_border, COLOR_PAIR(color) | A_BOLD);
        wmove(chat_border, 2, 0);
    }

    wattron(chat_border, COLOR_PAIR(color) | A_BOLD);
    box(chat_border, 0, 0);
    wattroff(chat_border, COLOR_PAIR(color) | A_BOLD);
    
    // recenter "CONNECTING ROOMS"
    wmove(room_window, 0, 0); // move cursor to beginning of the line
    wclrtoeol(room_window); // clear the line
    x = getmaxx(room_window); // get the size of the chat box
    if (x > 15) { // if "CONNECTED ROOMS" can fit on one line
        wmove(room_window, 0, x / 2 - 8);
        wattron(room_window, COLOR_PAIR(color) | A_BOLD);
        waddstr(room_window, "CONNECTED ROOMS");
        wattroff(room_window, COLOR_PAIR(color) | A_BOLD);
        wmove(room_window, 2, 0);
    }

    // recenter "PLAYERS / MONSTERS"
    wmove(player_window, 0, 0); // move cursor to beginning of the line
    wclrtoeol(player_window); // clear the line
    x = getmaxx(player_window); // get the size of the chat box
    if (x > 18) { // if "PLAYERS / MONSTERS" can fit on one line
        wmove(player_window, 0, x / 2 - 9);
        wattron(player_window, COLOR_PAIR(color) | A_BOLD);
        waddstr(player_window, "PLAYERS / MONSTERS");
        wattroff(player_window, COLOR_PAIR(color) | A_BOLD);
        wmove(player_window, 2, 0);
    }

    // recenter "STATUS"
    wmove(status_bar, 0, 0);
    wclrtoeol(status_bar);
    x = getmaxx(status_bar);
    if (x > 5) {
        wmove(status_bar, 0, x / 2 - 3);
        wattron(status_bar, COLOR_PAIR(color) | A_BOLD);
        waddstr(status_bar, "STATUS");
        wattroff(status_bar, COLOR_PAIR(color) | A_BOLD);
        wmove(status_bar, 2, 0);
    }

    // refresh all the windows
    wrefresh(input_bar_border);
    wrefresh(room_window_border);
    wrefresh(main_screen_border);
    wrefresh(player_window_border);
    wrefresh(status_bar_border);
    wrefresh(chat_border);
    wrefresh(player_window);
    wrefresh(status_bar);
    refresh_chat_pad(0, true);
    wrefresh(room_window);
    wrefresh(input_bar);
    refresh_main_pad(0, true);
}

void refresh_all_windows() { // called every .1 seconds, refresh all the windows that could change if the server sends the user some information
    wrefresh(status_bar);
    wrefresh(input_bar);
    refresh_chat_pad();
    wrefresh(room_window);
    refresh_main_pad();
}

void print_command_prompt(std::string message) { // print instructions to the top of the input bar to prompt users to enter something
    int y, x;
    getyx(input_bar, y, x);
    wmove(input_bar, 0, 2);
    wclrtoeol(input_bar);
    wprintw(input_bar, message.c_str());
    wmove(input_bar, y, x);
}

void print_error(std::string error_message) { // print a bold red error message to the main screen
    wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
    wprintw(main_screen, error_message.c_str());
    wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
}

void update_connected_rooms() { // update the connected rooms window. Called every .1 seconds in the main wgetch() loop to ensure it remains up to date. Some servers may send new connected rooms at any time
    std::string room_string;
    int width = getmaxx(room_window);
    wmove(room_window, 1, 0); // move to directly below "CONNECTED ROOMS"
    wclrtobot(room_window); // clear every line below the header
    std::string tmp = "";
    int count = 0;
    int height = getmaxy(room_window);
    
    pthread_mutex_lock(&room_mutex);
    int room_count = room_list.size();
    for (auto room : room_list) {
        if (height - 1 == getcury(room_window)) {
            wmove(room_window, height - 1, 0); // move the cursor to the beginning of the line and clear it (this may cut off part of the previous line, but there has to be a tradeoff for this to work correctly)
            wclrtoeol(room_window);
            if (height > 2) count--; // stupid solution to a problem, but essentially decrement count to make the number actually correct
            tmp = "[" + std::to_string(room_list.size() - count) + " more rooms]";
            wprintw(room_window, "%s", tmp.c_str());
            break;
        }
        room_string = std::to_string(room.first) + ": " + room.second;
        if (room_count != room_list.size()) // if the room to be printed is not the first one, add the divider
            room_string = " | " + room_string;
        if ((room_count != room_list.size() && (width - getcurx(room_window) > room_string.length()) || room_count == room_list.size())) // if there's space, print on the same line
            waddstr(room_window, room_string.c_str());
        else                                                        // if there is not enough room on the line, print a newline and print the room without the divider
            wprintw(room_window, "\n%s", room_string.substr(3).c_str());
        room_count--;
        count++;
    }
    pthread_mutex_unlock(&room_mutex);
    wrefresh(room_window);
}

void update_player_list() { // update the list of players window
    char color = 0;
    int width = getmaxx(player_window);
    int height = getmaxy(player_window);
    int remaining_space; // the space remaining on a line
    std::string tmp = "";
    std::vector<std::string> monsters, players; // vectors to hold monsters and players so monsters can be printed first reliably
    wmove(player_window, 1, 0); // move the cursor to just below the header
    wclrtobot(player_window); // clear the window


    pthread_mutex_lock(&character_map_mutex);
    int count = 0;

    for (auto c : character_map) {
        if (c.second->flags & MONSTER) // if the player is a monster
            monsters.push_back(c.first + std::string(" | ") + std::string(c.second->stat_string));
        else // the player is a regular player
            players.push_back(c.first + " | " + std::string(c.second->stat_string));
    }
    pthread_mutex_unlock(&character_map_mutex);

    // print the monsters before the players
    for (std::string m : monsters) {
        if (height - 1 == getcury(player_window)) { // if the cursor is on the last line of the window, print the [x more players / monsters] text
            wmove(player_window, height - 1, 0); // move the cursor to the beginning of the line and clear it (this may cut off part of the previous line, but there has to be a tradeoff for this to work correctly)
            wclrtoeol(player_window);
            pthread_mutex_lock(&character_map_mutex);
            tmp = "[" + std::to_string(character_map.size() - count) + " more players / monsters]";
            pthread_mutex_unlock(&character_map_mutex);
            if (color == 1) { // if color is 1, print with a gray background
                wattron(player_window, COLOR_PAIR(8));
                wprintw(player_window, "%s", tmp.c_str());
                wattroff(player_window, COLOR_PAIR(8));
                getcurx(player_window) != 0 ? remaining_space = width - getcurx(player_window) : remaining_space = 0;
                tmp = "";
                for (int i = 0; i < remaining_space; i++) { // contruct a string containing [remaining_space] number of spaces to make the rest of the line gray as well
                    tmp += " ";
                }
                if (tmp != "") {
                    wattron(player_window, COLOR_PAIR(8));
                    wprintw(player_window, "%s", tmp.c_str());
                    wattroff(player_window, COLOR_PAIR(8));
                }
                tmp = "";
            } else {
                wprintw(player_window, "%s", tmp.c_str());
            }
            break;
        }
        if (color == 1) {

            //print in red with a gray background
            wattron(player_window, COLOR_PAIR(5) | A_BOLD);
            wprintw(player_window, "%s", m.c_str());
            wattroff(player_window, COLOR_PAIR(5) | A_BOLD);

            getcurx(player_window) != 0 ? remaining_space = width - getcurx(player_window) : remaining_space = 0;

            for (int i = 0; i < remaining_space; i++) {
                tmp += " ";
            }
            if (tmp != "") {
                wattron(player_window, COLOR_PAIR(8));
                wprintw(player_window, "%s", tmp.c_str());
                wattroff(player_window, COLOR_PAIR(8));
            }
            color = 0;
            tmp = "";
        } else {
            wattron(player_window, COLOR_PAIR(3) | A_BOLD);  // print it in bold red with regular background
            wprintw(player_window, "%s", m.c_str());
            wattroff(player_window, COLOR_PAIR(3) | A_BOLD);
            if (getcurx(player_window) != 0)
                waddstr(player_window, "\n");
            color = 1;
        }
        count++;
    }
    // print the players after the monsters, copy paste a lot of code but whatever
    for (std::string p : players) {
        if (height - 1 == getcury(player_window)) {
            wmove(player_window, height - 1, 0); // move the cursor to the beginning of the line and clear it (this may cut off part of the previous line, but there has to be a tradeoff for this to work correctly)
            wclrtoeol(player_window);
            pthread_mutex_lock(&character_map_mutex);
            tmp = "[" + std::to_string(character_map.size() - count) + " more players / monsters]";
            pthread_mutex_unlock(&character_map_mutex);
            if (color == 1) { // if color is 1, print with a gray background
                wattron(player_window, COLOR_PAIR(8));
                wprintw(player_window, "%s", tmp.c_str());
                wattroff(player_window, COLOR_PAIR(8));
                getcurx(player_window) != 0 ? remaining_space = width - getcurx(player_window) : remaining_space = 0;
                tmp = "";
                for (int i = 0; i < remaining_space; i++) { // contruct a string containing [remaining_space] number of spaces to make the rest of the line gray as well
                    tmp += " ";
                }
                if (tmp != "") {
                    wattron(player_window, COLOR_PAIR(8));
                    wprintw(player_window, "%s", tmp.c_str());
                    wattroff(player_window, COLOR_PAIR(8));
                }
                tmp = "";
            } else {
                wprintw(player_window, "%s", tmp.c_str());
            }
            break;
        }
        if (color == 1) {

            //print with a gray background
            wattron(player_window, COLOR_PAIR(8));
            wprintw(player_window, "%s", p.c_str());
            wattroff(player_window, COLOR_PAIR(8));

            getcurx(player_window) != 0 ? remaining_space = width - getcurx(player_window) : remaining_space = 0;

            for (int i = 0; i < remaining_space; i++) {
                tmp += " ";
            }
            if (tmp != "") {
                wattron(player_window, COLOR_PAIR(8));
                wprintw(player_window, "%s", tmp.c_str());
                wattroff(player_window, COLOR_PAIR(8));
            }
            color = 0;
            tmp = "";
        } else { // print with the regular black background
            wprintw(player_window, "%s", p.c_str());
            if (getcurx(player_window) != 0)
                waddstr(player_window, "\n");
            color = 1;
        }
        count++;
    }
    wrefresh(player_window);
}

void clear_prompt_area() { // clear where the user inputs
    wmove(input_bar, 1, 0); // move the cursor to just below the command prompt
    wclrtobot(input_bar); // clear everything below the command prompt line
    wmove(input_bar, getmaxy(input_bar) - 2, 0); // move the cursor to the correct place
    waddstr(input_bar, "$ ");
    wrefresh(input_bar); // refresh the input bar
}

void update_status_bar() { // update the status bar
    wmove(status_bar, 1, 0);
    wclrtobot(status_bar); // clear to the bottom of the window
    int width = getmaxx(status_bar);
    pthread_mutex_lock(&character_map_mutex);
    pthread_mutex_lock(&player_mutex);
    std::vector<std::string> stats;
    try { // try to display the user's name. If they have not made a character yet, player.name will be uninitialized and std::logic_error will be thrown. In that case, simply print "N/A" in place of the name.
        stats = {std::string(player.name), std::string(" | ") + SWORDS + " ATK: " + std::to_string(player.attack), std::string(" | ") + SHIELD + " DEF: " + std::to_string(player.defense), std::string(" | ") + REGEN + " REG: " + std::to_string(player.regen), std::string(" | ") + HEART + " HP: " + std::to_string(player.health), " | Gold: $" + std::to_string(player.gold), " | Cur Rm: " + std::to_string(player.room_num)};
    } catch (std::logic_error) {
        stats = {std::string("N/A"), std::string(" | ") + SWORDS + std::string("ATK: ") + std::to_string(player.attack), std::string(" | ") + SHIELD + " DEF: " + std::to_string(player.defense), std::string(" | ") + REGEN + " REG: " + std::to_string(player.regen), std::string(" | ") + HEART + " HP: " + std::to_string(player.health), " | Gold: $" + std::to_string(player.gold), " | Cur Rm: " + std::to_string(player.room_num)};
    }
    pthread_mutex_unlock(&player_mutex);
    pthread_mutex_unlock(&character_map_mutex);

    for (int i = 0; i < stats.size() - 2; i++) { // for each stat in the vector
        if (i == 0) { // when i == 0, the name will be displayed. Make it bold magenta
            wattron(status_bar, COLOR_PAIR(4) | A_BOLD);
            waddstr(status_bar, stats[i].c_str());
            wattroff(status_bar, COLOR_PAIR(4) | A_BOLD);
            continue;
        }
        if (width - getcurx(status_bar) > stats[i].length() - 3 && getcurx(status_bar) != 0) // if there is room on the line and the cursor is not at the beginning of a line, print the stat with a vertical bar before it
            waddstr(status_bar, stats[i].c_str());
        else
            if (getcurx(status_bar) == 0) // if the cursor is at the beginning of a new line, print the stat without a vertical bar divider
                wprintw(status_bar, "%s", stats[i].substr(3).c_str());
            else
                wprintw(status_bar, "\n%s", stats[i].substr(3).c_str()); // else print a new line and the stat without a vertical bar divider
    }
    for (int i = stats.size() - 2; i < stats.size(); i++) { // this is stupid but use a separate for loop for the last two elements because they dont have unicode characters that mess with the actual length of the string
        if (width - getcurx(status_bar) > stats[i].length() && getcurx(status_bar) != 0)
            waddstr(status_bar, stats[i].c_str());
        else
            if (getcurx(status_bar) == 0)
                wprintw(status_bar, "%s", stats[i].substr(3).c_str());
            else
                wprintw(status_bar, "\n%s", stats[i].substr(3).c_str());
    }
}

void print_to_chat(char* sender, char* msg) {
    static char chat_color = 1;
    std::string tmp = "";
    if (chat_color == 0) {
        wattron(chat_box, COLOR_PAIR(7) | A_BOLD);
        wprintw(chat_box, "%s: ", sender);
        wattroff(chat_box, COLOR_PAIR(7) | A_BOLD);
        wattron(chat_box, COLOR_PAIR(8));
        wprintw(chat_box, "%s", msg); // print the message in the chat box
        int remaining_space = getmaxx(chat_box) - getcurx(chat_box);
        for (int i = 0; i < remaining_space; i++) {
            tmp += " ";
        }
        wprintw(chat_box, "%s", tmp.c_str()); // print the blank spaces
        tmp = "";
        wattroff(chat_box, COLOR_PAIR(8));
        chat_color = 1;
    } else {
        wattron(chat_box, COLOR_PAIR(4) | A_BOLD);
        wprintw(chat_box, "%s: ", sender);
        wattroff(chat_box, COLOR_PAIR(4) | A_BOLD);
        wprintw(chat_box, "%s\n", msg); // print the message in the chat box
        chat_color = 0;
    }
}