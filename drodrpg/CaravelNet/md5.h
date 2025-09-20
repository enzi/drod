#ifndef MD5_H
#define MD5_H

#include <string>
using std::string;

namespace MD5 {
	char* getChecksum(const string& text);
};

#endif
