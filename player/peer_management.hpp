extern "C" {
#include "udp.h"
}

//a structure used for synchronize multiple players
class peer_management
{
public:
	_udp_t clientskt[6];
	_udp_t serverskt;
	int current_frame[6], player_id, players;
	char buffer[20000];
	void init(int _player_id, int _players);
	
	void update_current_frame(int frame);
	int gap();
	void update_peers();
	void broadcast();
};
