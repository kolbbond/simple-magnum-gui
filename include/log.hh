#pragma once
// picked up from Van Nugterens project-rat

// general headers
#include <cassert>
#include <memory>
#include <stdio.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <mutex>

// Terminal color definitions
// color definitions
#define KNRM "\x1B[0m"
#define KBLD "\033[1m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

// specific headers
#include "typedefs.hh"
#include "error.hh"

namespace guild {

// shared pointer definition for Logger
typedef std::shared_ptr<class Logger> ShLoggerPr;

// shared pointer definition for no-output log
//typedef std::shared_ptr<class NullLogger> ShNullLoggerPr;

// output types
//enum VerboseType {general,fmm};

// logging to the terminal
class Logger {
	// properties
protected:
	// number of indentations
	int num_indent_ = 0;

	// create a mutex
	std::mutex mtx_;

	// methods
public:
	// logo options
	enum LogoType { RAT, NONE };

	// constructor
	explicit Logger(LogoType logo = NONE);

	// factory
	static ShLoggerPr create(LogoType logo = NONE);

	// virtual destructor (obligatory)
	virtual ~Logger(){};

	// only change indent
	virtual void msg(const int incr);

	// new line
	virtual void newl();

	// horizontal line
	virtual void hline(const int width,
		const char ch = '=',
		const std::string& str1 = {},
		const std::string& str2 = {});

	// access to indentation
	virtual int get_num_indent();

	// set indentation
	virtual void set_num_indent(const int num_indent);

	// cancelled flag
	virtual bool is_cancelled() const {
		return false;
	}

	// logo
	void show_logo(LogoType logo);

	// guild logo in ascii art
	// geneguilded by: https://cloudapps.herokuapp.com/imagetoascii/
	void guild_logo(const int vmajor, const int vminor, const int vpatch);

	// send text to logbook
	virtual void msg(const char* fmt, ...) {
		// lock for thread safety
		mtx_.lock();

		// create indentation
		for(int i = 0; i < num_indent_; i++)
			std::printf(" ");

		// process arguments and output
		va_list arg;
		va_start(arg, fmt);
		std::vprintf(fmt, arg);
		va_end(arg);

		// unlock
		mtx_.unlock();
	}

	// send text to logbook and change indentation afterwards
	virtual void msg(const int incr, const char* fmt, ...) {
		// lock for thread safety
		mtx_.lock();

		// create indentation
		if(incr != 0)
			for(int i = 0; i < num_indent_; i++)
				std::printf(" ");

		// process arguments and output
		va_list arg;
		va_start(arg, fmt);
		std::vprintf(fmt, arg);
		va_end(arg);

		// increment indentation
		assert(static_cast<int>(num_indent_) >= -incr);
		if(static_cast<int>(num_indent_) >= -incr) num_indent_ += incr;

		// unlock
		mtx_.unlock();
	}
};

// null logger (no output)
// used as a placeholder when no log is present
/*
	class NullLogger: public Logger{
		// methods
		public:
			// constructor
			NullLogger(){};

			// factory
			static ShNullLoggerPr create(){
				return std::make_shared<NullLogger>();
			}

			// send text to logbook
			void msg(const char*, ...)override final{} 

			// send text to logbook and change indentation afterwards
			void msg(const int, const char*, ...)override final{}

			// only change indent
			void msg(const int)override final{}

			// new line
			void newl()override final{}

			// access to indentation
			int get_num_indent()override final{return 0;}
	};
    */

} // namespace guild
