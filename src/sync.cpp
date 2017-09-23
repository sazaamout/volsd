#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

#include "ServerSocket.h"
#include "SocketException.h"

#include "Disks.h"
#include "Utils.h"
#include "Logger.h"

using namespace utility;
using namespace std;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 GLOBALS VARAIBLES AND STRUCTURES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */ 
  std::mutex m;

  std::string instance_id;
  std::string hostname; 
  utility::Configuration conf;
  bool _onscreen = false;
  std::string _conffile;
  int _loglevel;

  bool mastersync, syncing, pushing, deleting;


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  void mastersync_task(int transactionId);
  void push_task(int transactionId, std::string request);
  void delete_task(int transactionId, std::string request);
  void sync_task(int transactionId, std::string request);
  int parse_arguments(int argc, char* argv[]);
  std::string get_latest(int transactionId, Logger& logger);
  bool masterSyncForce = false;
  
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 MAIN PROGRAM
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
int main(int argc, char* argv[])
{
  
  
  if (!parse_arguments(argc, argv)){
    std:: cout << "must specify conf file location\n";
    return 1;
  }
  
  if (!utility::is_root()){
    std:: cout << "program must be ran as root\n";
    return 1;
  }
  
  
  // load the configurations 
  if (!utility::load_configuration(conf, _conffile)){
    std::cout << "failed to load configurations\n";
    return 1;
  }
  
  // this is wrong, why am i using the Dispatcher log level?
  _loglevel = conf.DispatcherLoglevel;
  Logger logger(_onscreen, conf.SyncerLogFile, _loglevel);

  // get this instance id
  //instance_id = utility::get_instance_id();  
  hostname    = utility::get_hostname();
  
  // start the mastersyncer thread
  std::thread mastersync_thread(mastersync_task, 0);
  mastersync_thread.detach();
  
  
  
  std::string request, cip, msg;
  int transId;

  
  try {
    // Create the server socket to listen on the SyncerPort
    ServerSocket syncerSocket ( conf.SyncerServicePort );
    
    while ( true ) 
    {
      transId = utility::get_transaction_id();
      logger.log("debug", hostname, "EBSSyncer", 0, "waiting for requests from clients ...");
      
      // create the client socket
      ServerSocket clientSocket;
      
      syncerSocket.accept ( clientSocket );
      
      // get the client ip
      cip = syncerSocket.client_ip();

      logger.log("info", hostname, "EBSSyncer", transId, "incoming connection accepted from " + cip);
      
      // reads the client request
      try {
        while ( true ) {
          clientSocket >> request;
          
          utility::clean_string(request);
          logger.log("info", hostname, "EBSSyncer", transId, "request:[" + request + "] from client:[" + cip + "]");
          
          if ( request.find("push") != std::string::npos ){
            
            clientSocket << "recived\n";

            std::thread push_thread(push_task, transId, request);
            push_thread.detach();
            
            clientSocket.close_socket();
            break;

          }else if ( request.find("delete") != std::string::npos ) {
          
            clientSocket << "recived\n";

            std::thread delete_thread(delete_task, transId, request);
            delete_thread.detach();
            
                      
            clientSocket.close_socket();
            break;
        
          }else if ( request == "mastersync" ) {
            // this means that dont wait, sync immedialty instead of waiting for the next time 
            masterSyncForce = true;
            clientSocket << "recived\n";
            
            std::thread mastersync_thread(mastersync_task, transId);
            mastersync_thread.detach();
            
                      
            clientSocket.close_socket();
            break;
            
          }else if ( request.find("sync") != std::string::npos ) {
          
            clientSocket << "recived\n";

            std::thread sync_thread(sync_task, transId, request);
            sync_thread.detach();
            
                      
            clientSocket.close_socket();
            break;
          }else {
            
            msg = "unknown request, shutting down connection\n";
            logger.log("error", hostname, "EBSSyncer", transId, "unknown request:[" + request + "], shutting down connection");
            clientSocket << msg;
            break;
          }
        } // end while
      } // end try
      catch ( SocketException& e) {
        logger.log("error", hostname, "EBSSyncer", transId, "[clientSocket] Exception was caught: " + e.description());
      }
    } // end of outer while
  } // end outer try
  catch ( SocketException& e ) {
    logger.log("error", hostname, "EBSSyncer", transId, "[syncerSocket] Exception was caught: " + e.description());
  }
    
  
  
  return 0;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION 
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  
  /* ---------------------------------------------------------------------------
     MASTERSYNC_TASK FUNCTION
     Description: This function is called priodically to sync all local and remote 
              disks to the taerget filesystem.
    -------------------------------------------------------------------------- */
  void mastersync_task(int transactionId){
    
    
    mastersync = true;
    
    int sleep_time = conf.SyncFrequency*60;
    
    // check when is the last time a sync was done.
    // if a sync was done 
    
    Logger logger(_onscreen, conf.SyncerLogFile, _loglevel);
    
    int diff;
    
    while (true) {
      std::string latest_datetime = get_latest(transactionId, logger);
      std::string current_datetime = utility::datetime();
      
      utility::clean_string(latest_datetime);
      utility::clean_string(current_datetime);

      // there is a very strange behaviour here. Sometime times is -3590 instead of few secodns. 
      diff = utility::datetime_diff(latest_datetime, current_datetime);
      
      if (diff < 0) diff = diff *(-1);

      logger.log("debug", hostname, "EBSSyncer", transactionId, 
            "LATEST-SYNC-TIME:[" + latest_datetime + 
            "], CURRENT-TIME:[" + current_datetime +
            "], TIME-DIFF:[" + utility::to_string(diff) +  
            "]", "mastersync_task"
            );

      
      if ( (diff < (conf.SyncFrequency) * 60) && (!masterSyncForce) ){
        logger.log("info", hostname, "EBSSyncer", transactionId, "no need to sync, latest was done ", "mastersync_task");
        logger.log("debug", hostname, "EBSSyncer", transactionId, "SLEEP-FOR:[" + utility::to_string((conf.SyncFrequency*60) - diff) + "]", "mastersync_task");
        mastersync = false;
        sleep_time = (conf.SyncFrequency*60) - diff;
        sleep(sleep_time);
        
      } else {
        
        masterSyncForce = false;
        
        logger.log("info", hostname, "EBSSyncer", transactionId, "Syncing all... ", "mastersync_task");
        
        std::ofstream myFileOut;
        myFileOut.open(conf.SyncDatesFile.c_str(), std::fstream::out | std::fstream::trunc);

        if (!myFileOut.is_open()) {
          logger.log("error", hostname, "EBSSyncer", transactionId, "could not open disk file", "mastersync_task");
        }
        
        myFileOut << current_datetime << "\n";
        myFileOut.close();
        myFileOut.clear();
        
        
        std::string source = conf.TargetFilesystemMountPoint;
        std::stringstream output_ss, error_ss;
        
        // ensuring the soutce does not have trailing '/'
        if ( source[source.length()-1] != '/' )
          source.append("/");
        
        std::string o, mp, att, output;
        
        
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
        // 1) sync path to local filesystem
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        std::stringstream l_ss( Disks::ebsvolume_list("local", conf.VolumeFilePath) );

          
        for (int ii = 0; l_ss >> mp; ii++)
        {
          if ( mp[mp.length()-1] != '/' )
            mp.append("/");
          logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[rsync -avlpt --delete " + source + " " + mp + "]", "mastersync_task");
          int res = utility::exec( output, 
                       "rsync -avlpt --delete " + 
                       source + " " + mp
                       );

          if (!res){
            utility::clean_string(output);
            logger.log("error", hostname, "EBSSyncer", transactionId, "failed to sync to [" +  mp +"]", "mastersync_task");
            logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "mastersync_task");
            error_ss << "["+ utility::to_string(transactionId) + "] syncing:[" << mp << "] failed\n";
            error_ss << "CMDERROR:[" << output << "]\n\n";
          } else {
            logger.log("info", hostname, "EBSSyncer", transactionId, "syncing:[" + mp + "] was successful", "mastersync_task");
            output_ss << "syncing:[" << mp << "] was successful" << "\n";
            output_ss << output.substr(0, output.length()-1) << "\n";
            
          }
          output.clear();
        }
        output.clear();
          
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
        // 2) sync path to remote filesystem
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        std::stringstream r_ss( Disks::ebsvolume_list("remote", conf.VolumeFilePath) );
        for (int ii = 0; r_ss >> att; ii++)
        {
          if (att != "none") {
            logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[rsync -avlpt -e \"ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" --delete " + source + " " + att + ":/home/cde/]", "mastersync_task");
            int res = utility::exec( output, 
                         "rsync -avlpt -e \"ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" --delete " + 
                         source + " " + att + ":/home/cde/"
                         );

            if (!res){
              utility::clean_string(output);
              logger.log("error", hostname, "EBSSyncer", transactionId, "failed to sync to [" +  att + ":/home/cde/]", "mastersync_task");
              logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "mastersync_task");
              error_ss << "["+ utility::to_string(transactionId) + "] syncing:[" << att << ":/home/cde/] failed\n";
              error_ss << "CMDERROR:[" << output << "]\n\n";
            } else {
              logger.log("info", hostname, "EBSSyncer", transactionId, "syncing:[" + att + ":/home/cde/] was successful", "mastersync_task");
              output_ss << "syncing:[" << att << ":/home/cde/] was successful" << "\n";
              output_ss << output.substr(0, output.length()-1) << "\n";
              
            }
            output.clear();
          } 
        }
              
        logger.log("info", hostname, "EBSSyncer", transactionId, "syncing is done", "mastersync_task");
        
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
        sleep_time = conf.SyncFrequency*60;
        sleep(sleep_time);
      }  
    }
    
    
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 3) sleep for SyncFrequency
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    mastersync = false;
  }
   
  /* ---------------------------------------------------------------------------
   PUSH_TASK
   * push a path to all local and remote disks
   -------------------------------------------------------------------------- */
  void push_task(int transactionId, std::string request){

    pushing  = true;
      
    Logger logger(_onscreen, conf.SyncerLogFile, _loglevel);
    
    // check if a path was supplied
    if ( request.find("/") == std::string::npos ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "no path was supplied", "push_task");
      return;
    }
    std::string path = request.substr(request.find("/"));
    std::string source = "/home/cde";
    std::string output, mp, att;
    std::stringstream output_ss, error_ss;
    
    logger.log("info", hostname, "EBSSyncer", transactionId, "request to push the path:[" + path + "]", "push_task");
    logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[/home/cde/saad/code/DiskManager/bin/local-push.sh " + path + " " + conf.TargetFilesystemMountPoint + " " + source + "]", "push_task");
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 1) sync path to target Filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    int res = utility::exec( output, 
                 "/home/cde/saad/code/DiskManager/bin/local-push.sh " +   
                 path + " " + conf.TargetFilesystemMountPoint + " " + 
                 source
                 );
                
    if (!res){
      utility::clean_string(output);
      logger.log("error", hostname, "EBSSyncer", transactionId, "pushing to target filesystem:[" + conf.TargetFilesystemMountPoint + "] failed", "push_task");
      logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "push_task");
      error_ss << "["+ utility::to_string(transactionId) + "] pushing path:[" << path << "] to target filesystem:[" << conf.TargetFilesystemMountPoint << "] failed\n";
      error_ss << "CMDERROR:[" << output << "]\n\n";
    } else {
      logger.log("info", hostname, "EBSSyncer", transactionId, "push to target filesystem:[" + conf.TargetFilesystemMountPoint + "] was successful", "push_task");
      // we cannot use utility::clean_string becoue this function will remove all of the '\n' from the varaible
      // while we want the format to be same.
      output_ss << output.substr(0, output.length()-1);
      //std::cout << "\nOutput:\n" << output_ss.str() << "\n";
    }
    output.clear();
      
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 2) sync path to local filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    std::stringstream l_ss( Disks::ebsvolume_list("local", conf.VolumeFilePath) );

    for (int ii = 0; l_ss >> mp; ii++){
      logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[/home/cde/saad/code/DiskManager/bin/local-push.sh " + path + " " + mp + " " + source + "]", "push_task");
      int res = utility::exec( output, 
                   "/home/cde/saad/code/DiskManager/bin/local-push.sh " + 
                   path + " " + mp + " " + source
                   );
      if (!res){
        utility::clean_string(output);
        logger.log("error", hostname, "EBSSyncer", transactionId, "pushing failed for:[" +  mp +"]", "push_task");
        logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "push_task");
        error_ss << "["+ utility::to_string(transactionId) + "] pushing path:[" << path << "] to:[" << mp << "] failed\n";
        error_ss << "CMDERROR:[" << output << "]\n\n";
      } else {
        logger.log("info", hostname, "EBSSyncer", transactionId, "push destination:[" + mp + "] was successful", "push_task");
        output_ss << output.substr(0, output.length()-1);
        //std::cout << "\nOutput:\n" << output_ss.str() << "\n";
      }
      output.clear();
    }
    output.clear();
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 3) sync path to remote filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    std::stringstream g_ss( Disks::ebsvolume_list("remote", conf.VolumeFilePath) );
    
    for (int ii = 0; g_ss >> att; ii++){
      logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[/home/cde/saad/code/DiskManager/bin/remote-push.sh " + path + " " + att + "]", "push_task");
      int res = utility::exec( output, 
                   "/home/cde/saad/code/DiskManager/bin/remote-push.sh " + 
                   path + " " + att 
                   );
      if (!res){
        utility::clean_string(output);
        logger.log("error", hostname, "EBSSyncer", transactionId, "puhsing failed for:[" +  att +"]", "push_task");
        logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "push_task");
        error_ss << "["+ utility::to_string(transactionId) + "] pushing path:[" << path << "] to:[" << att << "] failed\n";
        error_ss << "CMDERROR:[" << output << "]\n";
        //std::cout << "\nOutput to email:\n" << error_ss.str() << "\n\n";
      } else {
        logger.log("info", hostname, "EBSSyncer", transactionId, "push to remote server:[" + att + ":/home/cde] was successful", "push_task");
        output_ss << output.substr(0, output.length()-1);
        //std::cout << "\nOutput:\n" << output_ss.str() << "\n";
      }
      output.clear();
    }  

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 4) if there was an error, email it.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (error_ss.str() != ""){
      if ( conf.EmailPushError == "yes" )
        utility::send_email("Push errors", error_ss.str(), conf.EmailPushEmail);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + error_ss.str(), "push_task");
    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 5) if debug level is enabled, shows output in log file
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (output_ss.str() != ""){
      if ( conf.EmailPushOutput == "yes" )
        utility::send_email("Push output", output_ss.str(), conf.EmailPushEmail);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + output_ss.str(), "push_task");
    }
    
        
    pushing  = false;
  }


  /* ---------------------------------------------------------------------------
   DELETE_TASK
   -------------------------------------------------------------------------- */
  void delete_task(int transactionId, std::string request){
    deleting = true;
    
    Logger logger(_onscreen, conf.SyncerLogFile, _loglevel);
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 1) Checks and data gather
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // extract the full path and the subpath
    if ( request.find("/") == std::string::npos ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "no path was supplied", "delete_task");
      return;
    }
    
    // check to see if string length is less than 24 length('delete /home/cde/htdocs/')
    if ( request.length() < 24 ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "path was incorrect", "delete_task");
      return;
    }
    std::string fullPath = request.substr(request.find("/"));
    std::string subPath = fullPath.substr(fullPath.find("htdocs"));
    
    // check if TargetFilesystemMountPoint ends with / 
    std::string targetfs = conf.TargetFilesystemMountPoint;
    if ( targetfs[ targetfs.length() - 1 ] != '/') {
      targetfs.append("/");
    }
    
    // check to see if path does not contains '/home/cde/'
    if ( fullPath.find("/home/cde/htdocs") == std::string::npos ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "path must be inside '/home/cde/htdocs'", "delete_task");
      return;
    }
    
    // check if the requst is to delete the root
    if ( ( fullPath == "/home/cde/htdocs" ) || ( fullPath == "/home/cde/htdocs/" ) ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "cannot delete the root directory", "delete_task");
      return;
    }
    
    
    std::string source = "/home/cde";
    std::string output, mp, att;
    std::stringstream output_ss, error_ss;
    
    logger.log("info", hostname, "EBSSyncer", transactionId, "request to delete the path:[" + fullPath + "]", "delete_task");
    logger.log("debug", hostname, "EBSSyncer", transactionId, "FullPath:[" + fullPath + "], SubPath:[" + subPath + "]", "delete_task");
      
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 2) Delete path to target Filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // check and see if path is a file or directory
    if (is_file(fullPath.c_str())) {
      // now check if file exist in the target.
      logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if file:[" + targetfs + subPath + "] exist", "delete_task");
      if ( is_exist(targetfs + subPath) ) {
        logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing file:[" + targetfs + subPath + "]", "delete_task");
        
        logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[rm -f " + targetfs + subPath + "]", "delete_task");
        int res = utility::exec( output, "echo 'y' | rm  " + targetfs + subPath);
        if (!res){
          utility::clean_string(output);
          logger.log("error", hostname, "EBSSyncer", transactionId, "deleting file:[" + targetfs + subPath + "] failed", "delete_task");
          logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
          error_ss << "["+ utility::to_string(transactionId) + "] deleting file:[" << targetfs + subPath << "] failed\n";
          error_ss << "CMDERROR:[" << output << "]\n";
        } else {
          logger.log("info", hostname, "EBSSyncer", transactionId, "file:[" + targetfs + subPath + "] was deleted successfully", "delete_task");
          output_ss << "file:[" << targetfs << subPath << "] was deleted successfully" << "\n";
          output_ss << output.substr(0, output.length()-1);
        }  
      } else {
        logger.log("error", hostname, "EBSSyncer", transactionId, "File does not exist at the desintantion", "delete_task");
        
      }
      
    } else if (is_dir(fullPath.c_str())) {
      // now check if file exist in the target.
      
      logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if folder:[" + targetfs + subPath + "] exist", "delete_task");
      if ( is_exist(targetfs + subPath) ) {
        
        logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing folder:[" + targetfs + subPath + "]", "delete_task");
        logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[rm -rf " + targetfs + subPath + "]", "delete_task");
        
        int res = utility::exec( output, "rm -rf " + targetfs + subPath);
        if (!res){
          utility::clean_string(output);
          logger.log("error", hostname, "EBSSyncer", transactionId, "deleting folder:[" + targetfs + subPath + "] failed", "delete_task");
          logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
          error_ss << "["+ utility::to_string(transactionId) + "] deleting folder:[" << targetfs + subPath << "] failed\n";
          error_ss << "CMDERROR:[" << output << "]\n";
        } else {
          logger.log("info", hostname, "EBSSyncer", transactionId, "folder:[" + targetfs + subPath + "] was deleted successfully", "delete_task");
          output_ss << "folder:[" << targetfs << subPath << "] was deleted successfully" << "\n";
          output_ss << output.substr(0, output.length()-1);
        }
      } else {
        logger.log("error", hostname, "EBSSyncer", transactionId, "folder does not exist at the desintantion", "delete_task");
        
      }
      
      

    } else{
      logger.log("error", hostname, "EBSSyncer", transactionId, "File or Directory does not exist", "delete_task");
    }
    output.clear();
                
    
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 3) sync path to local filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    std::stringstream l_ss( Disks::ebsvolume_list("local", conf.VolumeFilePath) );
    if (is_file(fullPath.c_str())) {
      for (int ii = 0; l_ss >> mp; ii++)
      {
        if ( mp[ mp.length() - 1 ] != '/') 
          mp.append("/");
            
        // now check if file exist in the target.
        logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if file:[" + mp + subPath + "] exist", "delete_task");
                    
        if ( is_exist(mp + subPath) ) 
        {
          logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing file:[" + mp + subPath + "]", "delete_task");
          logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[rm -f " + mp + subPath + "]", "delete_task");
          int res = utility::exec( output, "rm -f " + targetfs + subPath);
        
          if (!res){
            utility::clean_string(output);
            logger.log("error", hostname, "EBSSyncer", transactionId, "deleting file:[" + mp + subPath + "] failed", "delete_task");
            logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
            error_ss << "["+ utility::to_string(transactionId) + "] deleting file:[" << mp + subPath << "] failed\n";
            error_ss << "CMDERROR:[" << output << "]\n";
          } else {
            logger.log("info", hostname, "EBSSyncer", transactionId, "file:[" + mp + subPath + "] was deleted successfully", "delete_task");
            output_ss << "file:[" << mp << subPath << "] was deleted successfully" << "\n";
            output_ss << output.substr(0, output.length()-1);
          }
        } else {
          logger.log("error", hostname, "EBSSyncer", transactionId, "File does not exist at the desintantion", "delete_task");
          
        }
        
        output.clear();
      }
    } else if (is_dir(fullPath.c_str())) {
      for (int ii = 0; l_ss >> mp; ii++)
      {
        if ( mp[ mp.length() - 1 ] != '/') 
          mp.append("/");
            
        logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if folder:[" + mp + subPath + "] exist", "delete_task");
        // now check if file exist in the target.
        if ( is_exist(mp + subPath) ) 
        {

          logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing folder:[" + mp + subPath + "]", "delete_task");
          logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[rm -rf " + mp + subPath + "]", "delete_task");
          int res = utility::exec( output, "rm -rf " + targetfs + subPath);
        
          if (!res){
            utility::clean_string(output);
            logger.log("error", hostname, "EBSSyncer", transactionId, "deleting folder:[" + mp + subPath + "] failed", "delete_task");
            logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
            error_ss << "["+ utility::to_string(transactionId) + "] deleting folder:[" << mp + subPath << "] failed\n";
            error_ss << "CMDERROR:[" << output << "]\n";
          } else {
            logger.log("info", hostname, "EBSSyncer", transactionId, "folder:[" + mp + subPath + "] was deleted successfully", "delete_task");
            output_ss << "folder:[" << mp << subPath << "] was deleted successfully" << "\n";
            output_ss << output.substr(0, output.length()-1);
          }
        } else {
          logger.log("error", hostname, "EBSSyncer", transactionId, "folder does not exist at the desintantion", "delete_task");
          
        }
      
        
        output.clear();
      }
    } else {
      logger.log("error", hostname, "EBSSyncer", transactionId, "File or Directory does not exist", "delete_task");
    }
    
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 3) sync path to remote filesystem
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    std::stringstream r_ss( Disks::ebsvolume_list("remote", conf.VolumeFilePath) );
    if (is_file(fullPath.c_str())) {
      for (int ii = 0; r_ss >> att; ii++)
      {
        std::string ip = att;
        att.append(":/home/cde/");
        
        logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if file:[" + att + subPath + "] exist", "delete_task");
        // now check if file exist in the target.
        if ( is_exist("/home/cde/" + subPath, ip) ) 
        {
          logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing file:[" + att + subPath + "]", "delete_task");
          logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " + ip + " 'rm -f /home/cde/" + subPath + "']", "delete_task");
          int res = utility::exec( output, "ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " + ip + " 'rm -f /home/cde/" + subPath + "'");
        
          if (!res){
            utility::clean_string(output);
            logger.log("error", hostname, "EBSSyncer", transactionId, "deleting file:[" + att + subPath + "] failed", "delete_task");
            logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
            error_ss << "["+ utility::to_string(transactionId) + "] deleting file:[" << att + subPath << "] failed\n";
            error_ss << "CMDERROR:[" << output << "]\n";
          } else {
            logger.log("info", hostname, "EBSSyncer", transactionId, "file:[" + att + subPath + "] was deleted successfully", "delete_task");
            output_ss << output.substr(0, output.length()-1);
          }
        } else {
          logger.log("error", hostname, "EBSSyncer", transactionId, "File does not exist at the desintantion", "delete_task");
        }
  
        output.clear();
      }
    } else if (is_dir(fullPath.c_str())) {
      for (int ii = 0; r_ss >> att; ii++)
      {
        std::string ip = att;
        att.append(":/home/cde/");
        
        logger.log("debug", hostname, "EBSSyncer", transactionId, "checking if folder:[" + att + subPath + "] exist", "delete_task");
        // now check if file exist in the target.
        if ( is_exist("/home/cde/" + subPath, ip) ) 
        {
          logger.log("info", hostname, "EBSSyncer", transactionId, "deleteing folder:[" + att + subPath + "]", "delete_task");
          logger.log("debug", hostname, "EBSSyncer", transactionId, "EXECMD:[ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " + ip + " 'rm -rf /home/cde/" + subPath + "']", "delete_task");
          int res = utility::exec( output, "ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " + ip + " 'rm -rf /home/cde/" + subPath + "'");
        
          if (!res){
            utility::clean_string(output);
            logger.log("error", hostname, "EBSSyncer", transactionId, "deleting folder:[" + att + subPath + "] failed", "delete_task");
            logger.log("error", hostname, "EBSSyncer", transactionId, "CMDERROR:[" + output + "]", "delete_task");
            error_ss << "["+ utility::to_string(transactionId) + "] deleting folder:[" << att + subPath << "] failed\n";
            error_ss << "CMDERROR:[" << output << "]\n";
          } else {
            logger.log("info", hostname, "EBSSyncer", transactionId, "folder:[" + att + subPath + "] was deleted successfully", "delete_task");
            output_ss << output.substr(0, output.length()-1);
          }
        } else {
          logger.log("error", hostname, "EBSSyncer", transactionId, "folder does not exist at the desintantion", "delete_task");
        }
        
        output.clear();
      }
    } else {
      logger.log("error", hostname, "EBSSyncer", transactionId, "File or Directory does not exist", "delete_task");
    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 5) if there was an error, email it.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (error_ss.str() != ""){
      if ( conf.EmailSynError == "yes" )
        utility::send_email("Delete errors", error_ss.str(), conf.SynErrorEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + error_ss.str(), "delete_task");
    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    // 6) if debug level is enabled, shows output in log file
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (output_ss.str() != ""){
      if ( conf.EmailSynOutput == "yes" )
        utility::send_email("Delete output", output_ss.str(), conf.SynOutputEmailTo);
      logger.log("debug", hostname, "EBSSyncer", transactionId, "results for syncing:\n" + output_ss.str(), "delete_task");
    }
    deleting = false;
  }


  /* ---------------------------------------------------------------------------
   SYNC_TASK
   * Given a volume, sync it with targetVolume or 
   -------------------------------------------------------------------------- */
  void sync_task(int transactionId, std::string request){
    
    syncing = true;
    
    Logger logger(_onscreen, conf.SyncerLogFile, _loglevel);
    
    // extract the full patha and the subpath
    if ( request.find("/") == std::string::npos ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "no path was supplied", "sync_task");
      return;
    }
    std::string mountPoint = request.substr(request.find("/"));
      
    // ensure we are going to sync inside conf.TempMountPoint (ex. /mnt/diskManager/uY79xFw9ek) and not some where else
    if ( mountPoint.find(conf.TempMountPoint) == std::string::npos ) {
      logger.log("error", hostname, "EBSSyncer", transactionId, "path must contains '" + conf.TempMountPoint + "'", "sync_task");
      return;
    }
    
    // check to see if we chosen a subdirectlry
    if ( mountPoint == conf.TempMountPoint) {
      logger.log("error", hostname, "EBSSyncer", transactionId, "path must contains '" + conf.TempMountPoint + "'", "sync_task");
      return;
    }
    
    // check if path exist
    if ( !utility::is_dir(mountPoint.c_str()) ) {
      logger.log("error", hostname, "EBSSyncer", transactionId, "path:[" +  mountPoint + "] does not exist", "sync_task");
      return;
    } 
    
    // check if TargetFilesystemMountPoint ends with / 
    std::string targetfs = conf.TargetFilesystemMountPoint;
    if ( targetfs[ targetfs.length() - 1 ] != '/') {
      targetfs.append("/");
    }
    
    if ( mountPoint[ mountPoint.length() - 1 ] != '/') {
      mountPoint.append("/");
    }

    
    logger.log("info", hostname, "EBSSyncer", transactionId, "request to sync:[" + mountPoint + " with:[" + targetfs + "]", "sync_task");
    logger.log("debug", hostname, "EBSSyncer", transactionId, "CMD:[rsync -avlpt --delete " + targetfs + " " + mountPoint + "]", "sync_task");
    
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
  }

  /* ---------------------------------------------------------------------------
   PARE_ARGUMENT
   -------------------------------------------------------------------------- */
  int parse_arguments(int argc, char* argv[]){
    std::string str;
    for (int i=0; i<argc; i++){
      str = argv[i];
      if (str == "--screen")
        _onscreen = true;
        
      if ( str.find("--conf") != std::string::npos )
        _conffile = str.substr(str.find("=")+1, str.find("\n"));
    }
    if (_conffile == "")
      return false;
      
    return true;
  }


  /* ---------------------------------------------------------------------------
   GET_LATEST
   -------------------------------------------------------------------------- */
  std::string get_latest(int transactionId, Logger& logger) {
    // open conf.SyncerFile and extract the time string
    std::ifstream myFile;
    myFile.open( conf.SyncDatesFile.c_str() );
    
    if ( !myFile.is_open() ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "could not open syncer file:[" + conf.SyncDatesFile + "]" , "get_latest");
      // sends any time to trigger creating a new sync
      return "1999-12-30 12:12:12";
    }
    
    if ( myFile.peek() == std::ifstream::traits_type::eof() ){
      logger.log("error", hostname, "EBSSyncer", transactionId, "file is empty" , "get_latest");
      return "1999-12-30 12:12:12";
    }
    
    std::string line;
    std::getline(myFile, line);
    //std::cout << "line:[" << line << "]\n";
    
    myFile.close();
    
    return line;
  }
  
  

       

  
