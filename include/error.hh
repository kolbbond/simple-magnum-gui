#pragma once

// general headers
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstring>

#include "typedefs.hh"

// filename macro for shortening the path
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// custom runtime_error implementation
// in order to include filename and line number
class guild_error: public std::runtime_error{
	private:
		std::string msg;
	public:
		// constructor
		guild_error(const std::string &arg, const char *file, const char *function, int line) :
		std::runtime_error(arg) {
			std::ostringstream o;
			o << KRED << KBLD << "error: " << KNRM << file << ":" << function << ":" << 
				KGRN << KBLD << line << KNRM << ": " << std::endl << " " KYEL << arg << KNRM;
			msg = o.str();
		}
		
		// destructor
		~guild_error() throw() {}
		const char *what() const throw() override{
			return msg.c_str();
		}
};

// macro for error
#define guild_throw_line(arg) throw guild_error(arg, __FILENAME__, __FUNCTION__, __LINE__);

