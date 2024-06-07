// taken from rat-common

// include header file
#include "log.hh"

// additional headers
//#include "extra.hh"

// code specific to Rat
namespace guild{

	// constructor
	Log::Log(const LogoType logo){
		show_logo(logo);
	}

	// factory
	ShLogPr Log::create(const LogoType logo){
		return std::make_shared<Log>(logo);
	}

	// force set increment
	void Log::set_num_indent(const int num_indent){
		num_indent_ = num_indent;
	}

	// only change indent
	void Log::msg(const int incr){
		// lock for thread safety
		mtx_.lock();

		// increment indentation
		assert(static_cast<int>(num_indent_)>=-incr);
		if(static_cast<int>(num_indent_>=-incr))
			num_indent_+=incr;

		// unlock
		mtx_.unlock();
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
		return num_indent_;
	}

	// horizontal line
	void Log::hline(const int width, const char ch, const std::string& str1, const std::string& str2){
		if(width<=0)return;
		msg("%s%c",str1.c_str(),ch);
		for(int i=0;i<width-1;i++){
			msg(0,"%c",ch);
		}
		msg(0,"%s\n",str2.c_str());
	}

	// show guild logo
	void Log::show_logo(const LogoType logo){
		switch(logo){
			case RAT:{guild_logo(0, 0, 0);break;}
			case NONE:{break;}
		}
	}

	// guild logo in ascii art
	// geneguilded by: https://cloudapps.herokuapp.com/imagetoascii/
	void Log::guild_logo(const int vmajor, const int vminor, const int vpatch){
        (void)vminor;
        (void)vmajor;
        (void)vpatch;

		newl();
		msg("    ..>.>>>771.171.                          \n");
		msg("                .>1>.   >11.                 \n");
		msg("                ...>71111#B7                 \n");
		msg("         .>7C568####DDD#E$D85C71>>.          \n");
		msg("      .79#DDDDDDDD#DDDDD#DDDD#%s<>%s>#87>.   .   \n",KGRN,KNRM);
		msg("    .C##DD#DDDDDDDDD##DDD#895CC77777CC27.>.  \n");
		msg("   .A##DDDDDDDDDDD###31>..1.       >7>       \n");
		msg("   CDDDDDDDDDDDDDD#A>     .>>                \n");
		msg("  >###DDDDDDDDDDDD#>       .1.               \n");
		msg("  >##DDDDDDDDDDDDD#1        .1.              \n");
		msg("  .#D#DDDDDDDDDDDDD#2>       .>>             \n");
		msg("  .5##DDDDDDDDDDDDDDD#7.      1.             \n");
		msg("   1##DDDDDDDDDDDDDD###4.    >9.             \n");
		msg("   .C#DDDDDDDDDDDDDDDDD#7.7>8C.              \n");
		msg("    .14#DD#DDDDDDDDDDDD#7>7..        %s%sRAT CODE%s\n",KBLD,KYEL,KNRM);
		msg("      .>75#DD#D##DD#D67.             %sv%s%s\n",KYEL,"0",KNRM);
		msg("         .>>>>111>>>.....     %sJ. van Nugteren%s\n",KYEL,KNRM);
		msg("===============================================\n");
		newl();
	}

}
