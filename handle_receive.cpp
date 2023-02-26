#include "lurk_structs.h"
#include "receive_from_server.h"
#include "curses_control.h"
#include "helper_funcs.h"
#include<string.h>
#include<ncurses.h>
#include<sys/socket.h>
#include<map>
#include<set>


extern int skt;
extern uint16_t initial_stats;

extern character player;
extern pthread_mutex_t player_mutex;

extern WINDOW* main_screen;
extern WINDOW * main_screen_border;

extern bool made_character;
extern pthread_mutex_t made_character_mutex;

extern int color;
extern pthread_mutex_t color_mutex;

extern int user_chosen_color; // color the user chooses. Used to cache their color preference in case they die and can be revived automatically by the server
extern pthread_mutex_t user_chosen_color_mutex;

extern std::map<std::string, character*> character_map;
extern pthread_mutex_t character_map_mutex;

extern std::map<int, std::string> room_list;
extern pthread_mutex_t room_mutex;

extern std::set<std::string> visited_rooms;
extern pthread_mutex_t graph_mutex;

extern uint16_t prev_room;
extern pthread_mutex_t prev_room_mutex;

void handle_message(bool &newline) {
    message* m = receive_message(skt);

    if (m->sender[30] == 0 && m->sender[31] == 1) { // the sender is the server (lurk version 2.3 only), so print to main screen
        if (newline) {
            wprintw(main_screen, "\n");
            newline = false;
        }
        if (strlen(m->sender) == 0) { // if the narrator has no name, print SERVER:
            wattron(main_screen, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
            wprintw(main_screen, "SERVER:");
            wattroff(main_screen, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
        } else {
            wattron(main_screen, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
            wprintw(main_screen, "%s:", m->sender);
            wattroff(main_screen, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
        }
        wprintw(main_screen, " %s\n\n", m->message);
    } else { // else another player has send the message, so it goes in the chat
        print_to_chat(m->sender, m->message); // print the message to the chat
    }
    free(m->message);
    free(m);
}

void handle_error(bool &newline) {
    error *e = receive_error(skt);

    if (newline) {
        wprintw(main_screen, "\n");
        newline = false;
    }
    wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
    wprintw(main_screen, "Error received from server with code %u: %s\n\n", e->err_code, e->message);
    //if (err_code == 1)  // bad room request
        //wprintw(main_screen, "Use !roomlist for a list of rooms (provided the server sent things in the correct order)\n\n");
    wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
    free(e->message);
    free(e);
}

void handle_accept(bool &newline) {
    char accept_type;
    recv(skt, &accept_type, 1, 0);
    //wprintw(main_screen, "Server accepted type %d\n\n", accept_type);
    if (accept_type == 1) {
        if (newline) {
            wprintw(main_screen, "\n");
            newline = false;
        }       
        wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
        wprintw(main_screen, "Message delivered successfully!\n\n");
        wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
    } else if (accept_type == 10) {
        if (newline) {
            wprintw(main_screen, "\n");
            newline = false;
        }
        wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
        wprintw(main_screen, "Character accepted!\n\n");
        wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
        pthread_mutex_lock(&made_character_mutex);
        made_character = true;
        pthread_mutex_unlock(&made_character_mutex);
    }
}

void handle_room(bool &newline, uint16_t &cur_room_num, char* cur_room_name) {
    static int16_t last_room_printed = -1;
    room *r = receive_room(skt);

    if (r->room_num == last_room_printed) { // dont bother printing it out if it has already been printed out
        free(r->description);
        free(r);
        return;
    }
    
    // if the user received a room (and it isnt a duplicate, which is checked above), that should mean they changed rooms. Clear the player list and connected rooms list
    cur_room_num = r->room_num;
    strncpy(cur_room_name, r->room_name, 32); // update cur_room_name
    pthread_mutex_lock(&character_map_mutex);
    for (auto c : character_map) {
        free(c.second->description);
        free(c.second->stat_string);
        free(c.second);
    }
    std::map<std::string, character*>().swap(character_map); // clear the player map
    pthread_mutex_unlock(&character_map_mutex);
    pthread_mutex_lock(&room_mutex);
    std::map<int, std::string>().swap(room_list); // clear the room list
    pthread_mutex_unlock(&room_mutex);

    pthread_mutex_lock(&graph_mutex);
    if (visited_rooms.find(std::string(r->room_name)) == visited_rooms.end())
        visited_rooms.insert(std::string(r->room_name));
    pthread_mutex_unlock(&graph_mutex);

    last_room_printed = r->room_num;

    if (newline) {
        wprintw(main_screen, "\n");
        newline = false;
    }

    std::string divider = std::string(getmaxx(main_screen_border) - 4, '-');
    wattron(main_screen, A_BOLD);
    wprintw(main_screen, divider.c_str());

    wattron(main_screen, COLOR_PAIR(6));
    wprintw(main_screen, "\nMoved to room %d: %s\n", r->room_num, r->room_name);
    wattroff(main_screen, COLOR_PAIR(6) | A_BOLD);
    wprintw(main_screen, "Description: %s\n\n", r->description);
    free(r->description);
    free(r);
}

void handle_character(bool &newline, uint16_t cur_room_num, bool &current_player, uint16_t &old_gold, int16_t old_health) {
    
    character *p = receive_character(skt);

    //wprintw(main_screen, "Character name: %s\n\n", std::string(name).c_str());

    pthread_mutex_lock(&character_map_mutex);
    if (character_map.find(std::string(p->name)) != character_map.end()) {
        // if the player is already in the map, they must have just been updated.
        // this means the server sent the character again, which means someone started a fight or they left the room.
        // if they are still in the room, the player should be removed from the map and re-inserted to update them
        // if they left the room, their room number will be different than the current room number. Delete them and move to the next loop iteration in this case

        if (character_map.at(std::string(p->name))->health > p->health) { // if the character lost health
            if (p->flags & MONSTER) {
                wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
                wprintw(main_screen, "%s ", p->name);
                wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            } else {
                wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
                wprintw(main_screen, "%s ", p->name);
                wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
            }
            if (!(p->flags & ALIVE))
                wprintw(main_screen, "died!\n");
            else 
                wprintw(main_screen, "lost %d health!\n", character_map.at(std::string(p->name))->health - p->health);
            
            newline = true;

        } else if (character_map.at(std::string(p->name))->health < p->health) { // if the character gained health
            if (p->flags & MONSTER) {
                wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
                wprintw(main_screen, "%s ", p->name);
                wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            } else {
                wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
                wprintw(main_screen, "%s ", p->name);
                wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
            }
            wprintw(main_screen, "gained %d health!\n", p->health - character_map.at(std::string(p->name))->health);
            newline = true;
        }
        free(character_map.at(std::string(p->name))->description);
        free(character_map.at(std::string(p->name))->stat_string);
        free(character_map.at(std::string(p->name)));
        character_map.erase(std::string(p->name)); // remove the character from the map

        if (p->room_num != cur_room_num) { // they moved rooms / left
            wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
            wprintw(main_screen, "%s went to room %d!\n\n", p->name, p->room_num);
            wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
            pthread_mutex_unlock(&character_map_mutex);
            free(p->description);
            free(p);
            return; // continue with the loop if this is the case
        }
    } else {
        if (p->flags & MONSTER) { // if the character received is a monster and it is not already in the list
            wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
            wprintw(main_screen, "Enemy alert: %s | %s\n", p->name, p->description);
            wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            newline = true;
        } 
        /* TODO? difficult to implement reliably 
        else { // else they are a player, print a message indicating they just arrived
            wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
            wprintw(main_screen, "%s has entered the room!\n\n", p->name);
            wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
        }
        */
    }
    pthread_mutex_unlock(&character_map_mutex);

    pthread_mutex_lock(&player_mutex);
    
    std::string(p->name) == std::string(player.name) ? current_player = true : current_player = false; // if the character received is the user, set current_player to true

    if (current_player) { // update the user
        if (player.room_num != p->room_num) { // if the room number has changed, update the prev_room
            pthread_mutex_lock(&prev_room_mutex);
            prev_room = player.room_num; // set the previous room to be what the user was just in
            pthread_mutex_unlock(&prev_room_mutex);
        }
        old_health = player.health;
        old_gold = player.gold;
        player.health = p->health;
        player.gold = p->gold;
        player.room_num = p->room_num;

        // update stats as well, as they may change if the user revives a character already present in a server
        player.attack = p->attack;
        player.defense = p->defense;
        player.regen = p->regen;
    }
    pthread_mutex_unlock(&player_mutex);
    
    if (current_player) {
        if (old_gold != p->gold || old_health != p->health) { // if the player's health or gold changed, let them know, else don't
            if (newline) {
                wprintw(main_screen, "\n");
                newline = false;
            }
            if (old_health < p->health) { // the player gained health
                wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
                wprintw(main_screen, "You gained %d health!\n", p->health - old_health);
                wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
            } else if (old_health > p->health) { // the player lost health
                wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
                wprintw(main_screen, "You lost %d health!\n", old_health - p->health);
                wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            } else { // player health stayed the same
                // do nothing
            }

            if (old_gold < p->gold) { // the player gained gold
                wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
                wprintw(main_screen, "You gained %d gold!\n", p->gold - old_gold);
                wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
            } else if (old_gold > p->gold) { // the player lost gold
                wattron(main_screen, COLOR_PAIR(3) | A_BOLD);
                wprintw(main_screen, "You lost %d gold!\n", old_gold - p->gold);
                wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
            } else { // player gold stayed the same
                // do nothing
            }

            wprintw(main_screen, "\n\n");

            if (p->flags & ALIVE) { // if the player is revived, reset the color to what the user chose
                pthread_mutex_lock(&user_chosen_color_mutex);
                pthread_mutex_lock(&color_mutex);
                color = user_chosen_color;
                pthread_mutex_unlock(&color_mutex);
                pthread_mutex_unlock(&user_chosen_color_mutex);
            }

            if (!(p->flags & ALIVE)) { // if the dead flag was set to dead
                wattron(main_screen, COLOR_PAIR(3) | A_BLINK | A_BOLD | A_REVERSE);
                wprintw(main_screen, "YOU DIED\n");
                wattroff(main_screen, A_BLINK | A_REVERSE);
                wprintw(main_screen, "Depending on the server, you may be able to be revived. If so, when you are revived, the colors should reset to normal. Otherwise, you can no longer move or fight. Use !leave to leave the game.\n\n");
                wattroff(main_screen, COLOR_PAIR(3) | A_BOLD);
                pthread_mutex_lock(&color_mutex);
                color = 3; // set the color to red
                pthread_mutex_unlock(&color_mutex);
            }
        }
        free(p->description);
        free(p);
        return;
    }

    // construct the stat string
    std::string species, status;
    (p->flags & MONSTER) ? species = "M" : species = "P";
    (p->flags & ALIVE) ? status = "Alive" : status = "Dead";
    
    std::string character_stat_string = "";
    character_stat_string += status + std::string(" | $");
    character_stat_string += std::to_string(p->gold) + std::string(" | ");
    character_stat_string += std::string(SWORDS) + " " + std::to_string(p->attack) + " | ";
    character_stat_string += std::string(SHIELD) + " " + std::to_string(p->defense) + " | ";
    character_stat_string += std::string(REGEN) + " " + std::to_string(p->regen) + " | ";
    character_stat_string += std::string(HEART) + " " + std::to_string(p->health);

    p->stat_string = (char*) malloc(character_stat_string.length() + 1);
    p->stat_string[character_stat_string.length()] = 0;

    strcpy(p->stat_string, character_stat_string.c_str());
    
    // add the player to the player map

    pthread_mutex_lock(&character_map_mutex);
    character_map.insert(std::make_pair(std::string(p->name), p));
    pthread_mutex_unlock(&character_map_mutex);

    /*
    wprintw(main_screen, "Name: %s (%s %s)\n", name, status.c_str(), species.c_str());
    wprintw(main_screen, "Description: %s\n", desc);
    wprintw(main_screen, "Stats: ATK %d, DEF %d, REG %d, HP %d\n", atk, defense, regen, health);
    wprintw(main_screen, "Room Number: %d\n\n", room_num);
    */

}

void handle_game() {
    game *g = receive_game(skt);
    initial_stats = g->init_points;
    //wprintw(main_screen, "Initial Stats: %u\n", init_stats);
    wprintw(main_screen, "Stat Limit: %u\n\n", g->stat_limit);
    //wprintw(main_screen, "Description Length: %u\n", g->msg_len);
    wprintw(main_screen, "Situation: %s\n", g->description);
    wattron(main_screen, COLOR_PAIR(1) | A_BOLD);
    wprintw(main_screen, "\nTo begin, use !make to make a character and !start to join the game! Alternatively, use !qmake to create a character quickly with just a name! For a list of commands and usages, type !help. Use the up and down arrow keys to scroll the main window, and CTRL+U and CTRL+D to scroll the message window.\n\n");
    wattroff(main_screen, COLOR_PAIR(1) | A_BOLD);
    free(g->description);
    free(g);
}

void handle_connection(bool &newline, char* cur_room_name, std::set<uint16_t> &seen_connections) {
    connection *conn = receive_connection(skt);

    //wprintw(main_screen, "Room Number: %d\n\n", room_num);
    //wprintw(main_screen, "Current Room Num: %d\n\n", cur_room_num);

    pthread_mutex_lock(&room_mutex);
    if (room_list.find(conn->room_num) != room_list.end()) { // the connecting room is already in the list, skip. Some servers continually send connecting room messages and this avoids cluttering the screen with duplicates
        pthread_mutex_unlock(&room_mutex);
        free(conn->description);
        free(conn);
        return;
    }
    pthread_mutex_unlock(&room_mutex);

    update_map(cur_room_name, conn->room_name); // update the map using the current room and the connection received

    // add the room to the connected rooms list
    pthread_mutex_lock(&room_mutex);
    room_list.insert(std::make_pair((int)conn->room_num, std::string(conn->room_name)));
    pthread_mutex_unlock(&room_mutex);

    if (newline) {
        wprintw(main_screen, "\n");
        newline = false;
    }

    wattron(main_screen, COLOR_PAIR(4) | A_BOLD);
    wprintw(main_screen, "Connecting Room #%d: %s\n", conn->room_num, conn->room_name);
    wattroff(main_screen, COLOR_PAIR(4) | A_BOLD);
    //wprintw(main_screen, "Description length: %d\n", desc_len);
    if (seen_connections.find(conn->room_num) == seen_connections.end()) { // if the client has not been shown this connection's description before
        wprintw(main_screen, "Description: %s\n\n", std::string(conn->description).c_str());
        seen_connections.insert(conn->room_num); // add it to the set
    }
    wprintw(main_screen, "\n");
    free(conn->description);
    free(conn);
}

void handle_version() {
    version *v = receive_version(skt);
    wprintw(main_screen, "Lurk Server running Lurk Version %u.%u\n", v->major, v->minor);
    //wprintw(main_screen, "Extensions Size: %lu\n\n", extension_size);
    recv(skt, nullptr, v->exten_size, MSG_WAITALL); // ignore the extensions, if there are any
    free(v);
}
