#include <string>
#include <iostream>
#include <thread>
#include <signal.h>

#include "SocketException.h"
#include "ServerSocket.h"
#include "ClientSocket.h"
#include "Volumes.h"
#include "Snapshots.h"
#include "Utils.h"
#include "Logger.h"
#include "Config.h"


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// GLOBALS VARAIBLES AND STRUCTURES
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  utility::Port *portsArray;
  utility::Configuration conf;

  std::string hostname; 
  std::string instance_id;
  std::vector<std::string>  devices_list;

  std::string _conffile;
  int inProgressVolumes;
  // defauls
  
  int syncVolumes = 1; // default is true
  int syncPeriods = 60; // default is 60 min
  int _loglevel = 3;
  bool _onscreen = false;
  
  std::thread manager_thread;
  std::thread snapshotManager_thread;
  std::thread volumesDispatcher_thread;
  
  // graceful stop
  int sigterm     = 0;
  int signalCount = 0;
  int diskCreateOpPending  = 0;
  int diskRemoveOpPending  = 0;
  int diskAcquireOpPending = 0;
  int diskReleaseOpPending = 0;
  int snapshotsOpPending   = 0;
  int syncingOpPending     = 0;
  
  ServerSocket *server;
  
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FUNCTION PROTOTYPES
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  void populate_port_array();
  std::string get_ports();
  void print_ports();
  int get_available_port();
  void get_arguments( int argc, char **argv );
  int ensure_mounted(Volumes &volumes, Logger &logger);

  // thread related functions
  void volumesDispatcher_handler( Volumes &volumes, Snapshots& snapshots, Sync& sync );
  void clientDiskAquire_handler(int portNo, std::string request, std::string ip, Volumes &volumes, 
                                int transId);
  void clientDiskRelease_handler( Volumes &volumes, const std::string t_volumeId, 
                                  const int t_transactionId );
  void createDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId);
  void removeDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId);
  
  void createSnapshot_handler( Snapshots& snapshots );
  void removeSnapshot_handler( Snapshots& snapshots );
  
  void volumesSync_handler( Snapshots& s, Volumes& volumes, Sync& sync);
  //void SyncRequests_handler( Volumes &volumes );
  
  void signalHandler( int signum );
  
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MAIN PROGRAM
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
using namespace std;
using namespace utility;
  
int main ( int argc, char* argv[] ) {


  // user must be root
  if (!utility::is_root()){
    std::cout << "error: program must be ran as root\n";
    return 1;
  }

  // Default value is DISPATCHER_CONF_FILE, if use want to sepcify different one
  // he must pass the -c flage when executig the program. 
  // Ex: dispatcher -c /path/to/conf/file
  _conffile = DISPATCHER_CONF_FILE;
  get_arguments( argc, argv );
                                  
  // -------------------------------------------------------------------
  // Initializations  
  // -------------------------------------------------------------------
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 1. Setup the signals handler
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  signal (SIGTERM, signalHandler);
  signal (SIGINT, signalHandler);
  signal(SIGHUP, signalHandler); 
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 2. Load the configurations from conf file
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if ( !utility::load_configuration(conf, _conffile) ){
    std::cout << "error: cannot locate configuration file\n";
    return 1;
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 3 overwirte defaults
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  syncVolumes = ( conf.SyncVolumes == "yes") ? 1 : 0;
  syncPeriods = conf.SyncVolumesInterval;
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 4. Ensure files and directories are created
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  utility::folders_create( conf.DispatcherLogPrefix );
  utility::folders_create( conf.TempMountPoint );
  utility::folders_create( conf.TargetFilesystemMountPoint );
    
  if (!utility::file_create(conf.SnapshotFile)){
    std::cout << "error: failed to create snapshot file\n";
    return 1;
  }
  if (!utility::file_create(conf.VolumeFilePath)){
    std::cout << "error: failed to create volume file\n";
    return 1;
  }
  if (!utility::file_create(conf.SyncDatesFile)){
    std::cout << "error: failed to create sync file\n";
    return 1;
  }  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 5. Object Instantiation  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  _loglevel = conf.DispatcherLoglevel;
  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
  Snapshots snapshots( conf.SnapshotMaxNumber, 
                       conf.SnapshotFile, 
                       conf.SnapshotFrequency, 
                       conf.SnapshotFileStorage);
  snapshots.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  
  Volumes volumes( conf.TempMountPoint, conf.VolumeFilePath, conf.MaxIdleDisk );
  volumes.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  
  Sync sync( conf.SyncDatesFile, utility::to_string(conf.SyncVolumesInterval) );
  sync.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 6. ensure that volumes are mounted
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if ( !ensure_mounted(volumes, logger) ){
    std::cout << "error: target filesystem " 
              << conf.TargetFilesystemMountPoint 
              << " is not mounted\n";
    return 1;
  }
 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 7. get the hostname and the Amazon Instance Id for this machine
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  hostname    = utility::get_hostname();
  instance_id = utility::get_instance_id();

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 8. Populating ports array. 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // These ports are there to be able to communicate with multiple clients at once
  portsArray = new Port[10];
  populate_port_array();

  volumesDispatcher_thread = std::thread( volumesDispatcher_handler, 
                                          std::ref(volumes), 
                                          std::ref(snapshots), 
                                          std::ref(sync));
  
  // -------------------------------------------------------------------
  // Core Functionality
  // -------------------------------------------------------------------
  int value;
  int latestSyncDate;
  
  while ((true) && (!sigterm) ){

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // 1. maintain snapshots
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if ( ( snapshots.size() <  conf.SnapshotMaxNumber ) || (!snapshotsOpPending) ) {
      // check the time
      if ( snapshots.timeToSnapshot() ){
        snapshotsOpPending = 1;
        std::thread createSnapshot_thread( createSnapshot_handler, std::ref(snapshots) );
        createSnapshot_thread.detach();
      }
    }
        
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // 2. maintain volumes count
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    value = volumes.get_idle_number() + inProgressVolumes;
    if ( value < conf.MaxIdleDisk ){
      int tranId = utility::get_transaction_id();
      logger.log( "debug", "", "volsd", tranId, "creating a new volume" );  
      std::thread creatDisk_thread( createDisk_handler, 
                                    std::ref(snapshots), 
                                    std::ref(volumes), 
                                    tranId );
      creatDisk_thread.detach();
    }
    
    if ( value > conf.MaxIdleDisk ){
      int tranId = utility::get_transaction_id();
      logger.log( "debug", "", "volsd", tranId, "removing a volume" );  
      std::thread removeDisk_thread( removeDisk_handler, 
                                    std::ref(snapshots), 
                                    std::ref(volumes), 
                                    tranId );
      removeDisk_thread.detach();
    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // 3. Sync volumes Periodically
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if ( ( syncVolumes ) && (!syncingOpPending) ){
      // check the time
      if ( sync.timeToSync() ){
        syncingOpPending = 1;
        std::thread volumesSync_thread( volumesSync_handler, std::ref(snapshots), 
                                        std::ref(volumes),   std::ref(sync));
        volumesSync_thread.detach();  
      }
    }
  
    sleep(10);
  }
  
  
  return 1;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions Implementaion
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


  // -------------------------------------------------------------------
  // Volumes Dispatcher Handler Function
  // -------------------------------------------------------------------

  void volumesDispatcher_handler( Volumes &volumes, Snapshots& snapshots, Sync& sync){
    
    std::string request, ack, cip;
    std::string msg;
    int transId;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log("info", "", "volsd", 0, "volumes dispatcher started", "VD");
      
    try {
      ServerSocket server( 9000 ); 

      while (( true ) && ( !sigterm ))
      {
        
        transId = utility::get_transaction_id();
        logger.log("debug", "", "volsd", 0, "waiting for requests from clients ...", "VD");
        ServerSocket new_sock;
        
        // Accept the incoming connection
        server.accept ( new_sock );

        // client Ip address      
        cip = server.client_ip();

        logger.log("info", "", "volsd", transId, "incoming connection accepted from " + cip, "VD");
        
        try {
          while ( true ) 
          {
            
            // reads client request
            new_sock >> request;
            utility::clean_string(request);

            if ( request.compare("DiskRequest") == 0 ) {
              
              // the reply to the client will be a communication port. The new port is used as
              // communication channel between the dispatcher and the client.
              logger.log("info", "", "volsd", transId, "request:[" + request + "] from client:[" + 
                         cip + "]", "VD");
              int availablePort = get_available_port();
              new_sock << utility::to_string(availablePort);

              logger.log( "debug", "", "volsd", transId, 
                          "reserving port:[" + utility::to_string(availablePort) + "] to client:[" + 
                          cip + "]", "VD"
              );

              new_sock.close_socket();
              // start a new thread which will listen on the port sent to the client. 
              // this thread will handle the disk request
              thread t1( clientDiskAquire_handler, 
                         availablePort, 
                         request, 
                         cip, 
                         std::ref(volumes), 
                         transId
                       );
              t1.detach();
              break;

            } else if ( request.find("DiskRelease") != std::string::npos ) {
              
              // DiskRelease request format: "DiskRelease:volId"
              logger.log("info", "", "volsd", transId, "request:[" + request + "] from client:[" + 
                         cip + "]", "VD");
              std::string volId = request.substr( request.find(":")+1, request.find("\n") );  
            
              if ( (volId == "") || (volId == " ") ) {
                logger.log("error", "", "volsd", transId, 
                           "client did not supply volume id to release", "VD");
                new_sock.close_socket();
                break;
              }
              
              thread disk_release_thread( clientDiskRelease_handler, 
                                          std::ref(volumes), 
                                          volId, 
                                          transId);
              disk_release_thread.detach();
            
              new_sock.close_socket();
              break;
                  
            } else if ( request.find("SyncAll") != std::string::npos ) {
              
              logger.log("info", "", "volsd", transId, "request:[" + request + "] from client:[" + 
                         cip + "]", "VD");
              // start a new thread which will listen on the port sent to the client. 
              // this thread will handle the disk request
              syncingOpPending = 1;
              
              std::thread volumesSync_thread( volumesSync_handler, 
                                              std::ref(snapshots), 
                                              std::ref(volumes),   
                                              std::ref(sync));
              volumesSync_thread.detach();  
              new_sock.close_socket();
              break;
            } else {
              
              msg = "unknown request, shutting down connection\n";
              logger.log("error", "", "volsd", transId, "unknown request:[" + request + 
                         "], shutting down connection", "VD");
              new_sock << msg;
              new_sock.close_socket();
              break;
            }
          } // end while
        } catch ( SocketException& e) {
          logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description(), "VD");
        }
      } // end of outer while
    } catch ( SocketException& e ) {
      logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description(), "VD");
    }
    
    return;
  }
  
  
  // -------------------------------------------------------------------
  // Client Disk Acquire Handler Function
  // -------------------------------------------------------------------
  void clientDiskAquire_handler(int portNo, std::string request, std::string ip, Volumes& volumes, 
                         int transId) {
    
    diskAcquireOpPending = 1;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    logger.log("debug", "", "volsd", transId, "awaiting clinet to connect tp port:[" + 
               utility::to_string(portNo) + "]", "VA" );
    
    std::string msg, ack;
    
    ServerSocket server ( portNo );
    
    ServerSocket s;
    server.accept ( s );
    
    std::string volumeId;
    int res = volumes.release( volumeId, transId );
    if (res == -1) {
      s << "MaxDisksReached";
      portsArray[portNo-9000-1].status=false;
      diskAcquireOpPending = 0;
      return;
    }
      
    if (res == -2){
      s << "umountFailed";
      portsArray[portNo-9000-1].status=false;
      diskAcquireOpPending = 0;
      return;
    }
      
    if (res == -3){
      s << "detachFailed";
      portsArray[portNo-9000-1].status=false;
      diskAcquireOpPending = 0;
      return;
    }
    
    logger.log("info", "", "volsd", transId, "sending volume:[" + volumeId + "] to client", "VA");
    
    try {
      s << volumeId;
      logger.log("info", "", "volsd", transId, "waiting for ACK from Client", "VA");
      
      // Client response
      s >> ack;
      
    } catch ( SocketException& e) {
      // if something happens in the communication, Mark the disk to failed.
      //label disk as used
      
      // remove volumes from m_volumes
      volumes.remove(volumeId, transId);
      
      logger.log("info", "", "volsd", transId, "removing volume from anazone space", "VA");      
      if (!volumes.del(volumeId, transId)){
        logger.log("error", "", "volsd", transId, "volume failed to be removed", "VA");      
      }
      logger.log("info", "", "volsd", transId, "aborting ...", "VA");      
      
      ack = "FAILED";
      logger.log("error", "", "volsd", transId, "connection closed: " + e.description(), "VA" );      
      portsArray[portNo-9000-1].status=false;
      diskAcquireOpPending = 0;
      return;
    }
    //********* AT THIS POINT, Volume is unmounted and detach but Not delete from list ***********//
    // if client successfuly mounted filestsrem, then update disk status and remove mount point
    
    // message format: "status [mountpoint]"
    std::stringstream ss;
    std::string status, mountpoint;
    ss >> status >> mountpoint;
    
    if ( status == "OK"){
      logger.log("info", "", "volsd", transId, "ACK recived from client: [OK]", "VA" );      

      //label disk as used
      if ( !volumes.update(volumeId, "status", "used", transId, ip, mountpoint) ) {
        logger.log("info", "", "volsd", transId, "failed to update volumes status","VA");
      }
      
      logger.log("info", "", "volsd", transId, "disk:[" + volumeId +
                 "] is mounted on client machine", "VA");      
      
    } else {
      // Ideally, we want to put disk back, but for now, just delete the disk and the EBSmanager 
      // will create another one.
      logger.log("info", "", "volsd", transId, "ACK from clinent was [" + ack + "]", "VA");  

      // remove volumes from m_volumes
      volumes.remove(volumeId, transId);
      
      
      // this is commented out since we are going to have the client to remove the volumes
      //logger.log("info", "", "volsd", transId, "delete volume from Amazone list");  
      //if (!volumes.del(volumeId, transId)){
      //}
      //logger.log("info", "", "volsd", transId, "aborting ...");      
    
      portsArray[portNo-9000-1].status=false;
      diskAcquireOpPending = 0;
      return;
    }

    // labling port as not used
    portsArray[portNo-9000-1].status=false;
    
    diskAcquireOpPending = 0;
  }


  // -------------------------------------------------------------------
  // Client Disk Release Handler Function
  // -------------------------------------------------------------------
  void clientDiskRelease_handler(Volumes &volumes, const std::string t_volumeId, 
                                 const int t_transactionId ) {
    
    diskReleaseOpPending = 1;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
        
    //check if vol exist
    logger.log("info", "", "volsd", t_transactionId, 
               "get the volume:[" + t_volumeId + "] information", "VR");
    if (!volumes.volume_exist(t_volumeId) || (t_volumeId == "") || (t_volumeId == " ") ){
      logger.log("info", "", "volsd", t_transactionId, "volume was not found.", "VR");
      diskReleaseOpPending = 0;
      return;
    }
    
    // delete from aws 
    // ### this is the client program job now. We dont have to do it here.
    //logger.log("info", "", "volsd", transId, "delete volume from volumes list");  
    //volumes.del(volumeId, transId);
    
    // delete from list
    logger.log("info", "", "volsd", t_transactionId, "remove volume from volumes list", "VR" );  
    if ( !volumes.remove(t_volumeId, t_transactionId) ){
      logger.log("error", "", "volsd", t_transactionId, 
                 "failed to remove volume:[" + t_volumeId + "] from volumes list", "VR");
      diskReleaseOpPending = 0;
    } else {
      logger.log("info", "", "volsd", t_transactionId, "volume:[" + t_volumeId + 
              "] was removed from volumes list", "VR");
    }

    diskReleaseOpPending = 0;
  }
   

  // -------------------------------------------------------------------
  // Create Disk Handler Function
  // -------------------------------------------------------------------
  void createDisk_handler(Snapshots& snapshot, Volumes& volumes, const int t_transactionId){
    
    //this variables tells the termination signal handler function if there is operation going on
    diskCreateOpPending = 1;
    
    // set this varabile so that main wont create another thread to create new disk
    inProgressVolumes++;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
    std::string snapshotId;
    if (!snapshot.latest(snapshotId)){
      logger.log("info", "", "volsd", t_transactionId, "No snapshot was found.", "CV");
    }
  
     
    // all of this operation myst be done in Volumes
    if (!volumes.acquire( conf.TargetFilesystemMountPoint, snapshotId, conf.TempMountPoint, 
                          instance_id,  t_transactionId )) {
      logger.log("info", "", "volsd", t_transactionId, "faield to acquire new volume.", "CV");
    }
    
    inProgressVolumes--;   


    diskCreateOpPending = 0;
  }


  // -------------------------------------------------------------------
  // Remove Disk Handler Function
  // -------------------------------------------------------------------
  void removeDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId) {
    
    //this variables tells the termination signal handler function if there is operation going on
    diskRemoveOpPending = 1;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);

    
    // umount, detach, remove mountpoint    
    std::string volumeId;
    int res = volumes.release( volumeId, t_transactionId );
    
    // delete from amazon
    logger.log("debug", "", "volsd", t_transactionId, "deleting volume in aws enviroment", "RV");  
    volumes.del(volumeId, t_transactionId);
    
    // delete vols list volumes list (m_volumes)
    logger.log("info", "", "volsd", t_transactionId, "deleting volume from volumes list", "RV");  
    if ( !volumes.remove(volumeId, t_transactionId) ){
      logger.log("error", "", "volsd", t_transactionId, 
                 "failed to delete volume:[" + volumeId + "] from amazon site", "RV");
    }
    
    diskRemoveOpPending = 0;
  }
  
    
  // -------------------------------------------------------------------
  // Create Snapshots Handler Function
  // -------------------------------------------------------------------  
  void createSnapshot_handler( Snapshots& snapshots ) {
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    logger.log( "info", "", "volsd", 1, "create new snapshot", "CSS" );  
    if (!snapshots.create_snapshot(conf.TargetFilesystem, conf.SnapshotFrequency)){
      logger.log( "error", "", "volsd", 1, "could not create new snapshot", "CSS" );  
    }
    
    snapshotsOpPending = 0;
  }
  
  
  
  // -------------------------------------------------------------------
  // Volumes Sync Handler Function
  // -------------------------------------------------------------------
  void volumesSync_handler( Snapshots& s, Volumes& volumes , Sync& sync){
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    
    logger.log( "info", "", "volsd", 4, "Syncing all volumes with " + conf.TargetFilesystem, "VS" );  
    // get a reference to the volumes list. We are using a referece rather than getting a copy for 
    // the list simply because the list get updated all the time, and sync is a lengthy operation. 
    // A problem will arrise when a volumes got removed from the list and the synchronizer goes and 
    // sync it
    std::vector<utility::Volume> v = volumes.get_list();
    for(std::vector<utility::Volume>::iterator it = v.begin(); it != v.end(); ++it) {
      if ( it->attachedTo == "localhost" ) {
        sync.synchronize( conf.TargetFilesystemMountPoint, it->mountPoint, 4, 1 );  
      } else {
        sync.synchronize( conf.TargetFilesystemMountPoint, it->attachedTo + ":" + it->mountPoint, 4, 0 );  
      }
    }
    
    sync.setSyncTime();
      
    syncingOpPending = 0;
  }
  
  // -------------------------------------------------------------------
  // Sync Request Handler Function
  // -------------------------------------------------------------------
  /* THIS IS GOING TO BE IMPLEMENTED NEXT VERSION
  void SyncRequests_handler( Volumes &volumes ){
    
    std::string request, ack, cip;
    std::string msg;
    int transId;
    
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log("info", "", "volsd", 0, "Sync Requests started");
      
    try {
      ServerSocket server( 8000 ); 

      while (( true ) && ( !sigterm ))
      {
        
        transId = utility::get_transaction_id();
        logger.log("debug", "", "volsd", 0, "waiting for requests from clients ...", "SyncThread");
        ServerSocket new_sock;
        
        // Accept the incoming connection
        server.accept ( new_sock );

        // client Ip address      
        cip = server.client_ip();

        
        
        try {
          while ( true ) 
          {
            
            // reads client request
            new_sock >> request;
            utility::clean_string(request);

            if ( request.compare("SyncAll") == 0 ) {
              
              // the reply to the client will be a communication port. The new port is used as
              // communication channel between the dispatcher and the client.
              logger.log("info", "", "volsd", transId, "request:[" + request + "] from client:[" + 
                         cip + "]", "SyncThread");
              new_sock << "OK";

              new_sock.close_socket();
              // start a new thread which will listen on the port sent to the client. 
              // this thread will handle the disk request
              syncingOpPending = 1;
              std::thread volumesSync_thread( volumesSync_handler, std::ref(snapshots), 
                                        std::ref(volumes),   std::ref(sync));
              volumesSync_thread.detach();  
        

            } else {
              msg = "unknown request, shutting down connection\n";
              logger.log("error", "", "volsd", transId, "unknown request:[" + request + 
                         "], shutting down connection", "SyncThread");
              new_sock << msg;
              new_sock.close_socket();
              break;
            }
          } // end while
        } catch ( SocketException& e) {
          logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description(), "SyncThread");
        }
      } // end of outer while
    } catch ( SocketException& e ) {
      logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description(), "SyncThread");
    }
    
    return;
  }
  */
  
  // -------------------------------------------------------------------
  // Signal Handler Function 
  // -------------------------------------------------------------------
  void signalHandler( int signum ) {

   switch(signum){
     case SIGHUP: // restart
     case SIGTERM: // graceful stop
     case SIGINT: 
       signalCount++;
       // calculate how many times any of the above signals issued.
       if (signalCount == 1){
        
         // 1. set the semaphor var to 1 so that threads knows that they should exit
         sigterm = 1;
         
         // 2 Stop volumeDispatcher thread         
         // BAD->HACK SOLUTION: since the server socket is waiting to accept connection, we cannot 
         // terminate the thread because accept is a blocking method. The only way is to connect to 
         // the socket that will make a connection to the socket. Once we pass the accept method, 
         // then the while loop will exit since it have !sigterm as condition.
         //server->close_socket();
         ClientSocket client_socket ( "localhost", 9000 );
         client_socket << "";
         client_socket.close_socket();
         volumesDispatcher_thread.join();

         // 3 check if main program have some opreation
         while (snapshotsOpPending){
           //std::cout << "waiting for snaphshot thread to finish\n";
           sleep(1);
         }
         
         while (diskCreateOpPending){
           //std::cout << "waiting for DiskCreate operation to finish\n";
           sleep(1);
         }
         
         while (diskRemoveOpPending){
           //std::cout << "waiting for DiskRemove operation to finish\n";
           sleep(1);
         }
         
         while (diskAcquireOpPending){
           //std::cout << "waiting for DiskAcquire operation to finish\n";
           sleep(1);
         }
         
         while (diskReleaseOpPending){
           //std::cout << "waiting for DiskRelease operation to finish\n";
           sleep(1);
         }
         
         
       }
       break;
       
     default:
       break; 
   }

   exit(signum);  
}


  // -------------------------------------------------------------------
  // Populate Ports array Function
  // -------------------------------------------------------------------
  void populate_port_array() {
    for (int i=0; i<10; i++) {
      portsArray[i].portNo = 9000 + 1 + i;
      portsArray[i].status = false;
    }
  }


  // -------------------------------------------------------------------
  // Get Available Port Function
  // -------------------------------------------------------------------
  int get_available_port() {
    for (int i=0; i<10; i++) {
      if (portsArray[i].status == false){
        portsArray[i].status = true;
        return portsArray[i].portNo;
      }
    }
    return -1;
  }


  // -------------------------------------------------------------------
  // Print Ports Function
  // -------------------------------------------------------------------
  void print_ports() {
    std::cout << " | ";
    for (int i=0; i<10; i++) {
      std::cout << portsArray[i].portNo << "-" << portsArray[i].status << " | ";
    }
    std::cout << "\n";
  }


  // -------------------------------------------------------------------
  // Get Ports Function
  // -------------------------------------------------------------------
  std::string get_ports() {

    std::string str;
    stringstream ss1, ss2;
    
    for (int i=0; i<10; i++) {
      ss1 << portsArray[i].portNo;
      ss2 << portsArray[i].status;
      str = str + "[" + ss1.str()  + ":" + ss2.str() + "]\n";
      ss1.str(""); ss2.str("");
    }
    return str;
  }

  // -------------------------------------------------------------------
  // Get Arguments Function
  // -------------------------------------------------------------------
  void get_arguments( int argc, char **argv ) {

    int index;
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "hvsc:")) != -1) 
      switch (c) {   
        case 'h':
          printf("%s: [options]\n", argv[0]);
          printf(" options:\n");
          printf("   -v   version number\n");
          printf("   -c   config file path\n");
          printf("   -s   dump stdout and stderr to the screen\n");
          printf("   -h   print this help menu\n");
          exit (0);
        case 'v':
          printf("Dispatcher version: %i.%i.%i\n", 
                  DISPATCHER_MAJOR_VERSION, DISPATCHER_MINOR_VERSION, DISPATCHER_PATCH_VERSION
          );
          exit (0);
        case 's':
          _onscreen = true;
           break;
        case 'c':
          _conffile = optarg;
          break;
        case '?':
          if (optopt == 'c'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (isprint (optopt)){
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else{
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }
          exit(1);
        default:
          exit(0); 
      }

    for (index = optind; index < argc; index++){
      printf ("Non-option argument %s\n", argv[index]);
      fprintf (stderr, "use -h for help.\n");
      exit(1);
    }
  }
  
  
  // -------------------------------------------------------------------
  // Ensure Mounted Function
  // -------------------------------------------------------------------
  int ensure_mounted(Volumes &volumes, Logger &logger){
    
    // add attach part to this
    logger.log("info", "", "volsd", 0, 
               "ensure that target filesystem and spare volumes are mounted");    
    std::string output;
    // first, check if targetFilesystem is mounted
    if ( !utility::is_mounted( conf.TargetFilesystemMountPoint ) ){
      logger.log("info", "", "volsd", 0, "Target filesystem [" +  
                 conf.TargetFilesystemMountPoint + "] is not mounted. >> mounting ...");
      if ( !utility::mountfs(output, conf.TargetFilesystemMountPoint, conf.TargetFilesystemDevice) ) 
      {
        logger.log("info", "", "volsd", 0, "cannot mount target filesystem. " + output);
        return 0;
      }
    }  
    logger.log("info", "", "volsd", 0, "Target filesystem " +  conf.TargetFilesystemMountPoint + 
               " is mounted");    
    // then, check if previously created volumes are mounted
    volumes.remount();
    
    
    return 1;
  }


  
