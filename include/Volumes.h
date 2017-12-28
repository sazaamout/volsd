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
    
    int is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int is_used(std::string mountPoint, bool d, int transactionId, Logger& logger);
    int sync_filesystem(std::string targetFilesystem, std::string destinationFilesystem);
    bool load();
    
    utility::Volume* resize(int resize_amount);
    bool write_to_file( int t_transactionId );
    bool ebsvolume_dump_to_file(int transactionId);
    
  public:
    Volumes ();
    Volumes (std::string rmd, std::string volumeFile); // for now, this is used by client
    Volumes (std::string rmd, std::string volumeFile, int idelVolumes);
    ~Volumes();
     
    void set_logger_att ( bool toScreen, std::string logFile, int loglevel );

    int create( std::string &t_volumeId, const std::string t_latestSnapshot, 
                const int t_transactionId );
    
    int attach( const std::string t_volumeId, const std::string t_device, 
                const std::string t_instanceId, const int t_transcation );    
    
    int mount( const std::string t_volumeId, const std::string t_mountPoint, 
               const std::string t_device, const int t_transactionId ); 
    
    int mount( const std::string t_volumeId,  const std::string t_mountPoint, 
               const std::string t_device,    const std::string t_fsType, 
               const std::string t_mntflags, const int t_transactionId );
    
    bool umount( const std::string t_volumeId, const std::string t_mountPoint, 
                 const int t_transactionId );
    void remount();
    
    int detach( const std::string t_idleVolId, const int t_transactionId );
    
    int del(const std::string t_volumeId, int t_transactionId);
        
    // m_volumes modifiers function
    int update ( const std::string volumeId, const std::string key, const std::string value , 
                 const int transactionId, std::string t_ip = "", std::string t_remoteMountPoint = "", 
                 std::string t_device = "" );
                 
    int remove ( const std::string t_volumeId, const int t_transactionId );
    int add ( const utility::Volume t_volumes, const int t_transactionId );
    int volume_exist( const std::string volId );    
    
    int get_idle_number();
    int get_idle_volume( int &idleVolumeIndex, const int transactionId );
    
    int remove_disk(std::string vol);
    int release_volume( std::string& v, std::string t_instanceId, std::string t_mountPoint, 
                             int t_transactionId );
    
    int release (std::string &volumeId, int t_transactionId);
    int acquire( const std::string t_targetFileSystem, const std::string t_snapshotId, 
                 const std::string t_rootMountsFolder, const std::string t_instanceId, 
                 const int t_transaction );
    
    int remove_mountpoint(std::string mp, int transactionId, Logger& logger);
    int wait(std::string op, utility::Volume volume, int transactionId, Logger& logger);
	
    std::string get_device();
    std::string get_device(std::vector<std::string>& list);
     
    void printxyz();
    
    std::vector<utility::Volume>& get_list();
  
    int is_attached( std::string t_instanceId, std::string t_volumeId, std::string t_device, int t_transactionId, 
                     Logger& logger);

};


#endif
