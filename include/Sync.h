
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
  
  public:
    Sync ();
    ~Sync();
    
    
    static  int synchronize ( const std::string t_source, const std::string t_destination, 
                              const int t_transId, Logger *&logger);
};


#endif
