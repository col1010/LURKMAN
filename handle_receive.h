void handle_message(bool &newline);
void handle_error(bool &newline);
void handle_accept(bool &newline);
void handle_room(bool &newline, uint16_t &cur_room_num, char* cur_room_name);
void handle_character(bool &newline, uint16_t cur_room_num, bool &current_player, uint16_t &old_gold, int16_t old_health);
void handle_game();
void handle_connection(bool &newline, char* cur_room_name, std::set<uint16_t> &seen_connections);
void handle_version();