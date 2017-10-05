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
	
  bool _onscreen = false;
  std::string _conffile;
  int _loglevel = 3;

  int inProgressVolumes;
  
  std::thread manager_thread;
  std::thread snapshotManager_thread;
  std::thread volumesDispatcher_thread;
  
  // graceful stop
  int sigterm     = 0;
  int signalCount = 0;
 
  int diskCreateOpPending = 0;
  int diskRemoveOpPending = 0;
  int snapshotsOpPending   = 0;
  
  
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
  void volumesDispatcher_handler( Volumes &volumes );
  void clientDiskAquire_handler(int portNo, std::string request, std::string ip, Volumes &volumes, 
                                int transId);
  void clientDiskRelease_handler( Volumes &volumes, const std::string t_volumeId, 
                                  const int t_transactionId );
  void createDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId);
  void removeDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId);
 
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
  // 0. Setup the signals handler
  signal (SIGTERM, signalHandler);
  signal (SIGINT, signalHandler);
  signal(SIGHUP, signalHandler); 
  
  // 1. Load the configurations from conf file
  if ( !utility::load_configuration(conf, _conffile) ){
    std::cout << "error: cannot locate configuration file\n";
    return 1;
  }
    
  std::cout << "tid:"<< conf.TargetFilesystem << "\n";
  std::cout << "t:"<< conf.TargetFilesystemMountPoint << "\n";
  std::cout << "td:"<< conf.TargetFilesystemDevice << "\n";
  
  
  // 2. Ensure files and directories are created
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
    
  // 3. Object Instantiation  
  _loglevel = conf.DispatcherLoglevel;
  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
  Snapshots snapshots(conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
  snapshots.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  
  Volumes volumes( conf.TempMountPoint, conf.VolumeFilePath, conf.MaxIdleDisk );
  volumes.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  
    
  // 4. ensure that volumes are mounted
  if ( !ensure_mounted(volumes, logger) ){
    std::cout << "error: target filesystem " 
              << conf.TargetFilesystemMountPoint 
              << " is not mounted\n";
    return 1;
  }
 
  
  // 5. get the hostname and the Amazon Instance Id for this machine
  hostname    = utility::get_hostname();
  instance_id = utility::get_instance_id();

  // 6. Populating ports array. These ports are there to be able to communicate
  // with multiple clients at once
  portsArray = new Port[10];
  populate_port_array();

  volumesDispatcher_thread = std::thread( volumesDispatcher_handler, std::ref(volumes));
  
  // -------------------------------------------------------------------
  // Core Functionality
  // -------------------------------------------------------------------
  int value;
  while ((true) && (!sigterm) ){

    // ~~~~~~~~~~~~~~~~~~~~~~~
    // 1. maintain snapshots
    // ~~~~~~~~~~~~~~~~~~~~~~~
    std::string output;
    logger.log( "debug", "", "volsd", 1, "check if we need to create a new snapshot" );
    if ( snapshots.renew() ) {
      //this variables tells the termination signal handler function if there is operation going on
      snapshotsOpPending = 1;
      if (!snapshots.create_snapshot(conf.TargetFilesystem, conf.SnapshotFrequency)){
        logger.log( "error", "", "volsd", 1, "could not create new snapshot" );	
      }
      snapshotsOpPending = 0;
    }
    logger.log( "debug", "", "volsd", 1, "No need to create new snapshot" );
    
    /*
    // ~~~~~~~~~~~~~~~~~~~~~~~
    // 1. maintain volumes
    // ~~~~~~~~~~~~~~~~~~~~~~~
    value = volumes.get_idle_number() + inProgressVolumes;
    if ( value < conf.MaxIdleDisk ){
      diskCreateOpPending = 1;
      logger.log( "debug", "", "volsd", 2, "creating a new volume" );	
      std::thread creatDisk_thread( createDisk_handler, 
                                    std::ref(snapshots), 
                                    std::ref(volumes), 
                                    2 );
      creatDisk_thread.detach();
    }
    
    if ( value > conf.MaxIdleDisk ){
      diskRemoveOpPending = 1;
      logger.log( "debug", "", "volsd", 2, "removing a volume" );	
      std::thread removeDisk_thread( removeDisk_handler, 
                                    std::ref(snapshots), 
                                    std::ref(volumes), 
                                    2);
      removeDisk_thread.detach();
    }
    
    
    */  
    sleep(10);
  }
  
  std::cout << "main program exited\n";
  return 1;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions 
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void volumesDispatcher_handler( Volumes &volumes ){
  
  std::string request, ack, cip;
  std::string msg;
  int transId;
  
  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  logger.log("info", "", "volsd", 0, "volumes dispatcher started");
    
  // -------------------------------------------------------------------
  // Core Functionality
  // -------------------------------------------------------------------
  try {
    // Create the socket
    //ServerSocket server( 9000 );
    server = new ServerSocket( 9000 ); 

    while (( true ) && ( !sigterm ))
    {
      
      transId = utility::get_transaction_id();
      logger.log("debug", "", "volsd", 0, "waiting for requests from clients ...");
      ServerSocket new_sock;
      
      // Accept the incoming connection
      server->accept ( new_sock );

      // client Ip address		  
      cip = server->client_ip();

      logger.log("info", "", "volsd", transId, "incoming connection accepted from " + cip);
      
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
                       cip + "]");
            int availablePort = get_available_port();
            new_sock << utility::to_string(availablePort);

            logger.log( "debug", "", "volsd", transId, 
                        "reserving port:[" + utility::to_string(availablePort) + "] to client:[" + 
                        cip + "]"
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
                       cip + "]");
	          std::string volId = request.substr( request.find(":")+1, request.find("\n") );	
					
	          if ( (volId == "") || (volId == " ") ) {
              logger.log("error", "", "volsd", transId, 
                         "client did not supply volume id to release");
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
          			
          } else {
						
            msg = "unknown request, shutting down connection\n";
            logger.log("error", "", "volsd", transId, "unknown request:[" + request + 
                       "], shutting down connection");
            new_sock << msg;
            new_sock.close_socket();
            break;
          }
        } // end while
      } catch ( SocketException& e) {
        logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description());
      }
    } // end of outer while
  } catch ( SocketException& e ) {
    logger.log("error", "", "volsd", transId, "Exception was caught: " + e.description());
  }
  
  return;
}
  
  
  // -------------------------------------------------------------------
  // TASK: Disk Request
  // -------------------------------------------------------------------
  void clientDiskAquire_handler(int portNo, std::string request, std::string ip, Volumes& volumes, 
                         int transId) {

    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    logger.log("debug", "", "volsd", transId, "awaiting clinet to connect tp port:[" + 
               utility::to_string(portNo) + "]" );
		
    std::string msg, ack;
		
    ServerSocket server ( portNo );
		
    ServerSocket s;
    server.accept ( s );
		
    std::string volumeId;
    int res = volumes.release( volumeId, transId );
    if (res == -1) {
      s << "MaxDisksReached";
      portsArray[portNo-9000-1].status=false;
      return;
    }
			
    if (res == -2){
      s << "umountFailed";
      portsArray[portNo-9000-1].status=false;
      return;
    }
			
    if (res == -3){
      s << "detachFailed";
      portsArray[portNo-9000-1].status=false;
      return;
    }
		
    logger.log("info", "", "volsd", transId, "sending to client volume:[" + volumeId + "]");
		
    try {
      s << volumeId;
      logger.log("info", "", "volsd", transId, "waiting for ACK from Client" );
      
      // Client response
      s >> ack;
		  
    } catch ( SocketException& e) {
      // if something happens in the communication, Mark the disk to failed.
      //label disk as used
      
      // remove volumes from m_volumes
      volumes.remove(volumeId, transId);
      
      logger.log("info", "", "volsd", transId, "removing volume from anazone space");		  
      if (!volumes.del(volumeId, transId)){
        logger.log("error", "", "volsd", transId, "volume failed to be removed");		  
      }
      logger.log("info", "", "volsd", transId, "aborting ...");		  
      
      ack = "FAILED";
      logger.log("error", "", "volsd", transId, "connection closed: " + e.description() );		  
      portsArray[portNo-9000-1].status=false;
      return;
    }
    //********* AT THIS POINT, Volume is unmounted and detach but Not delete from list ***********//
    // if client successfuly mounted filestsrem, then update disk status and remove mount point
    if (ack.compare("OK") == 0){
      logger.log("info", "", "volsd", transId, "ACK recived from client: [OK]" );		  

      //label disk as used
      if (!volumes.update(volumeId, "status", "used", transId)) {
        logger.log("info", "", "volsd", transId, "failed to update volumes status");
      }
		  
      logger.log("info", "", "volsd", transId, "disk:[" + volumeId +
                 "] is mounted on client machine");		  
      
    } else {
      // Ideally, we want to put disk back, but for now, just delete the disk and the EBSmanager 
      // will create another one.
      logger.log("info", "", "volsd", transId, "ACK from clinent was [" + ack + "]");	

      // remove volumes from m_volumes
      volumes.remove(volumeId, transId);
      
      // this is commented out since we are going to have the client to remove the volumes
      // delete from volumes list (m_volumes)
      //logger.log("info", "", "volsd", transId, "delete volume from Amazone list");	
      //if (!volumes.del(volumeId, transId)){
      //}
      //logger.log("info", "", "volsd", transId, "aborting ...");		  
	  
      portsArray[portNo-9000-1].status=false;
      return;
    }

    // labling port as not used
    portsArray[portNo-9000-1].status=false;
		
  }


  // -------------------------------------------------------------------
  // clientDiskRelease_handler
  // -------------------------------------------------------------------
  void clientDiskRelease_handler(Volumes &volumes, const std::string t_volumeId, 
                                 const int t_transactionId ) {
	
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
		    
    //check if vol exist
    logger.log("info", "", "volsd", t_transactionId, 
               "get the volume:[" + t_volumeId + "] information");
    if (!volumes.volume_exist(t_volumeId) || (t_volumeId == "") || (t_volumeId == " ") ){
      logger.log("info", "", "volsd", t_transactionId, "volume was not found.");
      return;
    }
    
    // delete from volumes list (m_volumes)
    // ### this is the client program job now. We dont have to do it here.
    //logger.log("info", "", "volsd", transId, "delete volume from volumes list");	
    //volumes.del(volumeId, transId);
	  
	  // delete from aws
	  logger.log("info", "", "volsd", t_transactionId, "remove volume from volumes list" );	
	  if ( !volumes.remove(t_volumeId, t_transactionId) ){
	    logger.log("error", "", "volsd", t_transactionId, 
                 "failed to remove volume:[" + t_volumeId + "] from volumes list");
      return;
	  }
    
    logger.log("info", "", "volsd", t_transactionId, "volume:[" + t_volumeId + 
              "] was removed from volumes list");
  }
   

  // -------------------------------------------------------------------
  // TASK: Create Disk
  // -------------------------------------------------------------------
  void createDisk_handler(Snapshots& snapshot, Volumes& volumes, const int t_transactionId){
    
	  // set this varabile so that main wont create another thread to create new disk
    inProgressVolumes++;
    
	  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
    std::string snapshotId;
    if (!snapshot.latest(snapshotId)){
	    logger.log("info", "", "Manager", t_transactionId, "No snapshot was found.");
    }
  
     
    // all of this operation myst be done in Volumes
    if (!volumes.acquire( conf.TargetFilesystemMountPoint, snapshotId, conf.TempMountPoint, 
                          instance_id,  t_transactionId )) {
      logger.log("info", "", "Manager", t_transactionId, "faield to acquire new volume.");
    }
    
    inProgressVolumes--;   


    diskCreateOpPending = 0;
  }


  // -------------------------------------------------------------------
  // TASK: Remove Disk 
  // -------------------------------------------------------------------
  void removeDisk_handler(Snapshots& s, Volumes& volumes, const int t_transactionId) {
	  
	  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
	  
	  std::string volumeId;
	
	  int res = volumes.release( volumeId, t_transactionId );
	  // delete from volumes list (m_volumes)
    logger.log("debug", "", "Manager", t_transactionId, "delete volume from volumes list");	
    volumes.del(volumeId, t_transactionId);
	  
	  // delete from aws
	  logger.log("info", "", "Manager", t_transactionId, "delete volume in aws enviroment");	
	  if ( !volumes.remove(volumeId, t_transactionId) ){
	    logger.log("error", "", "Manager", t_transactionId, 
                 "failed to delete volume:[" + volumeId + "] from amazon site");
    }
    
    diskRemoveOpPending = 0;
  }
  
  	
  // -------------------------------------------------------------------
  // POPULATE_PORT_ARRAY
  // -------------------------------------------------------------------
  void populate_port_array() {
    for (int i=0; i<10; i++) {
      portsArray[i].portNo = 9000 + 1 + i;
      portsArray[i].status = false;
    }
  }


  // -------------------------------------------------------------------
  // GET_AVAILABLE_PORT
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
  // PRINT_PORTS
  // -------------------------------------------------------------------
  void print_ports() {
    std::cout << " | ";
    for (int i=0; i<10; i++) {
      std::cout << portsArray[i].portNo << "-" << portsArray[i].status << " | ";
    }
    std::cout << "\n";
  }


  // -------------------------------------------------------------------
  // GET_PORTS
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
  // Get_Arguments 
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
         
         // BAD->HACK SOLUTION: since the server socket is waiting to accept connection, we cannot 
         // terminate the thread because accept is a blocking method. The only way is to connect to 
         // the socket that will make a connection to the socket. Once we pass the accept method, 
         // then the while loop will exit since it have !sigterm as condition.
         server->close_socket();

         // 2 Stop volumeDispatcher thread         
         ClientSocket client_socket ( "localhost", 9000 );
         client_socket << "";
         client_socket.close_socket();
         volumesDispatcher_thread.join();

         // 3 check if main program have some opreation
         while (snapshotsOpPending){
           std::cout << "waiting for snahpt thread to finish: " << snapshotsOpPending << "\n";
           sleep(1);
         }
         
         while (diskCreateOpPending){
           std::cout << "waiting for volumes manaber thread to finish\n";
           sleep(1);
         }
         
         while (diskRemoveOpPending){
           std::cout << "waiting for volumes manaber thread to finish\n";
           sleep(1);
         }
       }
       break;
       
     default:
       break; 
   }

   exit(signum);  
}
