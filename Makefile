all:
	g++ main.cpp receive_from_server.cpp send_to_server.cpp curses_control.cpp handle_receive.cpp helper_funcs.cpp -o lurkman -O3 -lncursesw -pthread -lgvc -lcgraph
debug:
	g++ main.cpp receive_from_server.cpp send_to_server.cpp curses_control.cpp handle_receive.cpp helper_funcs.cpp -g -o lurkman -O3 -lncursesw -pthread -lgvc -lcgraph


clean:
	rm -f lurkman
