
#ifndef SYNC_CLASS
#define SYNC_CLASS

#include <iostream>
#include <string>
#include <vector>
#include "Utils.h"
#include "Logger.h"


class Sync
{
  private:
    Logger *logger;
    std::string m_latestSync;
    std::string m_syncFile;
    std::string m_syncIntervals;
  public:
    Sync ( const std::string t_syncFile, const std::string t_syncIntervals );
    ~Sync();
    
    
    static  int synchronize ( const std::string t_source, const std::string t_destination, 
                              const int t_transId, Logger *&logger);
                              
    int synchronize ( const std::string t_source, const std::string t_destination, 
                      const int t_transId );                              
    std::string get_latest();
    std::string load();
    int timeToSync();
    void set_logger_att( bool toScreen, std::string logFile, int loglevel );
    int setSyncTime();
};


#endif
