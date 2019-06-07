#include"interpreter.h"
#include<string.h>
#include<iostream>

int Interpreter::interpreter(std::string s) {
	int start = 0;
	std::string word = splitWord(s, &start);

	if (strcmp(word.c_str(), "use") == 0) {
		db_name = splitWord(s,&start);
		if (db_name.empty()) {
			std::cout << "[syntax error] no database name!" << std::endl;
			return 0;
		}
	}
	else if (strcmp(word.c_str(), "create") == 0) {
		word = splitWord(s,&start);
		if (word.empty()) {
			std ::cout << "[syntax error]create failure!" << std::endl;
			return 0;
		}
		else if (strcmp(word.c_str(), "table") == 0) {
			std::string table_name = splitWord(s, &start);
			if (table_name.empty()) {
				std::cout << "[syntax error]no table name!" << std::endl;
				return 0;
			}

			word = splitWord(s, &start);


		}
	}
}

std::string Interpreter::splitWord(std::string s, int *pos) {
	int pos1 = *pos;

	while (s[pos1] != 0 && (s[pos1] == 9 || s[pos1] == 10 || s[pos1] == 32))
		pos1++;

	if (s[pos1] == 39 || s[pos1] == 40 || s[pos1] == 41 || s[pos1] == 44) {
		(*pos) = pos1 + 1;
		return s.substr(pos1, 1);
	}
	else {
		(*pos) = pos1;
		while (s[(*pos)] != 0 && s[(*pos)] != 10 && s[(*pos)] != 32 && s[(*pos)] != 40 && s[(*pos)] != 41 && s[(*pos)] != 44)
			(*pos)++;
		if ((*pos) != pos1)
			return s.substr(pos1, (*pos) - pos1);
		else return "";
	}
}