#include<string>
#include<ncurses.h>
#include<sys/socket.h>
#include<unistd.h>
#include<atomic>
#include<map>
#include<set>
#include<stdexcept>
#include "graphviz_local/gvc.h"
#include "lurk_structs.h"


extern int skt;
extern character player;
extern std::atomic<bool> exit_thread;
extern pthread_t thread;
extern std::map<std::string, character*> character_map;
extern std::set<std::string> visited_rooms;
extern pthread_mutex_t graph_mutex;
extern bool made_character;
extern pthread_mutex_t made_character_mutex;
extern Agraph_t* G;
extern GVC_t* gvc;


void exit_gracefully(int signal_num) { // send the LEAVE message, free all characters' descriptions, end curses, and exit. Called with either the SIGINT handler or !leave command

    char type = 12; // LEAVE
    send(skt, &type, 1, 0); // send the leave message
    close(skt);

    exit_thread = true;
    pthread_join(thread, 0);

    for (auto c : character_map) { // free each character description
        free(c.second->description);
        free(c.second->stat_string);
        free(c.second);
    }
    free(player.description); // free the user's description
    gvFreeLayout(gvc, G); // free the graphviz graph
	agclose(G);
    endwin(); // end curses terminal mode

    exit(EXIT_SUCCESS);
}

bool attempt_int_conversion(std::string str) { // attempt to convert a string to int
    try {
        stoi(str); // attempt to convert the integer
        return true;
    } catch (const std::out_of_range) {
        return false;
    } catch (...) {
        return false;
    }
}

void update_map(char* cur_room_name, char* connected_room_name) {
    char cur_tmp[32], connected_tmp[32]; // create temporary strings to avoid modifying the passed in values
    strcpy(cur_tmp, cur_room_name);
    strcpy(connected_tmp, connected_room_name);
    
    if (strlen(cur_tmp) > 15) { // if it's longer than 15 characters, truncate it to make the graph smaller
        cur_tmp[14] = '~';
        cur_tmp[15] = 0;
    }
    if (strlen(connected_tmp) > 15) { // if it's longer than 15 characters, truncate it to make the graph smaller
        connected_tmp[14] = '~';
        connected_tmp[15] = 0;
    }
    pthread_mutex_lock(&graph_mutex);
    Agnode_t *cur = agnode(G, cur_tmp, 1); // create a node for the current room, if it does not already exist
    Agnode_t *connected = agnode(G, connected_tmp, 1); // create a node for the connected room, if it does not already exist
    if (visited_rooms.find(std::string(connected_room_name)) == visited_rooms.end()) // if the connected room is not already one the user visited 
        Agedge_t *edge = agedge(G, cur, connected, (char*)"", 1); // connect the two rooms
    pthread_mutex_unlock(&graph_mutex);
}

bool check_made_character() { // check if the user has made a character. Thread safe
    bool tmp;
    pthread_mutex_lock(&made_character_mutex);
    tmp = made_character;
    pthread_mutex_unlock(&made_character_mutex);
    return tmp;
}