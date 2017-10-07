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
      logger->log("error", "", "SyncClass", t_transId, "path:[" +  destination + "] does not exist", "synchronize");
      return 0;
    } 
    
    
    // check if t_source and t_destination ends with / 
    if ( source[ source.length() - 1 ] != '/') {
      source.append("/");
    }
    if ( destination[ destination.length() - 1 ] != '/') {
      destination.append("/");
    }

    logger->log("info", "", "SyncClass", t_transId, "syncing:[" + destination + " with:[" + source + "]", "synchronize");
    std::string rsyncCmd = "rsync -alpti --delete " + 
                            source + " " + 
                            destination + 
                            " | awk '{ print $2 }'";
                            
    logger->log("debug", "", "SyncClass", t_transId, rsyncCmd , "synchronize");
    
    std::string output;
    std::stringstream output_ss, error_ss;
    int res = utility::exec( output, rsyncCmd );
    
    if (!res){
      utility::clean_string(output);
      logger->log("error", "", "SyncClass", t_transId, "failed to sync to [" +  destination +"]", "sync_task");
      logger->log("error", "", "SyncClass", t_transId, "CMDERROR:[" + output + "]", "sync_task");
      error_ss << "["+ utility::to_string(t_transId) + "] syncing:[" << destination << "] failed\n";
      error_ss << "CMDERROR:[" << output << "]\n\n";
      
    } else {
      logger->log("info", "", "SyncClass", t_transId, "destination:[" + destination + "] was synced successfully", "sync_task");
      output_ss << "syncing:[" << destination << "] was successful" << "\n";
      output_ss << output.substr(0, output.length()-1) << "\n";
    }
    
    
    /*
    // optional: print rysn putput and errors
    // print errors if they exist
    if (output_ss.str() != "") {
      if ( conf.EmailSynOutput == "yes" )
        utility::send_email("MasterSync Output", output_ss.str(), conf.SynOutputEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + output_ss.str(), "mastersync_task");
      
    }
    if (error_ss.str() != "") {
      if ( conf.EmailSynError == "yes" )
        utility::send_email("MasterSync Errors", error_ss.str(), conf.SynErrorEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "Errors for syncing:\n" + error_ss.str(), "mastersync_task");
    }

    syncing = false;
    */
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
      logger->log("error", "", "SyncClass", t_transId, "path:[" +  destination + "] does not exist", "synchronize");
      return 0;
    } 
    
    
    // check if t_source and t_destination ends with / 
    if ( source[ source.length() - 1 ] != '/') {
      source.append("/");
    }
    if ( destination[ destination.length() - 1 ] != '/') {
      destination.append("/");
    }

    logger->log("info", "", "SyncClass", t_transId, "syncing:[" + destination + "] with:[" + source + "]", "synchronize");
    std::string rsyncCmd = "rsync -alpti --delete " + 
                            source + " " + 
                            destination + 
                            " | awk '{ print $2 }'";
                            
    logger->log("debug", "", "SyncClass", t_transId, rsyncCmd , "synchronize");
    
    std::string output;
    std::stringstream output_ss, error_ss;
    int res = utility::exec( output, rsyncCmd );
    
    if (!res){
      utility::clean_string(output);
      logger->log("error", "", "SyncClass", t_transId, "failed to sync to [" +  destination +"]", "sync_task");
      logger->log("error", "", "SyncClass", t_transId, "CMDERROR:[" + output + "]", "sync_task");
      error_ss << "["+ utility::to_string(t_transId) + "] syncing:[" << destination << "] failed\n";
      error_ss << "CMDERROR:[" << output << "]\n\n";
      
    } else {
      logger->log("info", "", "SyncClass", t_transId, "destination:[" + destination + "] was synced successfully", "sync_task");
      output_ss << "syncing:[" << destination << "] was successful" << "\n";
      output_ss << output.substr(0, output.length()-1) << "\n";
    }
    
    std::cout << output_ss << std::endl;
    // once finish syncing, set the time 
    
    
    /*
    // optional: print rysn putput and errors
    // print errors if they exist
    if (output_ss.str() != "") {
      if ( conf.EmailSynOutput == "yes" )
        utility::send_email("MasterSync Output", output_ss.str(), conf.SynOutputEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + output_ss.str(), "mastersync_task");
      
    }
    if (error_ss.str() != "") {
      if ( conf.EmailSynError == "yes" )
        utility::send_email("MasterSync Errors", error_ss.str(), conf.SynErrorEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "Errors for syncing:\n" + error_ss.str(), "mastersync_task");
    }

    syncing = false;
    */
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
    std::cout << "Sync diff " << diff << "\n"; 
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
