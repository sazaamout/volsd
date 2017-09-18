// Definition of the ServerSocket class

#ifndef Disk_class
#define Disk_class

#include <string>
#include <stdio.h> // userd for popen()
#include <algorithm> // used for remove. for string_clean function
//#include <vector>
#include "Utils.h" 




class Disk
{
  private:
	bool debug;
	int max_wait_time;
	
	
	int exec(std::string& results, std::string cmd);
    int attach(std::string volume, std::string device, std::string instance_id, bool d);
    int detach(std::string volume, std::string instance_id, bool d);
    int status(std::string volume, bool d);
	int is_exist(std::string mountPoint, bool d);
	int is_used(std::string mountPoint, bool d);
	int make_filesystem(std::string device, bool d);
	std::string string_clean(std::string str);
	int mountfs(std::string device, std::string mountPoint, bool d);
	int umountfs(std::string mountPoint, bool d);
	int sync_filesystem(std::string targetFilesystem, std::string destinationFilesystem);
	std::string get_device(std::string devices);
	
  public:
    Disk (bool d);
    virtual ~Disk();
    
    //int mount(std::string volume, std::string mountPoint, std::string device, std::string instance_id);
    // this temprarly untill we merge disk with DiskController. We added the devices.
    int mount(std::string volume, std::string mountPoint, std::string instance_id,   std::string devices);
    //int mount(std::string volume, std::string mountPoint, std::string instance_id);
    int release_volume(std::string& v, std::string instance_id, std::string mountPoint, bool d);
    static int getDeviceName(std::string& output, std::string mountPoint);
    //std::string getVolumeId(std::string deviceName);
    std::string create_filesystem(std::string snapshotId);	
    
    std::string device;
};


#endif
