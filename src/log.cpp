// taken from Van Nugterens rat-common

// include header file
#include "log.hh"

// code specific to (Rat) smg
namespace smg{

	// constructor
	Log::Log(){
	}

	// factory
	ShLogPr Log::create(){
		return std::make_shared<Log>();
	}

	// force set increment
	void Log::set_num_indent(const int num_indent){
		std::lock_guard<std::mutex> lock(mtx_);
		num_indent_ = num_indent;
	}

	// only change indent
	void Log::msg(const int incr){
		std::lock_guard<std::mutex> lock(mtx_);

		// increment, but never below zero (the cast used to wrap the comparison,
		// not num_indent_, so the guard was effectively always true)
		if(static_cast<int>(num_indent_) >= -incr)
			num_indent_+=incr;
	}

	// new line
	void Log::newl(){
		// lock for thread safety
		mtx_.lock();

		// enter
		std::printf(" \n");

		// unlock
		mtx_.unlock();
	}

	// access to indentation
	int Log::get_num_indent(){
		std::lock_guard<std::mutex> lock(mtx_);
		return num_indent_;
	}

}
