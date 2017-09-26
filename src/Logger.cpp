#include "Logger.h"

using namespace utility;

Logger::Logger(bool onScreen, std::string file, int logLevel) {
  _onScreen = onScreen;
  _toFile   = true;
  _logLevel = logLevel;
  
  _filePath = file;
  
}


Logger::Logger(bool onScreen) {
  _onScreen = onScreen;
  _toFile   = false;
}

Logger::Logger() {

}

Logger::~Logger() { 
  myfile.close();
}

void Logger::set(bool onScreen, std::string file, int logLevel){
  _onScreen = onScreen;
  _toFile   = true;
  _logLevel = logLevel;
  _filePath = file;

}

void Logger::log(std::string loglevel, std::string hostname, std::string program, int transaction, std::string message, std::string subprogram ){
  
  myfile.open(_filePath.c_str(), std::ios::out | std::ios::app);
     
  //if (!myfile.is_open()){
  //  std::cout << "error: could not open log file\n";
  //}
  
  if ( get_int(loglevel) <=  _logLevel ) {
    // 1) write to file
    if (subprogram == "")
      myfile << get_date() << " " << hostname << " " << program << "[" << transaction << "]" <<" [" << loglevel << "]: " << message << "\n";
    else
      myfile << get_date() << " " << hostname << " " << program << "[" << transaction << "]" << "::" << subprogram << " [" << loglevel << "]: " << message << "\n";
      
    // 2) write to screen if enabled
    if (_onScreen) {
      if (subprogram == "")
        std::cout << get_date() << " " << hostname << " " << program << "[" << transaction << "]" <<" [" << loglevel << "]: " << message << "\n";
      else
        std::cout << get_date() << " " << hostname << " " << program << "[" << transaction << "]" << "::" << subprogram << " [" << loglevel << "]: " << message << "\n";
    }
  }
  
  std::cout.flush();
  myfile.close();
  myfile.clear();
}

 

std::string Logger::get_date() const{
  time_t now = time(0);
  char* date = ctime(&now);
  *std::remove(date, date+strlen(date), '\n') = '\0'; 
  std::string str(date);
  return str;
}


void Logger::set_file(std::string path) {
  _filePath = path;
}


int Logger::get_int(std::string ll){
  if ( ll == "error")
    return 0;
  if ( ll == "info")
    return 1;
  if ( ll == "debug")
    return 2;
  if ( ll == "verbos")
    return 3;
  
}
