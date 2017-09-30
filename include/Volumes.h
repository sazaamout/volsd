// Definition of the ServerSocket class

#ifndef Disks_class
#define Disks_class

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>  // for sleep
#include <map> 
#include <mutex>

#include "Utils.h"
#include "Logger.h"
#include "Sync.h"


class Volumes
{
  private:
    int transactionId;
    std::string _rootMountDirectory;
    std::string _volumeFile;
    std::vector<utility::Volume> m_volumes;
    std::vector<std::string> m_devicesOnHold;
    std::mutex m;
    
    
    Logger *logger;
  
    int attach(std::string volume, std::string device, std::string instance_id, bool d);
    int status(std::string volume, bool d, int transactionId);
    int is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int is_used(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int make_filesystem(std::string device, bool d);
    int sync_filesystem(std::string targetFilesystem, std::string destinationFilesystem);
    bool load();
    
    utility::Volume* resize(int resize_amount);
    bool write_to_file( int transactionId );
    bool ebsvolume_dump_to_file(int transactionId);
    
  public:
    Volumes ();
    Volumes (std::string rmd, std::string volumeFile); // for now, this is used by client
    Volumes (std::string rmd, std::string volumeFile, int idelVolumes);
    ~Volumes();
     
    void set_logger_att ( bool toScreen, std::string logFile, int loglevel );
    int ebsvolume_load_from_file();
        
    int create( std::string &t_volumeId, const std::string t_latestSnapshot, const int t_transactionId );
    int ebsvolume_create(utility::Volume& volume, std::string& latestSnapshot, int transactionId, Logger& logger);
    
    
    int attach( const std::string t_volumeId, const std::string t_device, const std::string t_instanceId, const int t_transcation );    
    int ebsvolume_attach(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger);

    
    int mount(const std::string t_volumeId, const std::string t_mountPoint, const std::string t_device, const int t_transactionId );
    int ebsvolume_mount(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger);
        
    bool umount( const std::string t_volumeId, const std::string t_mountPoint, const int t_transactionId );
    void remount();
    int ebsvolume_umount(utility::Volume& volume, int transactionId, Logger& logger);
    
    int detach( const std::string t_idleVolId, const int t_transactionId );
    int ebsvolume_detach(utility::Volume& volume, int transactionId, Logger& logger);
        
    int del(const std::string t_volumeId, int t_transactionId);
    int ebsvolume_delete(utility::Volume& volume, int transactionId, Logger& logger);
    
    
    
    void printxyz();
    // -----------------------------------
    // move these to sync class
    int ebsvolume_sync(std::stringstream& ss, std::string op, std::string path, std::string source, Logger& logger);
    int ebsvolume_sync(std::string destination, std::string source, int transcationId, Logger& logger);
    int ebsvolume_sync(std::string source, int transcationId, Logger& logger);
    // -----------------------------------
    
    
    
    int ebsvolume_exist(std::string volId);
    
    
    
    bool ebsvolume_setstatus(std::string , std::string, std::string, std::string, std::string, std::string, int transactionId, Logger& logger );
    
    // m_volumes modifiers function
    int update ( const std::string volumeId, const std::string key, const std::string value , const int transactionId);
    int remove ( const std::string t_volumeId, const int t_transactionId );
    int add ( const utility::Volume t_volumes, const int t_transactionId );
    
    int get_idle_number();
    int get_idle_volume( int &idleVolumeIndex, const int transactionId );
    bool ebsvolume_idle(std::string& results);
    
    static std::string ebsvolume_list(std::string select, std::string volumeFile);
    std::string ebsvolume_local_mountpoint_list();
    std::string ebsvolume_global_mountpoint_list();
  
    void ebsvolume_print();
    
    int remove_disk(std::string vol);
    int release_volume(std::string& v, std::string instance_id, std::string mountPoint, bool d, int transactionId, Logger& logger);
    
    int release (std::string &volumeId, int transactionId);
    int acquire( const std::string t_targetFileSystem, const std::string t_snapshotId, const std::string t_rootMountsFolder, const std::string t_instanceId, const int t_transaction );
    
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