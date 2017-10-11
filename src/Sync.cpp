#include "Sync.h"

Sync::Sync( const std::string t_syncFile, const std::string t_syncIntervals){
  
  m_syncFile      = t_syncFile;
  m_syncIntervals = t_syncIntervals;
  // read the latest sync date from file
  m_latestSync = load();
  
};


Sync::~Sync() {
  delete logger;
}

// =================================================================================================
// Function: Set Logger att
// =================================================================================================
void Sync::set_logger_att( bool toScreen, std::string logFile, int loglevel ) {
  logger = new Logger(toScreen, logFile, loglevel);
}


int Sync::synchronize ( const std::string t_source, 
                        const std::string t_destination, 
                        const int t_transId,
                        Logger *&logger
                      ) {
       
  std::string source, destination;
  source = t_source;
  destination = t_destination;
   
  // check if path exist
  if ( !utility::is_dir( destination.c_str() ) ) {
    logger->log("error", "", "volsd", t_transId, "path:[" +  destination + "] does not exist");
    return 0;
  } 
    
    
  // check if t_source and t_destination ends with / 
  if ( source[ source.length() - 1 ] != '/') {
    source.append("/");
  }
  if ( destination[ destination.length() - 1 ] != '/') {
    destination.append("/");
  }

  std::string rsyncCmd = "rsync -alpti --delete " + 
                          source + " " + 
                          destination + 
                          " | awk '{ print $2 }'";
  
  std::string output;
  std::stringstream output_ss, error_ss;
  int res = utility::exec( output, rsyncCmd );
    
  if (!res){
    // an error occure
    utility::clean_string(output);
    logger->log("info", "", "volsd", t_transId, "RSYNC: [" + destination + "] with:[" + source + "] failed ");      
    logger->log("info", "", "volsd", t_transId, "RSYNC ERROR: [" + output + "]"); 
    
  } else {
    // successful, show sync output
    logger->log("info", "", "volsd", t_transId, "RSYNC: [" + destination + "] with:[" + source + "] successful ");  
    if ( output != "" )
      logger->log("info", "", "volsd", t_transId, output); 
  }

  return 1;
}


int Sync::synchronize ( const std::string t_source, 
                        const std::string t_destination, 
                        const int t_transId
                      ) {
       
  std::string source, destination;
  source = t_source;
  destination = t_destination;
   
  // check if path exist
  if ( !utility::is_dir( destination.c_str() ) ) {
    logger->log("error", "", "volsd", t_transId, "path:[" +  destination + "] does not exist");
    return 0;
  } 
    
    
  // check if t_source and t_destination ends with / 
  if ( source[ source.length() - 1 ] != '/') {
    source.append("/");
  }
  if ( destination[ destination.length() - 1 ] != '/') {
    destination.append("/");
  }

  std::string rsyncCmd = "rsync -alpti --delete " + 
                          source + " " + 
                          destination + 
                          " | awk '{ print $2 }'";
  
  std::string output;
  std::stringstream output_ss, error_ss;
  int res = utility::exec( output, rsyncCmd );
    
  if (!res){
    // an error occure
    utility::clean_string(output);
    logger->log("info", "", "volsd", t_transId, "RSYNC: [" + destination + "] with:[" + source + "] failed ");      
    logger->log("info", "", "volsd", t_transId, "RSYNC ERROR: [" + output + "]"); 
    
  } else {
    // successful, show sync output
    logger->log("info", "", "volsd", t_transId, "RSYNC: [" + destination + "] with:[" + source + "] successful ");  
    if ( output != "" )
      logger->log("info", "", "volsd", t_transId, output); 
  }

  return 1;
}


  // -----------------------------------------------------------------------------------------------
  // Get Latest
  // -----------------------------------------------------------------------------------------------
  std::string Sync::get_latest() {
    return m_latestSync;
  }
  
  
  // -----------------------------------------------------------------------------------------------
  // Load
  // -----------------------------------------------------------------------------------------------
  std::string Sync::load() {
    
    // open m_syncFile and extract the timestamp
    std::ifstream myFile;
    myFile.open( m_syncFile.c_str() );
    
    if ( !myFile.is_open() ){
      return "654369275";
    }
    
    
    if ( utility::is_empty( m_syncFile ) ){
      return "654369275";
    }
    
    
    std::string line;
    std::getline(myFile, line);
    
    myFile.close();
    myFile.clear();
    
    
    return line;
  }
  

  // -----------------------------------------------------------------------------------------------
  // Load
  // -----------------------------------------------------------------------------------------------
  int Sync::timeToSync() {
    // get the current time 
    std::string current_timestamp = utility::unixTime();
  
    // get the creation date of the latest snapshot
    std::string date = get_latest();
  
    int diff = stol(date) - stol(current_timestamp);
  
    if (diff < 0) diff = diff * (-1);
    if ( diff < stol(m_syncIntervals)*60 ){
      return 0;
    } else {
      return 1;
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Load
  // -----------------------------------------------------------------------------------------------
  int Sync::setSyncTime() {
    // get the current time 
    m_latestSync = utility::unixTime();
  
    // open the file and write the date
    std::ofstream myFileOut;
    myFileOut.open(m_syncFile.c_str(), std::fstream::out | std::fstream::trunc);

    if (!myFileOut.is_open()) {
      logger->log("error", "", "volsd", 4, 
                  "could not open volumes file", 
                  "setSyncTime");
      return 0;
    }

    myFileOut << m_latestSync;
  
    
  
    myFileOut.close();
    myFileOut.clear();
  
    return 1;
  }
