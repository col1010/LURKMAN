// Created by Coleby Kauffman | Spring 2022 | CS-435 Networks
// Run like this:  ./lurkman address port
// packages needed (debian): libncurses5-dev, libncursesw5-dev, libncurses-dev, libgraph-easy-perl, libgraphviz-dev
// build like this: g++ lurk_client.cpp -o lurkman -O3 -lncursesw -pthread -lgvc -lcgraph

#include<sys/socket.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<netinet/ip.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<pthread.h>
#include<curses.h>
#include<string.h>
#include<string>
#include<signal.h>
#include<errno.h>
#include<locale.h>
#include<poll.h>
#include<fstream>
#include<iostream>
#include<vector>
#include<algorithm>
#include<map>
#include<set>
#include<atomic>
#include "graphviz_local/gvc.h" // the normal graphviz in /usr/include has a typedef box, which interferes with the box() function of curses. I have edited the geom.h header to not include the box struct
#include "lurk_structs.h"
#include "receive_from_server.h"
#include "send_to_server.h"
#include "curses_control.h"
#include "handle_receive.h"
#include "helper_funcs.h"

const std::vector<std::string> command_array = {"!make", "!start", "!qmake", "!move", "!back", "!fight", "!pvp", "!leave", "!msgn", "!msg", "!playerlist", "!monsterlist", "!alive", "!dead", "!roomlist", "!about", "!loot", "!color", "!map", "!bottom", "!cbottom", "!help", "!sus"}; // list of commands available for the user to input

// function prototypes
void* listen_to_server(void* arg);
void process_user_input(std::string user_input);

int skt;
uint16_t initial_stats = 0;
// global so all functions can access the windows
WINDOW * main_screen_border;
WINDOW * room_window_border;
WINDOW * input_bar_border;
WINDOW *room_window;
WINDOW *input_bar; 
WINDOW *main_screen;
WINDOW *chat_border;
WINDOW *chat_box;
WINDOW *player_window;
WINDOW *player_window_border;
WINDOW *status_bar_border;
WINDOW *status_bar;

// create global variables and their various mutexes if needed

character player; // keep track of the user's player information
pthread_mutex_t player_mutex;

std::map<int, std::string> room_list; // list of room connections
pthread_mutex_t room_mutex;

std::map<std::string, character*> character_map; // list of players in the current room
pthread_mutex_t character_map_mutex;

int color = 2; // color of the borders and headers. Changed either by the user or automatically when the player dies
pthread_mutex_t color_mutex;

int user_chosen_color = 2; // color the user chooses. Used to cache their color preference in case they die and can be revived automatically by the server
pthread_mutex_t user_chosen_color_mutex;

uint16_t prev_room; // the room the user was previously in, used for !back command
pthread_mutex_t prev_room_mutex;

std::string user_input; // current user input

std::set<std::string> visited_rooms; // set containing visited rooms, used when determining how to graph the map of the server
pthread_mutex_t graph_mutex;

bool made_character = false;
pthread_mutex_t made_character_mutex; // kind of pointless but potentially a race condition

bool map_enabled;

std::atomic<bool> exit_thread {false};
pthread_t thread;

Agraph_t* G;
GVC_t* gvc;
char username[20]; // store the system username of the user, for creating user-unique temporary map files

int main(int argc, char ** argv){

    setlocale(LC_ALL, ""); // add unicode support, doesnt work in all terminals but whatever
    G = agopen((char*)"map", Agstrictundirected, 0);
    agattr(G, AGRAPH, (char*) "rankdir", (char*) "TB");
    agset(G, (char*) "rankdir", (char*) "TB");
    gvc = gvContext();

    if (getlogin_r(username, 20) != 0) { // get the user's system username
        // returns nonzero on error, assign a default username if this occurs
        strcpy(username, "default");
    } 

	if(argc < 3){
		printf("Usage:  %s hostname port\n", argv[0]);
		return 1;
	}

	struct sockaddr_in sad;
	sad.sin_port = htons(atoi(argv[2]));
	sad.sin_family = AF_INET;

	skt = socket(AF_INET, SOCK_STREAM, 0);

	struct hostent* entry = gethostbyname(argv[1]);
	if(!entry){
		if(h_errno == HOST_NOT_FOUND){
			printf("Host not found\n");
		}
		herror("gethostbyname");
		return 1;
	}

	struct in_addr **addr_list = (struct in_addr**)entry->h_addr_list;
	struct in_addr* c_addr = addr_list[0];
	char* ip_string = inet_ntoa(*c_addr);
	sad.sin_addr = *c_addr;
	//printf("Connecting to:  %s\n", ip_string);

	if( connect(skt, (struct sockaddr*)&sad, sizeof(struct sockaddr_in)) ){
		perror("connect");
		return 1;
	}
    
    struct sigaction close_action;
    close_action.sa_handler = exit_gracefully;
    if(sigaction(SIGINT, &close_action, 0)){
        return 1;
    }
    
    set_up_curses(); // start the curses interface

    // print a welcome message

    wattron(main_screen, COLOR_PAIR(6));
    if (getmaxx(main_screen) >= 165)
        wprintw(main_screen, "          _____            _____                    _____                    _____                    _____                    _____                    _____          \n         /\\    \\          /\\    \\                  /\\    \\                  /\\    \\                  /\\    \\                  /\\    \\                  /\\    \\         \n        /  \\____\\        /  \\____\\                /  \\    \\                /  \\____\\                /  \\____\\                /  \\    \\                /  \\____\\        \n       /   /    /       /   /    /               /    \\    \\              /   /    /               /    |   |               /    \\    \\              /    |   |        \n      /   /    /       /   /    /               /      \\    \\            /   /    /               /     |   |              /      \\    \\            /     |   |        \n     /   /    /       /   /    /               /   /\\   \\    \\          /   /    /               /      |   |             /   /\\   \\    \\          /      |   |        \n    /   /    /       /   /    /               /   /__\\   \\    \\        /   /____/               /   /|  |   |            /   /__\\   \\    \\        /   /|  |   |        \n   /   /    /       /   /    /               /    \\   \\   \\    \\      /    \\    \\              /   / |  |   |           /    \\   \\   \\    \\      /   / |  |   |        \n  /   /    /       /   /    /      _____    /      \\   \\   \\    \\    /      \\____\\________    /   /  |  |___|______    /      \\   \\   \\    \\    /   /  |  |   | _____  \n /   /    /       /   /____/      /\\    \\  /   /\\   \\   \\   \\____\\  /   /\\           \\    \\  /   /   |        \\    \\  /   /\\   \\   \\   \\    \\  /   /   |  |   |/\\    \\ \n/   /____/       |   |    /      /  \\____\\/   /  \\   \\   \\   |    |/   /  |           \\____\\/   /    |         \\____\\/   /  \\   \\   \\   \\____\\/   /    |  |   /  \\____\\\n\\   \\    \\       |   |____\\     /   /    /\\  /   |    \\  /   |____|\\  /   |  |~~~|~~~~~     \\  /    / ~~~~~/   /    /\\  /    \\   \\  /   /    /\\  /    /|  |  /   /    /\n \\   \\    \\       \\   \\    \\   /   /    /  \\/____|     \\/   /    /  \\/____|  |   |           \\/____/      /   /    /  \\/____/ \\   \\/   /    /  \\/____/ |  | /   /    / \n  \\   \\    \\       \\   \\    \\ /   /    /         |         /    /         |  |   |                       /   /    /            \\      /    /           |  |/   /    /  \n   \\   \\    \\       \\   \\    /   /    /          |  |\\    /    /          |  |   |                      /   /    /              \\    /    /            |      /    /   \n    \\   \\    \\       \\   \\__/   /    /           |  | \\  /____/           |  |   |                     /   /    /               /   /    /             |     /    /    \n     \\   \\    \\       \\        /    /            |  |  ~|                 |  |   |                    /   /    /               /   /    /              |    /    /     \n      \\   \\    \\       \\      /    /             |  |   |                 |  |   |                   /   /    /               /   /    /               /   /    /      \n       \\   \\____\\       \\    /    /              \\  |   |                 \\  |   |                  /   /    /               /   /    /               /   /    /       \n        \\  /    /        \\  /____/                \\ |   |                  \\ |   |                  \\  /    /                \\  /    /                \\  /    /        \n         \\/____/          ~~                       \\|___|                   \\|___|                   \\/____/                  \\/____/                  \\/____/         \n");
    else if (getmaxx(main_screen) >= 95)
        wprintw(main_screen, "      ___       ___           ___           ___           ___           ___           ___     \n     /\\__\\     /\\__\\         /\\  \\         /\\__\\         /\\__\\         /\\  \\         /\\__\\    \n    / /  /    / /  /        /  \\  \\       / /  /        /  |  |       /  \\  \\       /  |  |   \n   / /  /    / /  /        / /\\ \\  \\     / /__/        / | |  |      / /\\ \\  \\     / | |  |   \n  / /  /    / /  /  ___   /  \\~\\ \\  \\   /  \\__\\____   / /| |__|__   /  \\~\\ \\  \\   / /| |  |__ \n / /__/    / /__/  /\\__\\ / /\\ \\ \\ \\__\\ / /\\     \\__\\ / / |    \\__\\ / /\\ \\ \\ \\__\\ / / | | /\\__\\\n \\ \\  \\    \\ \\  \\ / /  / \\/_|  \\/ /  / \\/_| |~~|~    \\/__/~~/ /  / \\/__\\ \\/ /  / \\/__| |/ /  /\n  \\ \\  \\    \\ \\  / /  /     | |  /  /     | |  |           / /  /       \\  /  /      | / /  / \n   \\ \\  \\    \\ \\/ /  /      | |\\/__/      | |  |          / /  /        / /  /       |  /  /  \n    \\ \\__\\    \\  /  /       | |  |        | |  |         / /  /        / /  /        / /  /   \n     \\/__/     \\/__/         \\|__|         \\|__|         \\/__/         \\/__/         \\/__/    \n\n");
    else if (getmaxx(main_screen) >= 71)
        wprintw(main_screen, "    ___       ___       ___       ___       ___       ___       ___   \n   /\\__\\     /\\__\\     /\\  \\     /\\__\\     /\\__\\     /\\  \\     /\\__\\  \n  / /  /    / / _/_   /  \\  \\   / / _/_   /  L_L_   /  \\  \\   / | _|_ \n / /__/    / /_/\\__\\ /  \\ \\__\\ /  -\"\\__\\ / /L \\__\\ /  \\ \\__\\ /  |/\\__\\\n \\ \\  \\    \\ \\/ /  / \\;   /  / \\; ;-\",-\" \\/_/ /  / \\/\\  /  / \\/|  /  /\n  \\ \\__\\    \\  /  /   | \\/__/   | |  |     / /  /    / /  /    | /  / \n   \\/__/     \\/__/     \\|__|     \\|__|     \\/__/     \\/__/     \\/__/  \n");
    wprintw(main_screen, "LURKMAN client created by Coleby Kauffman 2022\n\n");
    wprintw(main_screen, "Note: Program tested primarily with Windows Terminal and the Mac OS terminal. Things may look a little weird in other terminals. Unicode characters depicting swords, a shield, sparkles, and a heart may not show correctly, for example. Resizing the terminal is pretty well supported, so feel free to resize if needed.\n\n");
    wattroff(main_screen, COLOR_PAIR(6));
    wattron(main_screen, COLOR_PAIR(1) | A_BLINK | A_BOLD);
    wprintw(main_screen, "Connection to %s:%s successful!\n\n", argv[1], argv[2]);
    wattroff(main_screen, COLOR_PAIR(1) | A_BLINK | A_BOLD);

    if (access("/usr/bin/graph-easy", X_OK) == 0) { // check for the graph-easy binary
        map_enabled = true;
    } else {
        print_error("Warning: Graph-Easy not detected in /usr/bin or permissions were denied. !map functionality has been disabled.\n\n");
        map_enabled = false;
    }
    
    pthread_create(&thread, 0, listen_to_server, 0); // start the server listen thread

    // user stuff
    int ch;
    wmove(input_bar, getmaxy(input_bar) -2, 0);
    waddstr(input_bar, "$ ");
    wrefresh(input_bar);
    bool color_changed = false;
    int cur_color = color; // default, blue borders
    std::vector<std::string> cmd_hist;
    int cmd_pos = 0; // keep track of where the user is in the command history
    int prev_cur_pos = 2;

    while (ch = wgetch(input_bar)) { // wait for user input, returns ERR every .1 seconds if the user does not type something

        if (ch == KEY_UP) {
            refresh_main_pad(-1, false); // refresh the main pad, scroll up a line
        } else if (ch == KEY_DOWN) {
            refresh_main_pad(1, false); // refresh the main pad, scroll down a line
        } else if (ch == KEY_LEFT) {
            if (cmd_pos == 0) continue; // do nothing if there is no command history or the user has gotten to the beginning of it
            cmd_pos--; // decrement the position
            wmove(input_bar, getmaxy(input_bar) -2, 2); // clear the prompt line
            wclrtoeol(input_bar);
            waddstr(input_bar, cmd_hist[cmd_pos].c_str()); // add the command in the history to the window
            user_input = cmd_hist[cmd_pos]; // set the user input to the old command

        } else if (ch == KEY_RIGHT) {
            
            if (cmd_pos >= cmd_hist.size() - 1) { // if the user is at the end of the command history, clear the line and reset user_input
                wmove(input_bar, getmaxy(input_bar) -2, 2); 
                wclrtoeol(input_bar); // clear the prompt line
                user_input = "";
                cmd_pos = cmd_hist.size();
                continue; 
            } 
            cmd_pos++; // increment the position
            wmove(input_bar, getmaxy(input_bar) -2, 2); // clear the prompt line
            wclrtoeol(input_bar);
            waddstr(input_bar, cmd_hist[cmd_pos].c_str()); // add the command in the history to the window
            user_input = cmd_hist[cmd_pos]; // set the user input to the old command
            
        } else if (ch == CTRL('u')) {  // scroll the chat up by 1 line
            wmove(input_bar, getmaxy(input_bar) -2, getcurx(input_bar) - 2);
            wclrtoeol(input_bar);
            refresh_chat_pad(-1, false);
        } else if (ch == CTRL('d')) { // scroll the chat down by 1 line
            wmove(input_bar, getmaxy(input_bar) -2, getcurx(input_bar) - 2);
            wclrtoeol(input_bar);
            refresh_chat_pad(1, false);
        } else if (ch == KEY_RESIZE) { // terminal was resized
            resize_term(0, 0);
            //wprintw(main_screen, "Terminal resized!\nNew dimensions: %d rows, %d cols\n\n", LINES, COLS);
            resize_all_windows(color);
            clear_prompt_area();
            user_input = ""; // clear the user input
            wrefresh(input_bar);
        } else if (ch == '\n') {
            if (user_input == "") {
                wmove(input_bar, getmaxy(input_bar) -2, 2); // move the cursor back to the correct place (after the $)
                continue;
            }
            clear_prompt_area();
            wmove(input_bar, getmaxy(input_bar) -3, 2); // move the cursor to just above the prompt line
            wprintw(input_bar, "%s", user_input.c_str()); // print what the user just entered
            wmove(input_bar, getmaxy(input_bar) -2, 2); // move the cursor back to the correct place (after the $)
            process_user_input(user_input);
            if (std::find(command_array.begin(), command_array.end(), user_input) != command_array.end()) { // if the user entered a valid command
                if (cmd_hist.size() > 50) cmd_hist.erase(cmd_hist.begin()); // if the command history has reached size 50, start deleting the first element to maintain a consistent size
                if (cmd_hist.empty()) { // if the command history is empty, add the command to the vector
                    cmd_hist.push_back(user_input);

                } else if (cmd_hist[cmd_hist.size()-1] != user_input) { // if it is not empty and the previous entry was not the same as what was just entered
                    cmd_hist.push_back(user_input); // add the input to the command history vector
                }
            }
            cmd_pos = cmd_hist.size(); // reset cmd_pos to the end of the command history
            user_input = ""; // clear the user input
            prev_cur_pos = 2; // reset the previous cursor position

        } else if(ch == KEY_BACKSPACE) {
            int y, x;
            getyx(input_bar, y, x);
            if (x == 1)
                wmove(input_bar, getmaxy(input_bar) -2, 2);
            else if (user_input == "" && x > 2) {
                wmove(input_bar, getmaxy(input_bar) -2, 2);
                wclrtoeol(input_bar);
            } else if (x > 1) {
                user_input.pop_back();
                wdelch(input_bar);
                wrefresh(input_bar);
            }
            
        } else if (ch == ERR) { // if no character has been typed for .1 seconds, refresh everything (halfdelay mode makes wgetch() return ERR every n tenths of a second)
            pthread_mutex_lock(&color_mutex);
            if (color != cur_color) { // if the color has changed (either by the user or automatically) redraw the window borders as the new color
                resize_all_windows(color); // "resize" the windows (really just to redraw the borders and text as a new color)
                cur_color = color;
            }
            pthread_mutex_unlock(&color_mutex);
            update_status_bar();
            refresh_all_windows();
            update_connected_rooms();
            update_player_list();
        } else { // the user pressed anything else
            
            if (isalnum(ch) || isgraph(ch) || ch == ' ') { // if the character is alpha numeric or certain symbols
                user_input += ch; // add the character input to the string
                prev_cur_pos = getcurx(input_bar);
            }
            else { // if the user entered some weird stuff (ctrl and some character, alt and some character, etc)
                int dif = getcurx(input_bar) - prev_cur_pos;
                if (dif > 1) { // if the cursor moved more than one space
                    for (int i = 0; i < dif; i++) { // for each space the cursor moved
                        mvwdelch(input_bar, getcury(input_bar), getcurx(input_bar) - 1); // delete each character
                    }
                }
            }
        }
    }
	close(skt);
	return 0;
}

void* listen_to_server(void* arg) {
    char type;
    uint16_t cur_room_num;
    char cur_room_name[32];
    bool current_player = false; // current_player is true if the player received from the server is the one the user is playing as
    bool print_a_newline = false; // tracks when a monster was printed, so that formatting output can look better
    int16_t old_health; // keep track of the player's old health and gold
    uint16_t old_gold;
    std::set<uint16_t> seen_connections; // a list of connections that have been printed out before. When seen again, do not print their descriptions to reduce screen clutter

    struct pollfd pfd;
    pfd.fd = skt;
    pfd.events = POLLIN;
    struct pollfd fd_arr[1];
    fd_arr[0] = pfd;

    while (!exit_thread) {

        if (poll(fd_arr, 1, 500) == 0) { // continue the loop and check exit_thread bool every 500 ms that nothing has been sent
            continue;
        }

        if (recv(skt, &type, 1, 0) != 1) {// read the type of incoming message, error if it is not 1
            print_error("The server appears to have disconnected. Use !leave to leave the game.\n\n");
            return 0;
        }
            
        //wprintw(main_screen, "Type: %d\n", type);

        if (type == 1) { // MESSAGE
            //wprintw(main_screen, "Message received\n");
            handle_message(print_a_newline);

        } else if (type == 7) { // ERROR
            handle_error(print_a_newline);

        } else if (type == 8) { // ACCEPT
            handle_accept(print_a_newline);

        } else if (type == 9) { // ROOM
            handle_room(print_a_newline, cur_room_num, cur_room_name);
            
        } else if (type == 10) { // CHARACTER
            handle_character(print_a_newline, cur_room_num, current_player, old_gold, old_health);

        } else if (type == 11) { // GAME
            handle_game();

        } else if (type == 13) { // CONNECTION
            handle_connection(print_a_newline, cur_room_name, seen_connections);
            
        } else if (type == 14) { // VERSION
            handle_version();
        } else { // Something the server should not send
            print_error(std::string("Error! The server has sent something strange (type ") + type + std::string(")."));
            char buf[1024];
            while (1024 == read(skt, buf, 1024)) {};
        }
    }
    return 0;
}

// called when the user types something and presses enter. Handles commands. Messy, but making everything an individual function would be annoying
void process_user_input(std::string user_input) { 
    static std::string command = ""; // the current command the user is using
    static std::string player_name, recipient; // local copy of the player's name (to avoid having to access the global structure every time) and a static string holding the recipient of a message
    static int cur_stats = 0; // keep track of user's current stat allocations, which is used to display how many points they have left to allocate
    static int arg = 0; // keep track of which argument of a command a user is currently on
    static bool started = false; // bool used to track when a user has started a game. Disables some commands when true
    static int choice; // Used to keep track of the user's choice in a few different commands

    bool found_command = true; // if the user entered a valid command, true, else false

    if (arg == 0) {
        bool number = true;
        for (char c : user_input) { // for each character in the user input
            if (!isdigit(c)) { // if the input is not a number
                number = false;
                break; // break the loop and continue
            }
        }
        if (number) {
            arg = 1; // set the arg to 1
            command = "!move"; // set the command to !move
            found_command = true; // set found command to true  
        }
    }

    if (command == "") // if a command is not being handled, search the user input for a command
        std::find(command_array.begin(), command_array.end(), user_input) == command_array.end() ? found_command = false : found_command = true;

    if (arg == 0 && !found_command) {
        // if the user enters an invalid command, print an error and return
        print_error("Error! Command \"" + user_input + "\" not found. Use !help to see available commands\n\n");
        return;

    } else if (found_command) {
        // handle a command, increment the arg counter
        if (command == "") // if a command is not being handled currently, change the command to be the user input
            command = user_input;
        if (command == "!make") { // handle making a character. no need for mutex locking, as it is guaranteed to happen before the other thread can modify the player struct
            if (check_made_character()) {
                print_error("Error! Character already created.\n\n");
                command = "";
                return;
            }
            if (arg == 0)
                print_command_prompt("Enter your name (Max 31 characters)\n");

            if (arg == 1) {        // character name
                if (user_input.length() > 31) {
                    print_error("Error! Name too long. Try again.\n\n");
                    return;
                }
                player.name[31] = 0;
                strncpy(player.name, user_input.c_str(), 31);
                player_name = std::string(player.name); // keep a copy local to this function

                print_command_prompt("Would you like to automatically join battles? (y/n)\n");
            } else if (arg == 2) { // character flags
                if (user_input.length() != 1) {
                    print_error("Error! Enter either y or n.\n\n");
                    return;
                } else {
                    if (tolower(user_input[0] == 'y'))
                        player.flags |= JOIN_BATTLE;
                    else if (tolower(user_input[0] == 'n')) {
                        // do nothing
                    } else {
                        print_error("Error! Enter either y or n.\n\n");
                        return;
                    }
                }

                player.flags |= ALIVE | READY; // set the character to alive
                print_command_prompt("Enter character attack (remaining: " + std::to_string(initial_stats - cur_stats) + ")\n");
            } else if (arg == 3) { // character attack
                for (char c : user_input) {
                    if (!isdigit(c)) {
                        print_error("Error! Enter a valid, non-negative number.\n\n");
                        return;
                    }
                }
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                if (initial_stats - cur_stats - stoi(user_input) < 0) {
                    print_error("Error! Stat limit exceeded. Try again.\n\n");
                    return;
                }

                player.attack = stoi(user_input);

                cur_stats += player.attack;
                print_command_prompt("Enter character defense (remaining: " + std::to_string(initial_stats - cur_stats) + ")\n");
            } else if (arg == 4) { // character defense
                for (char c : user_input) {
                    if (!isdigit(c)) {
                        print_error("Error! Enter a valid, non-negative number.\n\n");
                        return;
                    }
                }
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                if (initial_stats - cur_stats - stoi(user_input) < 0) {
                    print_error("Error! Stat limit exceeded. Try again.\n\n");
                    return;
                }
                player.defense = stoi(user_input);
                cur_stats += player.defense;
                print_command_prompt("Enter character regen (remaining: " + std::to_string(initial_stats - cur_stats) + ")\n");
            } else if (arg == 5) {
                for (char c : user_input) {
                    if (!isdigit(c)) {
                        print_error("Error! Enter a valid, non-negative number.\n\n");
                        return;
                    }
                }
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                if (initial_stats - cur_stats - stoi(user_input) < 0) {
                    print_error("Error! Stat limit exceeded. Try again.\n\n");
                    return;
                }
                player.regen = stoi(user_input);
                print_command_prompt("Enter description\n");
            } else if (arg == 6) {
                player.desc_len = user_input.length() + 1;
                player.description = (char*) malloc(player.desc_len);
                player.description[player.desc_len - 1] = 0;
                memcpy(player.description, user_input.c_str(), player.desc_len - 1);

                send_character(skt, &player);
		cur_stats = 0; // reset cur_stats in case the server rejects the character and a new one needs to be made

                print_command_prompt(""); // essentially blank the command prompt line
                command = "";
                arg = 0;
                return;
            }

        } else if (command == "!start") {
            /* remove this check, allow the server to do the checking instead (my server needs to receive them for a feature to work)
            if (started && color != 3) { // if the user has started and are not dead (some servers allow for reviving, so re-enabling this is potentially important)
                print_error("Error! Game already started!\n\n");
                command = "";
                return;
            }
            */
            if (!check_made_character()) {
                print_error("Error! !make a character before starting!\n\n");
                command = "";
                return;
            }
            char type = 6;
            send(skt, &type, 1, 0);
            command = "";
            started = true;
            arg = 0;
            return;

        } else if (command == "!move") { // change to another room
            if (!started) {
                print_error("Error! !start the game before moving rooms.\n\n");
                command = "";
                arg = 0;
                return;
            }
            if (arg == 0)
                print_command_prompt("Enter the Room Number");
            else if (arg == 1) {
                
                for (char c : user_input) 
                    if (!isdigit(c)) {
                        print_error("Please enter a valid, non-negative number.\n\n");
                        return;
                    }

                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                uint16_t room_num = stoi(user_input);
                pthread_mutex_lock(&room_mutex);
                bool found_room = room_list.find(room_num) != room_list.end(); // true if found, false if not
                pthread_mutex_unlock(&room_mutex);

                //if (!found_room)
                    //print_error("Error! Connected room with that number not found. Please see the connected rooms list or use !roomlist.\n\n");

                send_changeroom(skt, room_num);

                print_command_prompt("");
                command = "";
                arg = 0;
                return;
            }
        } else if (command == "!back") {
            if (!started) {
                print_error("Error! !start the game before moving rooms.\n\n");
                command = "";
                return;
            }
            
            pthread_mutex_lock(&prev_room_mutex);
            uint16_t tmp = prev_room;
            pthread_mutex_unlock(&prev_room_mutex);

            send_changeroom(skt, tmp);

            command = "";
            return;
            
        } else if (command == "!fight") { // fight monsters in the room
            char type = 3;
            send(skt, &type, 1, 0); // send the fight request
            arg = 0;
            command = "";
            return;

        } else if (command == "!pvp") {

            static std::map<int, character> tmp_map;

            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            if (arg == 0) {

                int cnt = 1;
                pthread_mutex_lock(&character_map_mutex);

                if (character_map.size() == 0) {
                    print_error("Oops! There are no players or monsters in this room.\n\n");
                    command = "";
                    pthread_mutex_unlock(&character_map_mutex);
                    return;
                }
                for (auto c : character_map) {
                    tmp_map.insert(std::make_pair(cnt, *c.second)); // add the character and its associated number
                    cnt++;
                }
                pthread_mutex_unlock(&character_map_mutex);
                wprintw(main_screen, "List of characters in the room:\n\n");
                for (auto c : tmp_map) {
                    if (c.second.flags & MONSTER) {
                        wattron(main_screen, COLOR_PAIR(3) | A_BOLD); // print red if the character is a monster
                        wprintw(main_screen, "%d: %s\n", c.first, c.second.name);
                        wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
                    } else {
                        wprintw(main_screen, "%d: %s\n", c.first, c.second.name);
                    }
                }
                wprintw(main_screen, "\n");

                print_command_prompt("Enter a number corresponding to the desired character");

            } else if (arg == 1) {
                for (char c : user_input) 
                    if (!isdigit(c)) {
                        print_error("Error! Please enter a valid number.\n\n");
                        return;
                    }
                
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                choice = stoi(user_input);
                if (tmp_map.find(choice) == tmp_map.end()) {
                    print_error("Error! Please enter a valid number.\n\n");
                    return;
                }

                send_pvp(skt, tmp_map.at(choice).name);

                std::map<int, character>().swap(tmp_map); // clear the map
                command = "";
                arg = 0;
                print_command_prompt("");
                return;
            }

        } else if (command == "!msg") {
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            if (arg == 0) {
                print_command_prompt("Enter a player name (case-senstive!)");
            } else if (arg == 1) {
                if (user_input.length() > 32) {
                    print_error("Error! Player names cannot be more than 32 characters long! Try again.\n\n");
                    return;
                }
                recipient = user_input; // set the recipient to be what the user entered
                print_command_prompt("Enter a message to send");

            } else if (arg == 2) {
                if (user_input.length() > 65535) {
                    print_error("Error! Messages of length > 65535 are not permitted. Try again.\n\n");
                    return;
                }

                send_message(skt, player_name.c_str(), recipient.c_str(), user_input.c_str());

                print_to_chat((char*) (player_name + " -> " + recipient).c_str(), (char*) user_input.c_str()); // cast away the const :)

                print_command_prompt("");
                command = "";
                arg = 0;
                return;
            }
            
        } else if (command == "!msgn") { // message another player in the current room based on number
            static std::map<int, character> tmp_map;

            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            if (arg == 0) {

                int cnt = 1;
                pthread_mutex_lock(&character_map_mutex);
                if (character_map.size() == 0) {
                    print_error("Oops! There are no players or monsters in this room.\n\n");
                    pthread_mutex_unlock(&character_map_mutex);
                    return;
                }
                for (auto c : character_map) {
                    if (c.second->flags & MONSTER) continue; // the character is a monster, skip
                    tmp_map.insert(std::make_pair(cnt, *c.second)); // add the character and its associated number
                    cnt++;
                }
                pthread_mutex_unlock(&character_map_mutex);

                if (tmp_map.empty()) {
                    print_error("Oops! There are no players in this room. Use !msg to message anyone in the server by name instead.\n\n");
                    command = "";
                    return;
                }
                
                wprintw(main_screen, "List of characters in the room:\n\n");
                for (auto c : tmp_map) {
                    wprintw(main_screen, "%d: %s\n", c.first, c.second.name);
                }
                wprintw(main_screen, "\n");
                print_command_prompt("Enter a number corresponding to the desired character");

            } else if (arg == 1) {
                for (char c : user_input) 
                    if (!isdigit(c)) {
                        print_error("Error! Please enter a valid number.\n\n");
                        return;
                    }
                
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                choice = stoi(user_input);
                if (tmp_map.find(choice) == tmp_map.end()) {
                    print_error("Error! Please enter a valid number.\n\n");
                    return;
                }
                
                print_command_prompt(std::string("Enter a message to send to ") + tmp_map.at(choice).name);

            } else if (arg == 2) {
                if (user_input.length() > 65535) {
                    print_error("Error! Messages of length > 65535 are not permitted. Try again.\n\n");
                    return;
                }

                send_message(skt, player_name.c_str(), tmp_map.at(choice).name, user_input.c_str());

                print_to_chat((char*) (player_name + " -> " + std::string(tmp_map.at(choice).name)).c_str(), (char*) user_input.c_str()); // cast away the const :)

                std::map<int, character>().swap(tmp_map); // clear the map
                print_command_prompt("");
                command = "";
                arg = 0;
                choice = 0;
                return;
            }
            
        } else if (command == "!playerlist") { // list players in the room
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            int cnt = 0;
            wattron(main_screen, A_BOLD | A_ITALIC);
            wprintw(main_screen, "List of players in the current room:\n\n");
            wattroff(main_screen, A_ITALIC | A_BOLD);

            pthread_mutex_lock(&character_map_mutex);
            for (auto c : character_map) {
                if (c.second->flags & MONSTER) continue; // the character is a monster, skip
                
                wprintw(main_screen, (c.first + " | " + std::string(c.second->stat_string) + "\n").c_str());
                cnt++;
            }
            pthread_mutex_unlock(&character_map_mutex);
            if (cnt == 0) wprintw(main_screen, "There are no players in the current room!\n");

            wprintw(main_screen, "\n");
            
            command = "";
            return;

        } else if (command == "!monsterlist") { // list monsters in the room
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            std::string tmp = "";
            pthread_mutex_lock(&character_map_mutex);
            for (auto c : character_map) {
                if (!(c.second->flags & MONSTER)) continue; // player, skip

                tmp += c.first + " | " + std::string(c.second->stat_string) + "\n";
            }
            if (tmp == "") {
                wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
                wprintw(main_screen, "There are no monsters in the current room!\n\n");
                wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
                command = "";
                pthread_mutex_unlock(&character_map_mutex);
                return;
            }
            pthread_mutex_unlock(&character_map_mutex);
            wattron(main_screen, A_BOLD | A_ITALIC);
            wprintw(main_screen, "List of monsters in the current room:\n\n");
            wattroff(main_screen, A_BOLD | A_ITALIC);
            wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
            wprintw(main_screen, "%s\n", tmp.c_str());
            wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            command = "";
            return;

        } else if (command == "!alive") {
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            int cnt = 0;
            wattron(main_screen, A_BOLD | A_ITALIC);
            wprintw(main_screen, "List of living players in the current room:\n\n");
            wattroff(main_screen, A_ITALIC | A_BOLD);

            pthread_mutex_lock(&character_map_mutex);
            for (auto c : character_map) {
                if ((c.second->flags & MONSTER) || !(c.second->flags & ALIVE)) continue; // the player is a monster or dead, skip

                wprintw(main_screen, (c.first + " | " + std::string(c.second->stat_string) + "\n").c_str());
                cnt++;
            }
            pthread_mutex_unlock(&character_map_mutex);
            if (cnt == 0) wprintw(main_screen, "There are no living players in the current room!\n");

            wprintw(main_screen, "\n");
            
            command = "";
            return;
            
        } else if (command == "!dead") {
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            int cnt = 0;
            wattron(main_screen, A_BOLD | A_ITALIC);
            wprintw(main_screen, "List of dead players in the current room:\n\n");
            wattroff(main_screen, A_ITALIC | A_BOLD);

            pthread_mutex_lock(&character_map_mutex);
            for (auto c : character_map) {
                if ((c.second->flags & MONSTER) || (c.second->flags & ALIVE)) continue; // the character is a monster or alive, skip
                wprintw(main_screen, (c.first + " | " + std::string(c.second->stat_string) + "\n").c_str());
                cnt++;
            }
            pthread_mutex_unlock(&character_map_mutex);
            if (cnt == 0) wprintw(main_screen, "There are no dead players in the current room!\n");

            wprintw(main_screen, "\n");
            
            command = "";
            return;
            
        } else if (command == "!roomlist") {
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            std::string tmp = "";
            pthread_mutex_lock(&room_mutex);
            for (auto r : room_list) {
                tmp += std::to_string(r.first) + ": " + r.second + "\n";
            }
            pthread_mutex_unlock(&room_mutex);
            wattron(main_screen, A_BOLD | A_ITALIC);
            wprintw(main_screen, "List of connected rooms:\n\n");
            wattroff(main_screen, A_ITALIC);
            wattron(main_screen, COLOR_PAIR(4));
            wprintw(main_screen, "%s\n", tmp.c_str());
            wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
            command = "";
            return;
            
            
        } else if (command == "!loot") { // loot a dead player or monster in the current room based on number
        static std::map<int, character> tmp_map;

            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            if (arg == 0) {

                int cnt = 1;
                pthread_mutex_lock(&character_map_mutex);

                if (character_map.size() == 0) {
                    print_error("Oops! There are no players or monsters in this room.\n\n");
                    command = "";
                    pthread_mutex_unlock(&character_map_mutex);
                    return;
                }
                for (auto c : character_map) {
                    if (!(c.second->flags & ALIVE)) { // if the character is dead, add them
                        tmp_map.insert(std::make_pair(cnt, *c.second)); // add the character and its associated number
                        cnt++;
                    }
                }
                if (cnt == 1) {
                    print_error("Oops! There are no dead players or monsters in this room.\n\n");
                    command = "";
                    pthread_mutex_unlock(&character_map_mutex);
                    return;
                }
                pthread_mutex_unlock(&character_map_mutex);
                wprintw(main_screen, "List of dead characters in the room:\n\n");
                for (auto c : tmp_map) {
                    if (c.second.flags & MONSTER) {
                        wattron(main_screen, COLOR_PAIR(3) | A_BOLD); // print red if the character is a monster
                        wprintw(main_screen, "%d: %s (%u gold)\n", c.first, c.second.name, c.second.gold);
                        wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
                    } else {
                        wprintw(main_screen, "%d: %s (%u gold)\n", c.first, c.second.name, c.second.gold);
                    }
                }
                wprintw(main_screen, "\n");
                if (tmp_map.empty()) {
                    print_error("Oops! There are no dead characters in this room!\n\n");
                    command = "";
                    return;
                }
                print_command_prompt("Enter a number corresponding to the desired character");

            } else if (arg == 1) {
                for (char c : user_input) 
                    if (!isdigit(c)) {
                        print_error("Error! Please enter a valid number.\n\n");
                        return;
                    }
                
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                choice = stoi(user_input);
                if (tmp_map.find(choice) == tmp_map.end()) {
                    print_error("Error! Please enter a valid number.\n\n");
                    return;
                }

                send_loot(skt, tmp_map.at(choice).name);

                std::map<int, character>().swap(tmp_map); // clear the map
                command = "";
                arg = 0;
                print_command_prompt("");
                return;
            }
        } else if (command == "!about") {
            if (!started) {
                print_error("Make a character and start the game first!\n\n");
                command = "";
                return;
            }
            static std::map<int, character> tmp_map; // associate characters with numbers for the user to choose instead of typing their names

            if (arg == 0) {
                
                int cnt = 1;
                pthread_mutex_lock(&character_map_mutex);
                if (character_map.size() == 0) {
                    print_error("Oops! There are no players or monsters in this room.\n\n");
                    pthread_mutex_unlock(&character_map_mutex);
                    command = "";
                    return;
                }
                for (auto c : character_map) {
                    tmp_map.insert(std::make_pair(cnt, *c.second)); // add the character and its associated number
                    cnt++;
                }
                pthread_mutex_unlock(&character_map_mutex);
                wprintw(main_screen, "List of characters in the room:\n\n");
                for (auto c : tmp_map) {
                    if (c.second.flags & MONSTER) {
                        wattron(main_screen, COLOR_PAIR(3) | A_BOLD); // print red if the character is a monster
                        wprintw(main_screen, "%d: %s\n", c.first, c.second.name);
                        wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
                    } else {
                        wprintw(main_screen, "%d: %s\n", c.first, c.second.name);
                    }
                }
                wprintw(main_screen, "\n");
                print_command_prompt("Enter a number corresponding to the desired character");
            } else if (arg == 1) {
                for (char c : user_input) 
                    if (!isdigit(c)) {
                        print_error("Error! Please enter a valid number.\n\n");
                        return;
                    }
                
                if (!attempt_int_conversion(user_input)) {
                    print_error("Error! You entered a very large or small number or something very strange. Try again.\n\n");
                    return;
                }
                choice = stoi(user_input);
                if (tmp_map.find(choice) == tmp_map.end()) {
                    print_error("Error! Please enter a valid number.\n\n");
                    return;
                }
                wprintw(main_screen, "Description of ");
                if (tmp_map.at(choice).flags & MONSTER) { // if its a monster, print the name in red
                    wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
                    wprintw(main_screen, "%s", tmp_map.at(choice).name); // print the description of the specified character
                    wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
                } else {
                    wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
                    wprintw(main_screen, "%s", tmp_map.at(choice).name); // print the description of the specified character
                    wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
                }
                wprintw(main_screen, ": %s\n\n", tmp_map.at(choice).description);
                arg = 0;
                command = "";
                std::map<int, character>().swap(tmp_map); // clear the map
                print_command_prompt("");
                return;
            }

        } else if (command == "!color") { // change the color of the interface
            if (arg == 0) {
                pthread_mutex_lock(&color_mutex);
                if (color == 3) { // if the color is 3, the player is dead. Disallow changing it
                    print_error("Error! You are dead. The screen remains red.\n\n");
                    command = "";
                    pthread_mutex_unlock(&color_mutex);
                    return;
                }
                pthread_mutex_unlock(&color_mutex);
                print_command_prompt("Enter the number corresponding to the color you want");
                wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
                wprintw(main_screen, "1: GREEN\n");
                wattroff(main_screen, COLOR_PAIR(1));
                wattron(main_screen, COLOR_PAIR(2));
                wprintw(main_screen, "2: BLUE\n");
                wattroff(main_screen, COLOR_PAIR(2));
                wattron(main_screen, COLOR_PAIR(4));
                wprintw(main_screen, "3: MAGENTA\n");
                wattroff(main_screen, COLOR_PAIR(4));
                wattron(main_screen, COLOR_PAIR(6));
                wprintw(main_screen, "4: YELLOW\n");
                wattroff(main_screen, COLOR_PAIR(6));
                wattron(main_screen, COLOR_PAIR(9));
                wprintw(main_screen, "5: WHITE\n\n");
                wattroff(main_screen, COLOR_PAIR(9) | A_BOLD);
            } else if (arg == 1) {
                if (user_input.length() > 1 || !isdigit(user_input[0])) { // if the user entered incorrect input
                    print_error("Error! Please enter a valid number.\n\n");
                    return;
                }
                int col = stoi(user_input);
                pthread_mutex_lock(&user_chosen_color_mutex);
                switch (col) {
                    case 1: user_chosen_color = 1; break;
                    case 2: user_chosen_color = 2; break;
                    case 3: user_chosen_color = 4; break;
                    case 4: user_chosen_color = 6; break;
                    case 5: user_chosen_color = 9; break;
                    default: print_error("Error! Please enter a number between 1 and 5\n\n"); return;
                }
                pthread_mutex_unlock(&user_chosen_color_mutex);
                pthread_mutex_lock(&color_mutex);
                color = user_chosen_color;
                pthread_mutex_unlock(&color_mutex);
                print_command_prompt("");
                command = "";
                arg = 0;
                return;
            }

        } else if (command == "!map") {
            if (!map_enabled) {
                print_error("Error! Map is unavailable.\n\n");
                command = "";
                return;
            }
            if (!started) {
                print_error("Error! You have not started!\n\n");
                command = "";
                return;
            }
            std::string filename = std::string("/tmp/") + std::string(username) + std::string(".dot"); // construct a filename based on the user's system username
            struct stat i;
            int cnt = 0;
            while (stat(filename.c_str(), &i) == 0) { // while there IS a file already with that name, keep trying new names
                filename += std::to_string(cnt); // this creates [username].dot0, [username].dot1, etc. obviously this could cause problems if there were a whole lot of files, but I can't imagine that will happen
                cnt++;
            }
            // wprintw(main_screen, "File name: %s\n\n", filename.c_str());
            pthread_mutex_lock(&graph_mutex);
            gvLayout(gvc, G, "dot");
            gvRenderFilename(gvc, G, "dot", filename.c_str()); // render the current map to [filename]
            pthread_mutex_unlock(&graph_mutex);

            std::string cmd = std::string("graph-easy ") + filename + std::string(" --boxart 2>&1");

            FILE *f = popen(cmd.c_str(), "r");
            if (f == 0) {
                print_error("Error! Could not create a map.\n\n");
                command = "";
                return;
            }
            wattron(main_screen, A_BOLD);
            wprintw(main_screen, "Map of the known area:\n\n");
            wattroff(main_screen, A_BOLD);
            char buf[50000]; // create a large buffer, i'm assuming maps wont ever be that big...
            while (fgets(buf, 50000, f)) {
                wprintw(main_screen, "%s", buf);
            }
            pclose(f);
            remove(filename.c_str()); // attempt to remove the temp file
            wprintw(main_screen, "\n\n");
            command = "";
            return;
            
        } else if (command == "!qmake") {
            if (check_made_character()) {
                print_error("Error! Character already created.\n\n");
                command = "";
                return;
            }
            if (arg == 0) {
                print_command_prompt("Enter your name (Max 31 characters)\n");

            } else if (arg == 1) {
                if (user_input.length() > 31) {
                    print_error("Error! Name too long. Try again.\n\n");
                    return;
                }
                player.name[31] = 0;
                strcpy(player.name, user_input.c_str());
                player_name = std::string(player.name); // set the local player_name
                player.flags |= READY | ALIVE;
                player.attack = initial_stats / 2;
                player.defense = initial_stats / 4;
                player.regen = initial_stats / 4;
                player.desc_len = 24;

                player.description = (char*) malloc(24);
                player.description[player.desc_len -1] = 0;
                memcpy(player.description, "Logged in from LURKMAN", 23); // copied from lurkwolf :)

                send_character(skt, &player);

                char type = 6;
                send(skt, &type, 1, 0); // send the start message

                //wprintw(main_screen, "Character automatically created and started!\n\n");

                print_command_prompt("");
                command = "";
                started = true;
                arg = 0;
                return;
            }
            
            
        } else if (command == "!bottom") {
            refresh_main_pad(0, true); // refreshing the pads with a value of resized = true will scroll them to the bottom
            command = "";
            return;

        } else if (command == "!cbottom") {
            refresh_chat_pad(0, true);
            command = "";
            return;
            
        } else if (command == "!help") {
            wattron(main_screen, COLOR_PAIR(6));
            wprintw(main_screen, "List of commands:\n\n");
            wprintw(main_screen, "• Scroll the main window: up and down arrow keys (or just mouse scroll, depending on the terminal)\n");
            wprintw(main_screen, "• Scroll the message box: CTRL+U for up, CTRL+D for down\n");
            wprintw(main_screen, "• Use the left and right arrow keys to quickly access previous commands (similar to command line up and down arrow keys)\n\n");
            wprintw(main_screen, "!make:        Make a character to play as\n");
            wprintw(main_screen, "!start:       Start the game. Use after !make\n");
            wprintw(main_screen, "!qmake:       Create a character and start the game quickly using just a name\n");
            wprintw(main_screen, "!move:        Move to one of the connected rooms. You can also just type a number into the input bar to move rooms\n");
            wprintw(main_screen, "!map:         Print a map of explored areas. Susceptible to not fitting within the main window or other errors\n");
            wprintw(main_screen, "!back:        Return to the previous room, assuming it is still accessible\n");
            wprintw(main_screen, "!fight:       Initiate a fight in the current room\n");
            wprintw(main_screen, "!loot:        Attempt to loot a dead player or monster in the current room\n");
            wprintw(main_screen, "!pvp:         Initiate a player-vs-player fight in the current room, provided the server permits\n");
            wprintw(main_screen, "!msgn:        Message a player in the current room based on a number, rather than their name\n");
            wprintw(main_screen, "!msg:         Message any player in the server by typing their exact name\n");
            wprintw(main_screen, "!playerlist:  Print a list of players in the current room, along with their stats\n");
            wprintw(main_screen, "!monsterlist: Print a list of monsters in the current room, along with their stats\n");
            wprintw(main_screen, "!alive:       Print a list of all living players in the current room\n");
            wprintw(main_screen, "!dead:        Print a list of all dead players in the current room\n");
            wprintw(main_screen, "!roomlist:    Print a list of connected rooms and their numbers\n");
            wprintw(main_screen, "!about:       View the description of a given character\n");
            wprintw(main_screen, "!color:       Change the color of the interface\n");
            wprintw(main_screen, "!bottom:      Scroll the main window all the way down\n");
            wprintw(main_screen, "!cbottom:     Scroll the messages box all the way down\n");
            wprintw(main_screen, "!leave:       Leave the game and close Lurkman (this is the best command)\n");
            wprintw(main_screen, "!sus:         ඞ\n\n");
            wattroff(main_screen, COLOR_PAIR(6));

            command = "";
            arg = 0;
            return;
        } else if (command == "!sus") {
            
            wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
            wprintw(main_screen, "                     .::--====-:.                 \n                 -*%%@@@@@@@@@@@@@@%%+-             \n               -%%@@#=-:.       .-+#@@@#-          \n              +@@#.                 -*@@@+.       \n             =@@*                      =@@@:      \n            .@@%%%%              .:-==++++=+@@#      \n         ...+@@=          :*%%@@@@@@@@@@@@@@@%%+    \n     -*%%@@@@@@@.         .@@%%=-:.        .-*@@@-  \n   =@@@@#+=-+@@#         =@@=               =@@@- \n  #@@@+.     *@@-        +@@-                *@@%% \n =@@%%.       :@@*        .@@@=               *@@@ \n %%@@:         @@%%         .*@@@#+-.     ..:-*@@%%: \n @@%%          %%@@            -+%%@@@@@@@@@@@@@@+   \n:@@#          %%@@                .--====-:#@@-    \n-@@*          #@@.                        +@@-    \n=@@*          #@@.                        =@@+    \n-@@#          #@@.                        -@@*    \n:@@%%          %%@@                         .@@%%    \n @@@          %%@@                          @@@    \n *@@-         @@%%                          #@@.   \n -@@@        .@@*                          *@@:   \n  @@@#       =@@=                          #@@:   \n  -@@@#.     #@@.                          #@@.   \n   :%%@@@%%+=-*@@*          :*%%%%#-           @@@    \n     :*%%@@@@@@=           #@@#@@-          @@@    \n         .:%%@@.           #@@:@@%%         -@@*    \n           %%@@.           *@@:%%@@         =@@=    \n           #@@:           *@@-%%@@-..  ..:-%%@@.    \n           .%%@@@%%%%%%%%%%%%@@@@@@# :#@@@@@@@@@@@*:     \n             :=++***+++==--.     .:----::.        \n\n");
            wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);

            command = "";
            arg = 0;
            return;
            
        } else if (command == "!leave") {
            if (arg == 0) {
                print_command_prompt("Are you sure? (y/n)");
            } else if (arg == 1) {
                if (user_input.length() > 1) {
                    print_error("Error! Enter either y or n\n\n");
                    return;
                }
                if (user_input[0] == 'y') {
                    exit_gracefully(0);
                } else if (user_input[0] == 'n') {
                    print_command_prompt("");
                    command = "";
                    arg = 0;
                    return;
                } else {
                    print_error("Error! Enter either y or n\n\n");
                    return;
                }
            }
        }
        arg++;
    }
}
