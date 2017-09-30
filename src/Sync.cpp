#include "Sync.h"

Sync::Sync( ){

};


Sync::~Sync() {
  
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
    logger->log("debug", "", "SyncClass", t_transId, "CMD:[rsync -avlpt --delete " + source + " " + destination + "]", "synchronize");
    
    /*
    std::string output;
    std::stringstream output_ss, error_ss;
    int res = utility::exec( output, "rsync -avlpt --delete " + targetfs + " " + mountPoint );
    
    if (!res){
      utility::clean_string(output);
      logger.log("error", hostname, "EBSSyncer", transactionId, "failed to sync to [" +  mountPoint +"]", "sync_task");
      logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "sync_task");
      error_ss << "["+ utility::to_string(transactionId) + "] syncing:[" << mountPoint << "] failed\n";
      error_ss << "CMDERROR:[" << output << "]\n\n";
      
    } else {
      logger.log("info", hostname, "EBSSyncer", transactionId, "destination:[" + mountPoint + "] was synced successfully", "sync_task");
      output_ss << "syncing:[" << mountPoint << "] was successful" << "\n";
      output_ss << output.substr(0, output.length()-1) << "\n";
    }
    
    
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

