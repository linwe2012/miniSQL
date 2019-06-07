#ifndef INTERPRETER_H
#define INTERPRETER_H
#include"API.h"

class Interpreter {
public:
	Interpreter() {};
	~Interpreter() {};
	std::string file_name;
	std::string db_name;
	int interpreter(std::string s);
private:
	std::string splitWord(std::string s, int *pos);
};

#endif