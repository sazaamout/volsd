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
  
  Volumes volumes;
  Snapshots snapshots;
	
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  // thread related functions
  void disk_request_task(int portNo, std::string request, std::string ip, Volumes& d, int transId);
  void disk_release_task(Volumes &volumes, std::string volId, int transId);
  void volume_manager_task(Volumes& d);
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
  //int parse_arguments(int argc, char* argv[]);

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
    
  // TODO: mount/ensure the target file system 
  // TODO: start a cleaner thread that check if volumes are correct
  
  // 3. Object Instantiation  
  _loglevel = conf.DispatcherLoglevel;
  Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
  volumes = Volumes(true, conf.TempMountPoint, conf.VolumeFilePath, conf.MaxIdleDisk);
  snapshots  = Snapshots(conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
  
  // 4. get the hostname and the Amazon Instance Id for this machine
  hostname    = utility::get_hostname();
  instance_id = utility::get_instance_id();

  // 5. Populating ports array. These ports are there to be able to communicate
  // with multiple clients at once
  portsArray = new Port[10];
  populate_port_array();

  // 5. start the admin thread
  //thread debug_thread(debug_task, 8000, std::ref(volumes));
  //debug_thread.detach();
  
  // 6. start the volume manager thread
  logger.log("info", hostname, "Dispatcher", 0, "starting the volumes manager...");
  thread manager_thread(volume_manager_task, std::ref(volumes));
  manager_thread.detach();

  // 7. start a thread to create a snapshot every 4 hours
  logger.log("info", hostname, "Dispatcher", 0, "starting the snapshot manager...");
  std::thread snapshotManager_thread(createSnapshot_task, std::ref(snapshots), conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
  snapshotManager_thread.detach();
  
  logger.log("info", hostname, "Dispatcher", 0, "dispatcher programs started");
    
  // -------------------------------------------------------------------
  // Core Functionality
  // -------------------------------------------------------------------
  try {
    // Create the socket
    ServerSocket server ( 9000 );

    while ( true ) 
    {
      transId = utility::get_transaction_id();
      logger.log("debug", hostname, "Dispatcher", 0, "waiting for requests from clients ...");
      ServerSocket new_sock;
      
      // Accept the incoming connection
      server.accept ( new_sock );
      // client Ip address		  
      cip = server.client_ip();

      logger.log("info", hostname, "Dispatcher", transId, "incoming connection accepted from " + cip);
      
      try {
        while ( true ) 
        {
          // reads client request
          new_sock >> request;
          utility::clean_string(request);

          if ( request.compare("DiskRequest") == 0 ) {
            
            // the reply to the client will be a communication port. The new port is used as
            // communication channel between the dispatcher and the client.
            logger.log("info", hostname, "Dispatcher", transId, "request:[" + request + "] from client:[" + cip + "]");
            int availablePort = get_available_port();
            new_sock << utility::to_string(availablePort);

            logger.log( "debug", hostname, "Dispatcher", transId, 
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
            logger.log("info", hostname, "Dispatcher", transId, "request:[" + request + "] from client:[" + cip + "]");
	        std::string volId = request.substr( request.find(":")+1, request.find("\n") );	
					
	    if ( (volId == "") || (volId == " ") ) {
              logger.log("error", hostname, "Dispatcher", transId, "client did not supply volume id to release");
              new_sock.close_socket();
              break;
            }
					
            thread disk_release_thread(disk_release_task, std::ref(volumes), volId, transId);
            disk_release_thread.detach();
					
            new_sock.close_socket();
            break;
          			
          } else {
						
            msg = "unknown request, shutting down connection\n";
            logger.log("error", hostname, "Dispatcher", transId, "unknown request:[" + request + "], shutting down connection");
            new_sock << msg;
            break;
          }
        } // end while
      } catch ( SocketException& e) {
        logger.log("error", hostname, "Dispatcher", transId, "Exception was caught: " + e.description());
      }
    } // end of outer while
  } catch ( SocketException& e ) {
    logger.log("error", hostname, "Dispatcher", transId, "Exception was caught: " + e.description());
  }
  
  return 0;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Functions 
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  // -------------------------------------------------------------------
  // TASK: Disk Request
  // -------------------------------------------------------------------
  void disk_request_task(int portNo, std::string request, std::string ip, Volumes& dc, int transId) {
	
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    
    logger.log("debug", hostname, "Dispatcher", transId, 
               "awaiting clinet to connect tp port:[" + utility::to_string(portNo) + "]", "DiskRequestThread"
    );
		
    //int transaction = utility::get_transaction_id();   
    //int transaction = utility::get_transaction_id();   
		
    utility::Volume v;
    std::string msg, ack;
		
    ServerSocket server ( portNo );
		
    ServerSocket s;
    server.accept ( s );
		
    int res = disk_prepare(v, dc, transId, logger);
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
		
    logger.log("info", hostname, "Dispatcher", transId, "sending to client volume:[" + v.id + "]", "DiskRequestThread");
		
    try {
      s << v.id;
      logger.log("info", hostname, "Dispatcher", transId, "waiting for ACK from Client", "DiskRequestThread");
      
      s >> ack;
		  
    } catch ( SocketException& e) {
      ack = "FAILED";
      logger.log("error", hostname, "Dispatcher", transId, "connection closed: " + e.description(), "DiskRequestThread");		  
      portsArray[portNo-9000-1].status=false;
      return;
    }

    if (ack.compare("OK") == 0){
      logger.log("info", hostname, "Dispatcher", transId, "ACK recived from client: [OK]" , "DiskRequestThread");		  

      //label disk as used
      logger.log("info", hostname, "Dispatcher", transId, "set volume's status to 'used' by:[" + ip +"]" , "DiskRequestThread");		  
      m.lock();
      int res = dc.ebsvolume_setstatus( "update", v.id, "used", ip, "/home/cde", "none", transId, logger );
      m.unlock();
		  
      if (!res) {
        m.lock();
	dc.ebsvolume_setstatus( "update", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger );
        m.unlock();
        logger.log("error", hostname, "Dispatcher", transId, "ACK [OK] was recived but failed to write to volume file" , "DiskRequestThread");		  
        logger.log("error", hostname, "Dispatcher", transId, "aborting" , "DiskRequestThread");		  
      }else {
        logger.log("info", hostname, "Dispatcher", transId, "disk:[" + v.id + "] is mounted on client machine" , "DiskRequestThread");		  
      }
    } else {
      // Ideally, we want to put disk back, but for now, just delete the disk and the EBSmanager will create another one.
      logger.log("info", hostname, "Dispatcher", transId, "ACK from clinent was [" + ack + "]" , "DiskRequestThread");	
      logger.log("info", hostname, "Dispatcher", transId, "delete volume from volumes list" , "DiskRequestThread");	
      m.lock();
      dc.ebsvolume_setstatus( "delete", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger);
      m.unlock();
		  		  
      logger.log("info", hostname, "Dispatcher", transId, "aborting ..." , "DiskRequestThread");		  
	  
      portsArray[portNo-9000-1].status=false;
      return;
    }

    // labling port as not used
    portsArray[portNo-9000-1].status=false;
		
  }


  // -------------------------------------------------------------------
  // TASK: Volume_Manager
  // -------------------------------------------------------------------
  void volume_manager_task(Volumes& volumes){

	Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log( "debug", hostname, "Manager", 0, "volumes manager thread started" );	
    
	inProgressVolumes = 0;
	
	int value;
    while (true) {
	  // create disks if needed
      value = volumes.ebsvolume_idle_number() + inProgressVolumes;
      logger.log( "debug", hostname, "Manager", 0, "value is " + utility::to_string(value) );	
      if ( value < conf.MaxIdleDisk ){
        logger.log( "debug", hostname, "Manager", 0, "creating a new volume" );	
        std::thread creatDisk_thread(createDisk_task, std::ref(snapshots), std::ref(volumes) );
        creatDisk_thread.detach();
      }
    
      if ( value > conf.MaxIdleDisk ){
        logger.log( "debug", hostname, "Manager", 0, "removing a volume" );	
        std::thread removeDisk_thread(removeDisk_task, std::ref(snapshots), std::ref(volumes) );
        removeDisk_thread.detach();
      }
    
      sleep(10);
    }  
  }


  // -------------------------------------------------------------------
  // TASK: Create Disk
  // -------------------------------------------------------------------
  void createDisk_task(Snapshots& snapshot, Volumes& dc){
	  
	Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
  
    int transaction = utility::get_transaction_id();
    
    std::string snapshotId;
    if (!snapshot.latest(snapshotId, logger)){
		logger.log("info", hostname, "Manager", transaction, "No snapshot was found.");
	}
		
  
    // set this varabile so that main wont create another thread to create new disk
    inProgressVolumes++;
    
    utility::Volume v;
      
    /* -----------------------------------------------------------------   
     * 1) Prepare disk information.
     * ---------------------------------------------------------------*/
    // get a device. 
    logger.log("info", hostname, "Manager", transaction, "allocating a device.", "CreateDisk");
    v.device = dc.get_device(devices_list);
    devices_list.push_back(v.device);
    logger.log("info", hostname, "Manager", transaction, "device allocated:[" + v.device + "]", "CreateDisk");
    logger.log("info", hostname, "Manager", transaction, "Devices List:[" + utility::to_string(devices_list) + "]", "CreateDisk");
  
    // get mount point
    v.mountPoint = conf.TempMountPoint + utility::randomString();
    logger.log("info", hostname, "Manager", transaction, "allocating mounting point:[" +  v.mountPoint + "]", "CreateDisk");
    
    
    /* -----------------------------------------------------------------   
       * 2) Create Volume
   * ---------------------------------------------------------------*/   
    logger.log("info", hostname, "Manager", transaction, "create Volume from latest snapshot:[" + snapshotId +"]", "CreateDisk");
    if (!dc.ebsvolume_create(v, snapshotId, transaction, logger)){
      logger.log("error", hostname, "Manager", transaction, "FALIED to create a volume", "CreateDisk");
      //remove_device
      utility::remove_element(devices_list, v.device);
      inProgressVolumes--;
      return;
    }
    logger.log("info", hostname, "Manager", transaction, "volume:[" + v.id + "] creation was successful", "CreateDisk");
  
    // add disk to file
    logger.log("info", hostname, "Manager", transaction, "adding volume to volume file", "CreateDisk");
    v.attachedTo = "localhost";
    m.lock();
    dc.ebsvolume_setstatus( "add", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
    m.unlock();


    /* -----------------------------------------------------------------   
     * 3) Attach Volume
     * ---------------------------------------------------------------*/ 
    logger.log("info", hostname, "Manager", transaction, "attaching new disk to localhost", "CreateDisk");
    if (!dc.ebsvolume_attach(v, instance_id, transaction, logger)){
      //logger("error", hostname, "EBSManager::thread", transaction, "failed to attaching new disk to localhost. Removing...");
      logger.log("error", hostname, "Manager", transaction, "FAILED to attaching new disk to localhost. Removing  from AWS space", "CreateDisk");
      if (!dc.ebsvolume_delete(v, transaction, logger)){
        logger.log("error", hostname, "Manager", transaction, "FAILED to delete volume from AWS space", "CreateDisk");
      }
      logger.log("info", hostname, "Manager", transaction, "volume was removed from AWS space", "CreateDisk");
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
      logger.log("debug", hostname, "Manager", transaction, "removing device", "CreateDisk");
      utility::remove_element(devices_list, v.device);
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing disk from file");
      logger.log("info", hostname, "Manager", transaction, "removing volume from volume file", "CreateDisk");
      m.lock();
      dc.ebsvolume_setstatus( "delete", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
      m.unlock();
      inProgressVolumes--;
      return;
    }
    logger.log("info", hostname, "Manager", transaction, "volume was attached successfully", "CreateDisk");
    sleep(5);
    
    /* -----------------------------------------------------------------   
     * 4) mount Volume
     * ---------------------------------------------------------------*/ 
    logger.log("info", hostname, "Manager", transaction, "mounting volume into localhost", "CreateDisk");
    if (!dc.ebsvolume_mount(v, instance_id, transaction, logger)){
    
      //logger("error", hostname, "EBSManager::thread", transaction, "failed to mount new disk to localhost. Detaching...");
      logger.log("error", hostname, "Manager", transaction, "FAILED to mount new disk to localhost. Detaching...", "CreateDisk");
      if (!dc.ebsvolume_detach(v, transaction, logger))
        logger.log("error", hostname, "Manager", transaction, "FAILED to detach.", "CreateDisk");
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
      logger.log("info", hostname, "Manager", transaction, "removing device", "CreateDisk");
      utility::remove_element(devices_list, v.device);
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing disk from file");
      logger.log("info", hostname, "Manager", transaction, "removing disk from file", "CreateDisk");
      m.lock();
      dc.ebsvolume_setstatus( "delete", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
      m.unlock();
      inProgressVolumes--;
      return;
    }
    logger.log("info", hostname, "Manager", transaction, "volume was mounted successfully into localhost", "CreateDisk");
  
  
    /* -----------------------------------------------------------------   
     * 5) Sync Volume
     * ---------------------------------------------------------------*/ 
    // sync it with the TargetFilesystemMountPoint
    // get all of the paths from conf.SyncRequestsFile, sync the volume 
    logger.log("info", hostname, "Manager", transaction, "syncing new volume", "CreateDisk");
    std::fstream myFile;
    std::string output, line, pathDate;
  
    // open a socket send a sync request
  
    // wait for resutls
  
  
    //if (!dc.ebsvolume_sync(v.mountPoint, conf.TargetFilesystemMountPoint, transaction, logger))
    //  logger.log("info", hostname, "EBSManager", transaction, "syncing new volume failed", "CreateDisk");
    logger.log("debug", hostname, "Manager", transaction, "EXECMD:[echo 'sync " + v.mountPoint + " ' > /dev/tcp/infra1/" + utility::to_string( conf.SyncerServicePort ), "CreateDisk");
    int res = utility::exec(output, "echo 'sync " + v.mountPoint + "' > /dev/tcp/infra1/" + utility::to_string( conf.SyncerServicePort ) );
  
    if (!res){
      logger.log("debug", hostname, "Manager", transaction, "syntax error in command", "CreateDisk");
    }
  
  
    logger.log("info", hostname, "Manager", transaction, "adding disk to file and changing status", "CreateDisk");
    m.lock();
    dc.ebsvolume_setstatus( "update", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
    m.unlock();
  
    //dc.ebsvolume_print();
  
    inProgressVolumes--;

    // now that volume is mounted, there is no need to keep the device in the devices_list since the get_device function (Disks Class) will read all of the mounted
    // devices on the server.
    //remove_device
    utility::remove_element(devices_list, v.device);
	  
  }


  // -------------------------------------------------------------------
  // TASK: Remove Disk 
  // -------------------------------------------------------------------
  void removeDisk_task(Snapshots& s, Volumes& dc ) {
	  
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    int transaction = utility::get_transaction_id();
    utility::Volume v;
    
    std::cout << " REMOVE_DISK(" << transaction << "):: thread started: REMOVING extra Disk \n";
    
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
  }
  
  
  // -------------------------------------------------------------------
  // TASK: Create Snapshot
  // -------------------------------------------------------------------
  void createSnapshot_task(Snapshots& snapshots, int snapshotMaxNumber, std::string snapshotFile, int snapshotFreq) {
    Logger logger(_onscreen, conf.DispatcherLogPrefix + "dispatcher.log", _loglevel);
    logger.log( "debug", hostname, "SnapShots", 0, "snapshots manager started" );	
    
    std::string output;
    
    while (true) {
      if (!snapshots.create_snapshot(conf.TargetFilesystem, snapshotFreq, logger)){
        logger.log( "error", hostname, "SnapShots", 0, "could not create new snapshot" );	
      }
      sleep(snapshotFreq*60);
    }
  }
  
  
  // -------------------------------------------------------------------
  // DISK_PREPARE
  // -------------------------------------------------------------------
  int disk_prepare(utility::Volume& v, Volumes& dc, int transactionId, Logger& logger) {

    std::string volId;
    logger.log("info", hostname, "Dispatcher", transactionId, "get idle volume from volumes file", "DiskRequestThread");
  
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // 1. Get Idle Volume  
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    if (!dc.ebsvolume_idle(v, transactionId, logger)) {
      logger.log("error", hostname, "Dispatcher", transactionId, "no more idle volume available", "DiskRequestThread");
      return -1;
    } else {
      logger.log("info", hostname, "Dispatcher", transactionId, "idle volume found:[" + v.id + "]", "DiskRequestThread");
      logger.log("info", hostname, "Dispatcher", transactionId, "set new volume's status to 'inprogress'", "DiskRequestThread");
      m.lock();
      if (!dc.ebsvolume_setstatus( "update", v.id, "inprogress", "none", "none", "none", transactionId, logger)){
        logger.log("error", hostname, "Dispatcher", transactionId, "failed to update disk status", "DiskRequestThread");
      }
      m.unlock();
			
      logger.log("info", hostname, "Dispatcher", transactionId, "umounting volume", "DiskRequestThread");
      
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      // 2. unmount disk
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      if (!dc.ebsvolume_umount(v, transactionId, logger)) {
        logger.log("error", hostname, "Dispatcher", transactionId, "failed to umount volume", "DiskRequestThread");
        logger.log("info", hostname, "Dispatcher", transactionId, "remove volume from volume list", "DiskRequestThread");
        m.lock();
        if (!dc.ebsvolume_setstatus( "delete", v.id, "", "", "", "", transactionId, logger)) {
          logger.log("error", hostname, "Dispatcher", transactionId, "failed to update disk status", "DiskRequestThread");
        }
        m.unlock();
        return -2;
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "volume umounted", "DiskRequestThread");
		
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      // 3. Detach the volume
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      logger.log("info", hostname, "Dispatcher", transactionId, "detaching volume", "DiskRequestThread");
      if (!dc.ebsvolume_detach(v, transactionId, logger)) {
        logger.log("error", hostname, "Dispatcher", transactionId, "failed to detach volume", "DiskRequestThread");
        return -3;
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "volume was detached", "DiskRequestThread");
			
      logger.log("info", hostname, "Dispatcher", transactionId, "remove mountpoint:[" + v.mountPoint +"]", "DiskRequestThread");
      if (!dc.remove_mountpoint(v.mountPoint, transactionId, logger)){
        logger.log("error", hostname, "Dispatcher", transactionId, "could not remove mountpoint", "DiskRequestThread");
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "mount point was removed", "DiskRequestThread");
    }

    sleep(5);
    return 1;
  }

	
  // -------------------------------------------------------------------
  // DISK_RELEASE_TASK
  // -------------------------------------------------------------------
  void disk_release_task(Volumes &dc, std::string volId, int transId) {
		
    Logger logger(_onscreen, conf.DispatcherLogPrefix, _loglevel);
		
    utility::Volume v;
    v.id = volId;
		
    //check if vol exist
    logger.log("info", hostname, "Dispatcher", transId, 
               "checks if volume:[" + volId + "] exist in disk file", "DiskReleaseThread"
    );
    if (!dc.ebsvolume_exist(v.id) || (v.id == "") || (v.id == " ") ){
      logger.log("info", hostname, "Dispatcher", transId, "volume was not found in diskfile", "DiskReleaseThread");
      return;
    }
    logger.log("info", hostname, "Dispatcher", transId, "volume was found.", "DiskReleaseThread");
	
    logger.log("info", hostname, "Dispatcher", transId, "removing volume:[" + v.id + "] from disk file", "DiskReleaseThread");
    m.lock();
    dc.ebsvolume_setstatus( "delete", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger);
    m.unlock();
        
    logger.log("info", hostname, "Dispatcher", transId, "deleting volume:[" + v.id + "]", "DiskReleaseThread");
    if (!dc.ebsvolume_delete(v, transId, logger)){
      logger.log("error", hostname, "Dispatcher", transId, 
                  "failed to delete volume:[" + v.id + "] from amazon site", "DiskReleaseThread"
      );
      return;
    }
  
  logger.log("info", hostname, "Dispatcher", transId, "volume:[" + v.id + "] was deleted", "DiskReleaseThread");
    
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
  // DEBUG_TASK
  // -------------------------------------------------------------------
  void debug_task(int portNo, Volumes& dc) {
	  
    Logger logger(_onscreen, conf.DispatcherLogPrefix, _loglevel);
		  
    std::string request, ack, reply;
    ServerSocket debugThread ( portNo );

    while ( true ) {
      ServerSocket new_sock;
      // here, make a new threads. Every new clinet needs a threds. 
      debugThread.accept ( new_sock );
	  
      try {
        while ( true ) {
          try {
            new_sock  >> request;
            utility::clean_string(request);
		
            logger.log("info", hostname, "Dispatcher", 0, "Request recived [" + request + "]", "Debug");
				
            if ( request.compare("portlist") == 0 ) {
              //std::cout << "  DEBUG: portlist request Recived" << std::endl;
              reply = get_ports();
              new_sock << reply;
				
            }else if ( ( request.compare("help") == 0 ) || ( request.compare("h") == 0 ) ) {
              //std::cout << "  DEBUG help request Recived" << std::endl;
              reply = "Commands List:\nvollist\nvolsetid\nportlist\ebssync\nebspush\nexit\n";
              new_sock << reply;
              //new_sock.close_socket();
              //break;				
            }else if ( request.compare("vollist") == 0 ) {
              // TODO
              reply = dc.ebsvolume_list("all", conf.VolumeFilePath);
              new_sock << reply;
				
              //new_sock.close_socket();
              //break;	
            } else if ( request.find("snapshot_create") != std::string::npos ) {
              // TODO
              // this will create a snapshot from the target point and set it to latest. 
		
              std::string volId = request.substr( request.find(" ")+1, request.find("\n") );
              //new_sock.close_socket();
              //break;
            } else if ( request.compare("volsetidle") == 0 ) {
              std::cout << "debug" << std::endl;
              reply = "volume ID:"; 
              new_sock << reply;
			  
              new_sock >> reply;
              utility::clean_string(reply);
								
              dc.ebsvolume_setstatus("update", reply, "idle", "none", "none", "none", 0, logger);
              reply = "Volume [" + reply + "] status was chagned\n";
              new_sock << reply;
              //new_sock.close_socket();
              //break;	 
              
            } else if (( request.compare("exit") == 0 ) || ( request.compare("quit") == 0 )) {
              //std::cout << "  DEBUG Client request to Exit\n\n";
              new_sock.close_socket();
              break;

            } else if ( request.compare("listRemoteServers") == 0 ) {
              reply = "";
              std::string remote = dc.ebsvolume_list("all", conf.VolumeFilePath);
				
              std::istringstream iss(remote);

              for (std::string line; std::getline(iss, line); ) {
                if (line.find("used") != std::string::npos ){
                  std::stringstream ss(line);
                  std::string a,b,c;
                  ss >> a >> b >> c;
                  reply += c + "\n";
                }
              }
              new_sock << reply;
              //new_sock.close_socket();
              //break;	 

            }else {
              reply = "unknown request, type help for list of commands\n\n";
              new_sock << reply;
              //break;
            } 
          } catch ( SocketException& e) {
            ack = "FAILED";
            //std::cout << "  DEBUG: Connection Closed: " << e.description() << "\n\n";
            logger.log("error", hostname, "Dispatcher", 0, "nkown error, connection Closed", "Debug");
            new_sock.close_socket();
            break;
          } // ~~~ end of inner try
        }
      } catch ( SocketException& e) {
      }
    } // ~~~ end of outer while
  } // ~~~ end of function

	

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
