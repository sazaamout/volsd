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
   
  // if local, check if path exist
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


  int Sync::synchronize ( const std::string t_source, const std::string t_destination, 
                          const int t_transId, const int local, const std::string op ) {
       
    std::string source, destination, ip;
    source = t_source;
    destination = t_destination;

    // ------------------------------------------
    // 1. Select operation
    // ------------------------------------------
    if ( op == "SyncAll" ) {
      // check if path exist on the local idle disks, no need to do it for the remote since it is mounted
      // and we know it is there
      if (( ! utility::is_exist( destination.c_str() ) ) && (local) ) {
        logger->log("error", "", "volsd", t_transId, "path:[" +  destination + "] does not exist");
        return 0;
      }

      if (!local) {
        ip = destination.substr(0, destination.find(":"));
        destination = destination.substr(destination.find(":")+1);
      }
 
      // check if t_source and t_destination ends with /
      if ( source[ source.length() - 1 ] != '/') {
        source.append("/");
      }
      if ( destination[ destination.length() - 1 ] != '/') {
        destination.append("/");
      }

    } else if ( op == "SyncPath" ){

      // ~~~~~~~~~~~~~~~~~~~~~
      // check if source exist
      // ~~~~~~~~~~~~~~~~~~~~~
      if ( !utility::is_exist( source ) ){
        logger->log("error", "", "volsd", t_transId, "path:[" + source + "] does not exist");
        return 0;
      }
      
      if (!local) {
        ip = destination.substr(0, destination.find(":"));
        destination = destination.substr(destination.find(":")+1);
      }

      // ~~~~~~~~~~~~~~~~~~~~~
      // If Directory Do...
      // ~~~~~~~~~~~~~~~~~~~~~
      if ( utility::is_dir( source.c_str() ) ){
        // append the trailing '/' to both of them
        if ( source[ source.length() - 1 ] != '/') {
          source.append("/");
        }
        if ( destination[ destination.length() - 1 ] != '/') {
          destination.append("/");
        }
      }

      // ~~~~~~~~~~~~~~~~~~~~~
      // If File Do ...
      // ~~~~~~~~~~~~~~~~~~~~~
      if ( utility::is_file( source.c_str() ) ){
        // remove trailing '/' from source
        if ( source[ source.length() - 1 ] == '/') {
          source = source.substr(0, source.length()-1);
        }
        // remove the filename from the destination
        // example: rsync -alvpt /home/cde/file.txt /home/cde
        destination = destination.substr(0, destination.find_last_of("/"));
      }
    

    } else {
        logger->log("error", "", "volsd", t_transId, "unknown transaction");
        return 0;

    }


    logger->log("info", "", "volsd", t_transId, "Syncing From:[" + source + "] To:[" + destination + "]" );
    // ------------------------------------------
    // 2. do the rsync
    // ------------------------------------------
    std::string  rsyncCmd;
    if (local){
      rsyncCmd = "rsync -alpti --delete --exclude \"BKU/\" --exclude \"logs/*\" --delete --exclude \'*~\' --delete --exclude \"devl/\" --exclude \".git/\" --delete --exclude \"files/\" --delete --exclude  \"reports/\" --exclude \".svn/\" " +
                  source + " " + 
                  destination + 
                  " | awk '{ print $2 }'";
    }else {
      rsyncCmd = "rsync -alptie \"ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" --delete --rsync-path=\"[ ! -d " + destination + " ] && mkdir " + destination + ";  rsync\" --exclude \"BKU/\" --exclude \"logs/*\" --delete --exclude \'*~\' --delete --exclude \"devl/\" --exclude \".git/\" --delete --exclude \"files/\" --delete --exclude  \"reports/\" --exclude \".svn/\" " +
                 source + " " +
                 ip + ":" + destination +
                 " | awk '{ print $2 }'";
    }
    
    // ------------------------------------------
    // 3. catch the output
    // ------------------------------------------
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
