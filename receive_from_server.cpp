#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "lurk_structs.h"
#include <stdio.h>

character* receive_character(int fd) {
    character *c = (character*) malloc(CHARACTER_HEADER_SIZE + sizeof(size_t) + sizeof(size_t)); // add size_t size for the char*
    size_t readlen = recv(fd, &c->name, CHARACTER_HEADER_SIZE - 1, MSG_WAITALL);
    c->name[31] = 0; // ensure the name sent in is null terminated
    c->description = (char*) malloc(c->desc_len + 1);
    c->description[c->desc_len] = 0; // null terminate the description
    //printf("Beginning to receive character from fd %d\n", fd);
    if (c->desc_len)
        recv(fd, c->description, c->desc_len, MSG_WAITALL);
    //printf("Done!\n");
    return c;
}

message* receive_message(int fd) {
    message *m = (message*) malloc(MESSAGE_HEADER_SIZE + sizeof(size_t)); // add a size_t for the char*
    recv(fd, &m->msg_len, MESSAGE_HEADER_SIZE - 1, MSG_WAITALL); // read everything but the actual message
    //m->sender[31] = 0;
    //m->recipient[31] = 0;
    m->message = (char*) malloc(m->msg_len + 1);
    m->message[m->msg_len] = 0; // null terminate the message
    //printf("received msg_len: %u, recip: %s, sender: %s, msg: %s\n", m->msg_len, m->recipient, m->sender, m->message);
    if (m->msg_len)
        recv(fd, m->message, m->msg_len, MSG_WAITALL);
    return m;
}

error* receive_error(int fd) {
    error *e = (error*) malloc(ERROR_HEADER_SIZE + sizeof(size_t));
    recv(fd, &e->err_code, ERROR_HEADER_SIZE - 1, MSG_WAITALL);
    e->message = (char*) malloc(e->msg_len + 1);
    e->message[e->msg_len] = 0;
    if (e->msg_len)
        recv(fd, e->message, e->msg_len, MSG_WAITALL);
    return e;
}

room* receive_room(int fd) {
    room *r = (room*) malloc(ROOM_HEADER_SIZE + sizeof(size_t));
    recv(fd, &r->room_num, ROOM_HEADER_SIZE - 1, MSG_WAITALL);
    r->description = (char*) malloc(r->desc_len + 1);
    r->description[r->desc_len] = 0;
    if (r->desc_len)
        recv(fd, r->description, r->desc_len, MSG_WAITALL);
    return r;
}

game* receive_game(int fd) {
    game *g = (game*) malloc(GAME_HEADER_SIZE + sizeof(size_t));
    recv(fd, &g->init_points, GAME_HEADER_SIZE - 1, MSG_WAITALL);
    g->description = (char*) malloc(g->desc_len + 1);
    g->description[g->desc_len] = 0;
    if (g->desc_len)
        recv(fd, g->description, g->desc_len, MSG_WAITALL);
    return g;
}

connection* receive_connection(int fd) {
    connection *c = (connection*) malloc(CONNECTION_HEADER_SIZE + sizeof(size_t));
    recv(fd, &c->room_num, CONNECTION_HEADER_SIZE - 1, MSG_WAITALL);
    c->description = (char*) malloc(c->desc_len + 1);
    c->description[c->desc_len] = 0;
    if (c->desc_len)
        recv(fd, c->description, c->desc_len, MSG_WAITALL);
    return c;
}

version* receive_version(int fd) {
    version *v = (version*) malloc(VERSION_HEADER_SIZE+ sizeof(size_t));
    recv(fd, &v->major, VERSION_HEADER_SIZE - 1, MSG_WAITALL);
    return v;
}
