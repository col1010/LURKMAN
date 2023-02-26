struct character* receive_character(int fd);
message* receive_message(int fd);
error* receive_error(int fd);
room* receive_room(int fd);
game* receive_game(int fd);
connection* receive_connection(int fd);
version* receive_version(int fd);