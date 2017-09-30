#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <fstream>
#include <mutex>

#include "SocketException.h"
#include "Volumes.h"
#include "ServerSocket.h"
#include "Utils.h"
#include "Logger.h"
#include "Config.h"
#include "Snapshots.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 GLOBALS VARAIBLES AND STRUCTURES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ 
  utility::Port *portsArray;
  utility::Configuration conf;

  std::string hostname; 
  std::string instance_id;
	
  std::mutex m;
	
  std::vector<std::string>  devices_list;
	
  bool _onscreen = false;
  std::string _conffile;
  int _loglevel = 3;

  int inProgressVolumes;
  
  
  
	
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  // thread related functions
  void disk_request_task(int portNo, std::string request, std::string ip, Volumes& d, int transId);
  void disk_release_task(Volumes &volumes, std::string volId, int transId);
  void volume_manager_task(Snapshots &snapshots, Volumes& d);
  void debug_task(int portNo, Volumes& d);
  void createDisk_task(Snapshots& s, Volumes& dc);
  void removeDisk_task(Snapshots& s, Volumes& dc );
  void createSnapshot_task(Snapshots& sshot, int snapshotMaxNumber, std::string snapshotFile, int snapshotFreq);
  //void EBSVolumeManager_task();
  //void EBSVolumeSync_task();

  void populate_port_array();
  std::string get_ports();
  void print_ports();
  int get_available_port();

  int disk_prepare(utility::Volume& volume, Volumes& dc, int transactionId, Logger& logger);
  void get_arguments( int argc, char **argv );
  int ensure_mounted(Volumes &volumes, Logger &logger);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 MAIN PROGRAM
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
using namespace std;
using namespace utility;
	
int main ( int argc, char* argv[] ) 
{
  
  std::string request, ack, cip;
  std::string msg;
  int transId;

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
  // 1. Load the configurations from conf file
  if ( !utility::load_configuration(conf, _conffile) ){
    // TODO: ensure that conf file is correct
    std::cout << "error: cannot locate configuration file\n";
    return 1;
  }
      
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
  
  Volumes volumes( conf.TempMountPoint, conf.VolumeFilePath, conf.MaxIdleDisk );
  volumes.set_logger_att ( _onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel );
  
  // 4. ensure that volumes are mounted
  if ( !ensure_mounted(volumes, logger) ){
    std::cout << "error: target filesystem " << conf.TargetFilesystemMountPoint << " is not mounted\n";
    return 0;
  }
 
  
  // 5. get the hostname and the Amazon Instance Id for this machine
  hostname    = utility::get_hostname();
  instance_id = utility::get_instance_id();

  // 6. Populating ports array. These ports are there to be able to communicate
  // with multiple clients at once
  portsArray = new Port[10];
  populate_port_array();

  
  // 7. start the volume manager thread
  logger.log("info", "", "volumesd", 0, "starting the volumes manager...");
  thread manager_thread(volume_manager_task, std::ref(snapshots), std::ref(volumes));
  manager_thread.detach();

  // 8. start a thread to create a snapshot every 4 hours
  logger.log("info", "", "volumesd", 0, "starting the snapshot manager...");
  std::thread snapshotManager_thread(createSnapshot_task, std::ref(snapshots), conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
  snapshotManager_thread.detach();
  
  logger.log("info", "", "volumesd", 0, "volumesd programs started");
    
  // -------------------------------------------------------------------
  // Core Functionality
  // -------------------------------------------------------------------
  try {
    // Create the socket
    ServerSocket server ( 9000 );

    while ( true ) 
    {
      transId = utility::get_transaction_id();
      logger.log("debug", "", "volumesd", 0, "waiting for requests from clients ...");
      ServerSocket new_sock;
      
      // Accept the incoming connection
      server.accept ( new_sock );
      // client Ip address		  
      cip = server.client_ip();

      logger.log("info", "", "volumesd", transId, "incoming connection accepted from " + cip);
      
      try {
        while ( true ) 
        {
          // reads client request
          new_sock >> request;
          utility::clean_string(request);

          if ( request.compare("DiskRequest") == 0 ) {
            
            // the reply to the client will be a communication port. The new port is used as
            // communication channel between the dispatcher and the client.
            logger.log("info", "", "volumesd", transId, "request:[" + request + "] from client:[" + cip + "]");
            int availablePort = get_available_port();
            new_sock << utility::to_string(availablePort);

            logger.log( "debug", "", "volumesd", transId, 
                        "reserving port:[" + utility::to_string(availablePort) + "] to client:[" + cip + "]"
            );

            new_sock.close_socket();
            // start a new thread which will listen on the port sent to the client. 
            // this thread will handle the disk request
            thread t1(disk_request_task, availablePort, request, cip, std::ref(volumes), transId);
            t1.detach();
            break;

          } else if ( request.find("DiskRelease") != std::string::npos ) {
            
            // DiskRelease request format: "DiskRelease:volId"
            logger.log("info", "", "volumesd", transId, "request:[" + request + "] from client:[" + cip + "]");
	        std::string volId = request.substr( request.find(":")+1, request.find("\n") );	
					
	    if ( (volId == "") || (volId == " ") ) {
              logger.log("error", "", "volumesd", transId, "client did not supply volume id to release");
              new_sock.close_socket();
              break;
            }
					
            thread disk_release_thread(disk_release_task, std::ref(volumes), volId, transId);
            disk_release_thread.detach();
					
            new_sock.close_socket();
            break;
          			
          } else {
						
            msg = "unknown request, shutting down connection\n";
            logger.log("error", "", "volumesd", transId, "unknown request:[" + request + "], shutting down connection");
            new_sock << msg;
            break;
          }
        } // end while
      } catch ( SocketException& e) {
        logger.log("error", "", "volumesd", transId, "Exception was caught: " + e.description());
      }
    } // end of outer while
  } catch ( SocketException& e ) {
    logger.log("error", "", "volumesd", transId, "Exception was caught: " + e.description());
  }
  
  return 0;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions 
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  // -------------------------------------------------------------------
  // TASK: Disk Request
  // -------------------------------------------------------------------
  void disk_request_task(int portNo, std::string request, std::string ip, Volumes& volumes, int transId) {
	
	
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    logger.log("debug", "", "volumesd", transId, 
               "awaiting clinet to connect tp port:[" + utility::to_string(portNo) + "]", "DiskRequestThread"
    );
		
    //int transaction = utility::get_transaction_id();   
    //int transaction = utility::get_transaction_id();   
		
    //utility::Volume v;
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
		
    logger.log("info", "", "volumesd", transId, "sending to client volume:[" + volumeId + "]");
		
    try {
      s << volumeId;
      logger.log("info", "", "volumesd", transId, "waiting for ACK from Client" );
      
      s >> ack;
		  
    } catch ( SocketException& e) {
      ack = "FAILED";
      logger.log("error", "", "volumesd", transId, "connection closed: " + e.description() );		  
      portsArray[portNo-9000-1].status=false;
      return;
    }
    //********* AT THIS POINT, Volume is unmounted and detach but Not delete from list ***********//
    // if client successfuly mounted filestsrem, then update disk status and remove mount point
    if (ack.compare("OK") == 0){
      logger.log("info", "", "volumesd", transId, "ACK recived from client: [OK]" );		  

      //label disk as used
      if (!volumes.update(volumeId, "status", "used", transId)) {
        logger.log("info", "", "volumesd", transId, "failed to update volumes status");
      }
		  
      logger.log("info", "", "volumesd", transId, "disk:[" + volumeId + "] is mounted on client machine");		  
      
    } else {
      // Ideally, we want to put disk back, but for now, just delete the disk and the EBSmanager will create another one.
      logger.log("info", "", "volumesd", transId, "ACK from clinent was [" + ack + "]");	

      // delete from volumes list (m_volumes)
      logger.log("info", "", "volumesd", transId, "delete volume from volumes list");	
      volumes.del(volumeId, transId);
	  
	  // delete from aws
	  logger.log("info", "", "volumesd", transId, "delete volume in aws enviroment");	
	  volumes.remove(volumeId, transId);
	  
      logger.log("info", "", "volumesd", transId, "aborting ...");		  
	  
      portsArray[portNo-9000-1].status=false;
      return;
    }

    // labling port as not used
    portsArray[portNo-9000-1].status=false;
		
  }


  // -------------------------------------------------------------------
  // TASK: Volume_Manager
  // -------------------------------------------------------------------
  void volume_manager_task(Snapshots &snapshots, Volumes &volumes){

	  
	Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log( "debug", "", "Manager", 0, "volumes manager thread started" );	
    
	inProgressVolumes = 0;
	
	int value;
    while (true) {
      // create disks if needed
      value = volumes.get_idle_number() + inProgressVolumes;
      if ( value < conf.MaxIdleDisk ){
        logger.log( "debug", "", "Manager", 0, "creating a new volume" );	
        std::thread creatDisk_thread(createDisk_task, std::ref(snapshots), std::ref(volumes) );
        creatDisk_thread.detach();
      }
    
      if ( value > conf.MaxIdleDisk ){
        logger.log( "debug", "", "Manager", 0, "removing a volume" );	
        std::thread removeDisk_thread(removeDisk_task, std::ref(snapshots), std::ref(volumes) );
        removeDisk_thread.detach();
      }
    
      sleep(10);
    }  
    
  }


  // -------------------------------------------------------------------
  // TASK: Create Disk
  // -------------------------------------------------------------------
  void createDisk_task(Snapshots& snapshot, Volumes& volumes){
	  
	  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
    int transaction = utility::get_transaction_id();
    
    std::string snapshotId;
    if (!snapshot.latest(snapshotId, logger)){
		  logger.log("info", "", "Manager", transaction, "No snapshot was found.");
	  }
		
  
    // set this varabile so that main wont create another thread to create new disk
    inProgressVolumes++;

    
     
    // all of this operation myst be done in Volumes
    if (!volumes.acquire( conf.TargetFilesystemMountPoint, snapshotId, conf.TempMountPoint, instance_id,  transaction )) {
      logger.log("info", "", "Manager", transaction, "faield to acquire new volume.");
    }
    
    inProgressVolumes--;      
    
	  
  }


  // -------------------------------------------------------------------
  // TASK: Remove Disk 
  // -------------------------------------------------------------------
  void removeDisk_task(Snapshots& s, Volumes& dc ) {
	/*
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    int transaction = utility::get_transaction_id();
    
    utility::Volume v;
    
    std::cout << " REMOVE_DISK(" << transaction << "):: thread started: REMOVING extra Disk \n";
    logger.log("info", "", "Manager", transaction, "exsting volumes numbner exceeds the MaxIdleDisk");
    
    if (!dc.ebsvolume_idle(v, transaction, logger)) 
      return;
    
    m.lock();
    dc.ebsvolume_setstatus( "delete", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
    m.unlock();
  
    if (!dc.ebsvolume_umount(v, transaction, logger)){
      return;
    }

    if (!dc.ebsvolume_detach(v, transaction, logger)) 
      return;
    
    if (!dc.ebsvolume_delete(v, transaction, logger)) 
      return;  
    
    if (!dc.remove_mountpoint(v.mountPoint, transaction, logger)) 
      return;  
  
    std::cout << "REMOVE_DISK(" << transaction << "):: thread finished. \n";
    */
  }
  
  
  // -------------------------------------------------------------------
  // TASK: Create Snapshot
  // -------------------------------------------------------------------
  void createSnapshot_task(Snapshots& snapshots, int snapshotMaxNumber, std::string snapshotFile, int snapshotFreq) {
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log( "debug", "", "SnapShots", 0, "snapshots manager started" );	
    
    std::string output;
    
    while (true) {
      if (!snapshots.create_snapshot(conf.TargetFilesystem, snapshotFreq, logger)){
        logger.log( "error", "", "SnapShots", 0, "could not create new snapshot" );	
      }
      sleep(snapshotFreq*60);
    }
  }
	
  // -------------------------------------------------------------------
  // DISK_RELEASE_TASK
  // -------------------------------------------------------------------
  void disk_release_task(Volumes &dc, std::string volId, int transId) {
		
    Logger logger(_onscreen, conf.DispatcherLogPrefix, _loglevel);
		
    utility::Volume v;
    v.id = volId;
		
    //check if vol exist
    logger.log("info", "", "volumesd", transId, 
               "checks if volume:[" + volId + "] exist in disk file", "DiskReleaseThread"
    );
    if (!dc.ebsvolume_exist(v.id) || (v.id == "") || (v.id == " ") ){
      logger.log("info", "", "volumesd", transId, "volume was not found in diskfile", "DiskReleaseThread");
      return;
    }
    logger.log("info", "", "volumesd", transId, "volume was found.", "DiskReleaseThread");
	
    logger.log("info", "", "volumesd", transId, "removing volume:[" + v.id + "] from disk file", "DiskReleaseThread");
    m.lock();
    dc.ebsvolume_setstatus( "delete", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger);
    m.unlock();
        
    logger.log("info", "", "volumesd", transId, "deleting volume:[" + v.id + "]", "DiskReleaseThread");
    if (!dc.ebsvolume_delete(v, transId, logger)){
      logger.log("error", "", "volumesd", transId, 
                  "failed to delete volume:[" + v.id + "] from amazon site", "DiskReleaseThread"
      );
      return;
    }
    logger.log("info", "", "volumesd", transId, "volume:[" + v.id + "] was deleted", "DiskReleaseThread");
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
    logger.log("info", "", "volumesd", 0, "ensure that target filesystem and spare volumes are mounted");    
    std::string output;
    // first, check if targetFilesystem is mounted
    if ( !utility::is_mounted( conf.TargetFilesystemMountPoint ) ){
      logger.log("info", "", "volumesd", 0, "Target filesystem " +  conf.TargetFilesystemMountPoint + " is not mounted. >> mounting ...");
      if ( !utility::mountfs(output, conf.TargetFilesystemMountPoint, conf.TargetFilesystemDevice) ) {
        logger.log("info", "", "volumesd", 0, "cannot mount target filesystem. " + output);
        return 0;
      }
    }  
    logger.log("info", "", "volumesd", 0, "Target filesystem " +  conf.TargetFilesystemMountPoint + " is mounted");    
    // then, check if previously created volumes are mounted
    volumes.remount();
    
    
    return 1;
  }
