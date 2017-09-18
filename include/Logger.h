// Definition of the ServerSocket class

#ifndef Logger_Class
#define Logger_Class

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <stdio.h> // userd for popen()
#include <algorithm> // used for remove. for string_clean function
#include <ctime>  // used: logger, randomString
#include <cstring> // used: logger

class Logger
{
	private:
		std::ofstream myfile;
		std::string _filePath, buffer;
		std::stringstream ss;
		std::string get_date() const;
		bool _onScreen, _toFile;
		int _logLevel;
		
		enum _LogLevels {ERROR = 0, WARNING = 1, INFO = 2, DEBUG = 3};
		
  public:
		Logger();
		Logger(bool onScreen);
		Logger(bool onScreen, std::string file, int logLevel);
		~Logger();
	
		void set(bool onScreen, std::string file, int logLevel);
		void set_file(std::string path);
		void log(std::string loglevel, std::string hostname, std::string program, int transaction, std::string message, std::string subprogram = "");
		int get_int(std::string ll);
};


#endif
