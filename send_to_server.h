bool send_message(int fd, const char* sender, const char* recipient, const char* message);
bool send_character(int fd, character* ch);
bool send_changeroom(int fd, uint16_t room_num);
bool send_pvp(int fd, char* target);
bool send_loot(int fd, char* target);