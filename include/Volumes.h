// Definition of the ServerSocket class

#ifndef Disks_class
#define Disks_class

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "Utils.h"
#include "Logger.h"
#include <unistd.h>  // for sleep
#include <map> 


class Volumes
{
  private:
    int transactionId;
    int         _max_wait_time;
    bool        _debug;
    std::string _rootMountDirectory;
    std::string _volumeFile;
    utility::Volume *vols;
    int counter;
    
    bool ebsvolume_dump_to_file(int transactionId);
  
    int attach(std::string volume, std::string device, std::string instance_id, bool d);
    int status(std::string volume, bool d, int transactionId);
    int is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int is_used(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int make_filesystem(std::string device, bool d);
    int sync_filesystem(std::string targetFilesystem, std::string destinationFilesystem);
    bool load();
    
  public:
    Volumes ();
    Volumes (bool debug);
    Volumes (bool debug, std::string rmd, std::string volumeFile); // for now, this is used by client
    Volumes (bool debug, std::string rmd, std::string volumeFile, int idelVolumes);
    virtual ~Volumes();

    int ebsvolume_load_from_file();
    //int ebsvolume_add(std::string latestSnapshot, std::string ec2InstanceId, int transactionId, Logger& logger);
    int ebsvolume_create(utility::Volume& volume, std::string& latestSnapshot, int transactionId, Logger& logger);
    int ebsvolume_attach(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger);
    int ebsvolume_mount(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger);
    int mount(utility::Volume& volume, int transactionId, Logger& logger);
    void remount(Logger &logger);
    int ebsvolume_umount(utility::Volume& volume, int transactionId, Logger& logger);
    int ebsvolume_detach(utility::Volume& volume, int transactionId, Logger& logger);
    int ebsvolume_delete(utility::Volume& volume, int transactionId, Logger& logger);
    //int ebsvolume_remove(utility::Volume& volume, int transactionId, Logger& logger);
    void printxyz();
    // -----------------------------------
    // move these to sync class
    int ebsvolume_sync(std::stringstream& ss, std::string op, std::string path, std::string source, Logger& logger);
    int ebsvolume_sync(std::string destination, std::string source, int transcationId, Logger& logger);
    int ebsvolume_sync(std::string source, int transcationId, Logger& logger);
    // -----------------------------------
    
    
    int ebsvolume_idle_number();
    int ebsvolume_exist(std::string volId);
    
    bool ebsvolume_idle(utility::Volume& v, int transactionId, Logger& logger);
    bool ebsvolume_setstatus(std::string , std::string, std::string, std::string, std::string, std::string, int transactionId, Logger& logger );
    bool ebsvolume_idle(std::string& results);
    
    static std::string ebsvolume_list(std::string select, std::string volumeFile);
    std::string ebsvolume_local_mountpoint_list();
    std::string ebsvolume_global_mountpoint_list();
  
    void ebsvolume_print();
    
    int remove_disk(std::string vol);
    int release_volume(std::string& v, std::string instance_id, std::string mountPoint, bool d, int transactionId, Logger& logger);
    int remove_mountpoint(std::string mp, int transactionId, Logger& logger);
    int wait(std::string op, utility::Volume volume, int transactionId, Logger& logger);
	
	// -----------------------------------
	// move this to utililty functions
    std::string get_devices();
    std::string get_device();
    std::string get_device(std::vector<std::string>& list);
    std::string create_filesystem(std::string snapshotId);  
    // -----------------------------------
        
};


#endif
