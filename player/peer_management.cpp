#include "peer_management.hpp"

//initialization
void peer_management::init(int _player_id, int _players)
{
	players = _players;
	for (int i = 0; i < 6; ++i) {
		if (i == _player_id) continue;
		udp_client_init(&(clientskt[i]), "127.0.0.1", 8010 + i);
	}

	for (int i = 0; i < players; ++i)
		current_frame[i] = 0;
		
	player_id = _player_id;
	udp_server_init(&serverskt, "127.0.0.1", 8010 + player_id);
}
	
void peer_management::update_current_frame(int frame)
{
	if (frame > current_frame[player_id]) current_frame[player_id] = frame;
}
	
int peer_management::gap()
{
	int ret = 0;
	for (int i = 0; i < players; ++i)
		if (-current_frame[player_id] + current_frame[i] > ret) ret = -current_frame[player_id] + current_frame[i];
	return ret;
}

//receive the information about the current frame ID being displayed by other players.
void peer_management::update_peers()
{
	int size = 0, x, y;
	char* p;
	while (1) {
		if (_select(1, serverskt)) size = udp_receive(&serverskt, buffer);//non-blocked socket receive
		if (size < 0) puts("Error receiving peer information");
		if (size == 0) break;
		
		p = buffer;
		while (p < buffer + size) {
			x = *p;
			y = *(int*)(p + 1);
			if (current_frame[x] < y) current_frame[x] = y;
			p += 5;
		}
		size = 0;
	}
}
	
//tell all other players about the current frame ID being displayed by the player.
//the format of the message:
//+----------+----------+----------+----------+----------+
//|player_id |                     frame_id              |
//+----------+----------+----------+----------+----------+
void peer_management::broadcast()
{
	int size = 0;
	char* p;
	for (int i = 0; i < players; ++i) {
		if (i == player_id) continue;
		p = buffer;
		*p = (char)player_id;
		p++;
		*((int*)p) = current_frame[player_id];
		udp_send(&(clientskt[i]), buffer, 5);
	}
}	

