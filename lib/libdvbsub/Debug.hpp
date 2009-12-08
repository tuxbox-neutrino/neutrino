#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <cstdio>

class Debug {
public:
	Debug();
	~Debug();

	void set_level(int level);
	FILE* set_file(char* file);
	void print(int level, const char *fmt, ...);

	static const int ERROR = 0;
	static const int INFO = 1;
	static const int VERBOSE = 2;

private:
	char* file_;
	int level_;
	FILE* fp_;

};

extern Debug sub_debug;

#endif
