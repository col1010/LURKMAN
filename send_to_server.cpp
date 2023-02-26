#include<string.h>
#include<unistd.h>
#include<map>
#include<vector>
#include<sys/types.h>
#include<sys/socket.h>
#include<time.h>
#include "lurk_structs.h"

bool send_message(int fd, const char* sender, const char* recipient, const char* msg) {
	message m;
	m.msg_len = strlen(msg) + 1;
	strncpy(m.recipient, recipient, 32);
    strncpy(m.sender, sender, 32);
    char buf[MESSAGE_HEADER_SIZE + m.msg_len];
    memcpy(buf, &m, MESSAGE_HEADER_SIZE);
    memcpy(buf + MESSAGE_HEADER_SIZE, msg, m.msg_len);
    size_t send_size = MESSAGE_HEADER_SIZE + m.msg_len;
    return send_size == send(fd, buf, send_size, MSG_DONTWAIT);
}

bool send_character(int fd, character* ch) {
    char buf[CHARACTER_HEADER_SIZE + ch->desc_len]; // create an appropriately sized buffer to send
    memcpy(buf, ch, CHARACTER_HEADER_SIZE); // copy everything but the description
    strncpy(buf + CHARACTER_HEADER_SIZE, ch->description, ch->desc_len); // copy the description and ensure the null terminator is copied correctly
    size_t send_size = CHARACTER_HEADER_SIZE + ch->desc_len;
    return send_size == send(fd, buf, send_size, MSG_DONTWAIT);
}

bool send_changeroom(int fd, uint16_t room_num) {
    changeroom cr;
    cr.room_num = room_num;
    return CHANGEROOM_HEADER_SIZE == send(fd, &cr, CHANGEROOM_HEADER_SIZE, MSG_DONTWAIT);
}

bool send_pvp(int fd, char* target) {
    pvp p;
    strncpy(p.target, target, 32);
    return PVP_HEADER_SIZE == send(fd, &p, PVP_HEADER_SIZE, MSG_DONTWAIT);
}

bool send_loot(int fd, char* target) {
    loot l;
    strncpy(l.target, target, 32);
    return LOOT_HEADER_SIZE == send(fd, &l, LOOT_HEADER_SIZE, MSG_DONTWAIT);
}