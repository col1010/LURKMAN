#include<stdint.h>
#include<string>

#pragma pack(push, 1)

// define the unicode characters
#define SWORDS "\u2694" // swords unicode character
#define SHIELD "\u26E8" // shield unicode character
#define REGEN "\u2728"  // "regen" unicode character (sparkles)
#define HEART "\u2665"  // heart unicode character

#define CTRL(c) ((c) & 037) // used to detect when a user presses CTRL + a character

#define INITIAL_HEALTH 1000

#define MESSAGE_HEADER_SIZE 67
struct message {
    const uint8_t type = 1;
    uint16_t msg_len;
    char recipient[32];
    char sender[32];
    char* message;
}__attribute__((packed));

#define CHANGEROOM_HEADER_SIZE 3
struct changeroom {
    const uint8_t type = 2;
    uint16_t room_num;
}__attribute__((packed));

#define PVP_HEADER_SIZE 33
struct pvp {
    const uint8_t type = 4;
    char target[32];
}__attribute__((packed));

#define LOOT_HEADER_SIZE 33
struct loot {
    const uint8_t type = 5;
    char target[32];
}__attribute__((packed));

#define ERROR_HEADER_SIZE 4
struct error {
    const uint8_t type = 7;
    uint8_t err_code;
    uint16_t msg_len;
    char* message;
}__attribute__((packed));

#define ACCEPT_HEADER_SIZE 2
struct accept {
    const uint8_t type = 8;
    uint8_t accept_type;
}__attribute__((packed));

#define ROOM_HEADER_SIZE 37
struct room {
    const uint8_t type = 9;
    uint16_t room_num;
    char room_name[32];
    uint16_t desc_len;
    char* description;
}__attribute__((packed));

// define the player flags
#define ALIVE 0b10000000
#define JOIN_BATTLE 0b01000000
#define MONSTER 0b00100000
#define STARTED 0b00010000
#define READY 0b00001000

#define CHARACTER_HEADER_SIZE 48
struct character {
    const uint8_t type = 10;
    char name[32];
    uint8_t flags;
    uint16_t attack;
    uint16_t defense;
    uint16_t regen;
    int16_t health;
    uint16_t gold;
    uint16_t room_num;
    uint16_t desc_len;
    char* description;
    char* stat_string; // string with |-separated attack, defense, etc
}__attribute__((packed));

#define STAT_LIMIT 65535
#define INITIAL_STATS 1000

#define GAME_HEADER_SIZE 7
struct game {
    const uint8_t type = 11;
    uint16_t init_points;
    uint16_t stat_limit;
    uint16_t desc_len;
    char* description;
}__attribute__((packed));

#define CONNECTION_HEADER_SIZE 37
struct connection {
    const uint8_t type = 13;
    uint16_t room_num;
    char room_name[32];
    uint16_t desc_len;
    char* description;
}__attribute__((packed));

#define VERSION_HEADER_SIZE 5
struct version {
    const uint8_t type = 14;
    uint8_t major; // 2
    uint8_t minor; // 3
    uint16_t exten_size; // 0
    // list of extensions not implemented
}__attribute__((packed));

#pragma pack(pop)