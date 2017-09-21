#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

#include "Disks.h"
#include "Utils.h"
#include "Snapshots.h"
#include "Logger.h"

using namespace utility;
using namespace std;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 GLOBALS VARAIBLES AND STRUCTURES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */ 
  std::mutex m;

  int InprogressDisksNo;
  std::string conf_file = "etc/server.conf";
  std::string instance_id;
  std::string hostname; 

  //bool snapshotManager_status = true;
  //std::string snapshotManager_msg;

  utility::Configuration conf;

  std::vector<std::string>  devices_list;

  bool _onscreen = false;
  std::string _conffile;
  int _loglevel;


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  int idle_disks();
  void createDisk_task(Snapshots& s, Disks& dc);
  void createSnapshot_task(Snapshots& sshot, int snapshotMaxNumber, std::string snapshotFile, int snapshotFreq);
  void cleaner_task(Disks& dc);
  void removeDisk_task(Snapshots& s, Disks& dc );
  int parse_arguments(int argc, char* argv[]);
  

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
    std:: cout << "failed to load configurations\n";
    return 1;
  }

  _loglevel = conf.ControllerLoglevel;
  //utility::print_configuration(conf);

  //logger.set(_onscreen, conf.ManagerLogFile, _loglevel);
  Logger logger(_onscreen, conf.ManagerLogFile, _loglevel);
  
  
  
  // no disks are being created
  InprogressDisksNo = 0;
  
  hostname    = utility::get_hostname();
  instance_id = utility::get_instance_id();
  
  Disks diskcontroller(true, conf.TempMountPoint, conf.VolumeFilePath);
  
  // load the snapshots
  logger.log("info", hostname, "EBSManager", 0, "loading snapshots ...");
  Snapshots sshot(conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
    
  // start a thread to create a snapshot every 4 hours
  logger.log("info", hostname, "EBSManager", 0, "starting the snapshot manager...");
  std::thread snapshotManager_thread(createSnapshot_task, std::ref(sshot), conf.SnapshotMaxNumber, conf.SnapshotFile, conf.SnapshotFrequency);
  snapshotManager_thread.detach();
  
  // start the cleaner thread
  logger.log("info", hostname, "EBSManager", 0, "starting the disk cleaner manager...");
  std::thread cleaner_thread(cleaner_task, std::ref(diskcontroller) );
  cleaner_thread.detach();
  
  //logger.flush();
  
  int value;
  while (true) {
    
    value = diskcontroller.ebsvolume_idle_number() + InprogressDisksNo;
    
    if ( value < conf.MaxIdleDisk ){
      std::thread creatDisk_thread(createDisk_task, std::ref(sshot), std::ref(diskcontroller) );
      creatDisk_thread.detach();
    }
    
    if ( value > conf.MaxIdleDisk ){
      std::thread removeDisk_thread(removeDisk_task, std::ref(sshot), std::ref(diskcontroller) );
      removeDisk_thread.detach();
    }
    
    sleep(10);
  }  
  
  return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION 
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 
  /* ---------------------------------------------------------------------------
   CREATEDISK_TASK
   -------------------------------------------------------------------------- */
  void createDisk_task(Snapshots& s, Disks& dc) {
  
    Logger logger(_onscreen, conf.ManagerLogFile, _loglevel);
  
  
    int transaction = utility::get_transaction_id();
  
    std::string latestSnapshot = s.get_latest("id", logger);
  
    // set this varabile so that main wont create another thread to create new disk
    InprogressDisksNo++;
    
    utility::Volume v;
      
    /* -----------------------------------------------------------------   
     * 1) Prepare disk information.
     * ---------------------------------------------------------------*/
    // get a device. 
    logger.log("info", hostname, "EBSManager", transaction, "allocating a device.", "CreateDiskThread");
    v.device = dc.get_device(devices_list);
    devices_list.push_back(v.device);
    logger.log("info", hostname, "EBSManager", transaction, "device allocated:[" + v.device + "]", "CreateDiskThread");
    logger.log("info", hostname, "EBSManager", transaction, "Devices List:[" + utility::to_string(devices_list) + "]", "CreateDiskThread");
  
    // get mount point
    v.mountPoint = conf.TempMountPoint + utility::randomString();
    logger.log("info", hostname, "EBSManager", transaction, "allocating mounting point:[" +  v.mountPoint + "]", "CreateDiskThread");
    
    
    /* -----------------------------------------------------------------   
       * 2) Create Volume
   * ---------------------------------------------------------------*/   
    logger.log("info", hostname, "EBSManager", transaction, "create Volume from latest snapshot:[" + latestSnapshot +"]", "CreateDiskThread");
    if (!dc.ebsvolume_create(v, latestSnapshot, transaction, logger)){
      logger.log("error", hostname, "EBSManager", transaction, "FALIED to create a volume", "CreateDiskThread");
      //remove_device
      utility::remove_element(devices_list, v.device);
      InprogressDisksNo--;
      return;
    }
    logger.log("info", hostname, "EBSManager", transaction, "volume:[" + v.id + "] creation was successful", "CreateDiskThread");
  
    // add disk to file
    logger.log("info", hostname, "EBSManager", transaction, "adding volume to volume file", "CreateDiskThread");
    v.attachedTo = "localhost";
    m.lock();
    dc.ebsvolume_setstatus( "add", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
    m.unlock();


    /* -----------------------------------------------------------------   
     * 3) Attach Volume
     * ---------------------------------------------------------------*/ 
    logger.log("info", hostname, "EBSManager", transaction, "attaching new disk to localhost", "CreateDiskThread");
    if (!dc.ebsvolume_attach(v, instance_id, transaction, logger)){
      //logger("error", hostname, "EBSManager::thread", transaction, "failed to attaching new disk to localhost. Removing...");
      logger.log("error", hostname, "EBSManager", transaction, "FAILED to attaching new disk to localhost. Removing  from AWS space", "CreateDiskThread");
      if (!dc.ebsvolume_delete(v, transaction, logger)){
        logger.log("error", hostname, "EBSManager", transaction, "FAILED to delete volume from AWS space", "CreateDiskThread");
      }
      logger.log("info", hostname, "EBSManager", transaction, "volume was removed from AWS space", "CreateDiskThread");
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
      logger.log("debug", hostname, "EBSManager", transaction, "removing device", "CreateDiskThread");
      utility::remove_element(devices_list, v.device);
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing disk from file");
      logger.log("info", hostname, "EBSManager", transaction, "removing volume from volume file", "CreateDiskThread");
      m.lock();
      dc.ebsvolume_setstatus( "delete", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
      m.unlock();
      InprogressDisksNo--;
      return;
    }
    logger.log("info", hostname, "EBSManager", transaction, "volume was attached successfully", "CreateDiskThread");
    sleep(5);
    
    /* -----------------------------------------------------------------   
     * 4) mount Volume
     * ---------------------------------------------------------------*/ 
    logger.log("info", hostname, "EBSManager", transaction, "mounting volume into localhost", "CreateDiskThread");
    if (!dc.ebsvolume_mount(v, instance_id, transaction, logger)){
    
      //logger("error", hostname, "EBSManager::thread", transaction, "failed to mount new disk to localhost. Detaching...");
      logger.log("error", hostname, "EBSManager", transaction, "FAILED to mount new disk to localhost. Detaching...", "CreateDiskThread");
      if (!dc.ebsvolume_detach(v, transaction, logger))
        logger.log("error", hostname, "EBSManager", transaction, "FAILED to detach.", "CreateDiskThread");
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
      logger.log("info", hostname, "EBSManager", transaction, "removing device", "CreateDiskThread");
      utility::remove_element(devices_list, v.device);
    
      //logger("info", hostname, "EBSManager::thread", transaction, "removing disk from file");
      logger.log("info", hostname, "EBSManager", transaction, "removing disk from file", "CreateDiskThread");
      m.lock();
      dc.ebsvolume_setstatus( "delete", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
      m.unlock();
      InprogressDisksNo--;
      return;
    }
    logger.log("info", hostname, "EBSManager", transaction, "volume was mounted successfully into localhost", "CreateDiskThread");
  
  
    /* -----------------------------------------------------------------   
     * 5) Sync Volume
     * ---------------------------------------------------------------*/ 
    // sync it with the TargetFilesystemMountPoint
    // get all of the paths from conf.SyncRequestsFile, sync the volume 
    logger.log("info", hostname, "EBSManager", transaction, "syncing new volume", "CreateDiskThread");
    std::fstream myFile;
    std::string output, line, pathDate;
  
    // open a socket send a sync request
  
    // wait for resutls
  
  
    //if (!dc.ebsvolume_sync(v.mountPoint, conf.TargetFilesystemMountPoint, transaction, logger))
    //  logger.log("info", hostname, "EBSManager", transaction, "syncing new volume failed", "CreateDiskThread");
    logger.log("debug", hostname, "EBSManager", transaction, "EXECMD:[echo 'sync " + v.mountPoint + " ' > /dev/tcp/infra1/" + utility::to_string( conf.SyncerServicePort ), "CreateDiskThread");
    int res = utility::exec(output, "echo 'sync " + v.mountPoint + "' > /dev/tcp/infra1/" + utility::to_string( conf.SyncerServicePort ) );
  
    if (!res){
      logger.log("debug", hostname, "EBSManager", transaction, "syntax error in command", "CreateDiskThread");
    }
  
  
    logger.log("info", hostname, "EBSManager", transaction, "adding disk to file and changing status", "CreateDiskThread");
    m.lock();
    dc.ebsvolume_setstatus( "update", v.id, v.status, v.attachedTo, v.mountPoint, v.device, transaction, logger);
    m.unlock();
  
    //logger("info", hostname, "EBSManager::thread", transaction, "Printing disk tables");
    //logger.log("info", hostname, "EBSManager", transaction, "Printing disk tables", "CreateDiskThread");
    //dc.ebsvolume_print();

    //logger.flush();
  
    InprogressDisksNo--;


    // now that volume is mounted, there is no need to keep the device in the devices_list since the get_device function (Disks Class) will read all of the mounted
    // devices on the server.
    //remove_device
    utility::remove_element(devices_list, v.device);

  }

  void createSnapshot_task(Snapshots& sshot, int snapshotMaxNumber, std::string snapshotFile, int snapshotFreq)
  {
    Logger logger(_onscreen, conf.ManagerLogFile, _loglevel);
    std::string output;
    
    logger.log("info", hostname, "EBSManager", 0, "snapshot manager started", "CreateSnapshotThread");  
    //if (snapshotManager_status) {
    while (true) {
      sshot.create_snapshot(conf.TargetFilesystem, snapshotFreq, logger);
      //logger.flush();
      sleep(snapshotFreq*60);
    }
  //}
  }


  /* ---------------------------------------------------------------------------
   REMOVEDISK_TASK
   -------------------------------------------------------------------------- */
  void removeDisk_task(Snapshots& s, Disks& dc ){
    Logger logger(_onscreen, conf.ManagerLogFile, _loglevel);
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


  /* ---------------------------------------------------------------------------
   CLEANER_TASK
   -------------------------------------------------------------------------- */
  void cleaner_task(Disks& dc) {
  
    int transaction = utility::get_transaction_id();
    Logger logger(_onscreen, conf.ManagerLogFile, _loglevel);
  
    logger.log("info", hostname, "EBSManager", transaction, "started", "CleanerThread");
  
    logger.log("info", hostname, "EBSManager", transaction, "finished", "CleanerThread");
  
    //logger.flush();
  }


  /* ---------------------------------------------------------------------------
   PARSE_ARGUMENT
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
  


