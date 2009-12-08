#ifndef PACKET_QUEUE_H_
#define PACKET_QUEUE_H_
#include <inttypes.h>
#include <pthread.h>
#include <list>

class PacketQueue {
public:
	PacketQueue();
	~PacketQueue();
	void push(uint8_t* data);
	uint8_t* pop();
	size_t size();

private:
	std::list<uint8_t*> queue;
	pthread_mutex_t mutex;
};

#endif
