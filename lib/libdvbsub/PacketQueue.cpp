#include <cstdlib>
#include <pthread.h>
#include <list>
#include "Debug.hpp"
#include "PacketQueue.hpp"

PacketQueue::PacketQueue()
{
	pthread_mutex_init(&mutex, NULL);
}

PacketQueue::~PacketQueue()
{
	while (queue.begin() != queue.end()) {
		delete[] queue.front();
		queue.pop_front();
	}
}

void PacketQueue::push(uint8_t* data)
{
	pthread_mutex_lock(&mutex);

	queue.push_back(data);

	pthread_mutex_unlock(&mutex);
}

uint8_t* PacketQueue::pop()
{
	uint8_t* retval;

	pthread_mutex_lock(&mutex);

	retval = queue.front();
	queue.pop_front();

	pthread_mutex_unlock(&mutex);
	
	return retval;
}

size_t PacketQueue::size()
{
	return queue.size();
}

