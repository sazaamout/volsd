
#ifndef Utility_namespace
#define Utility_namespace


#include <string>
#include <fstream> // load_configuration
#include <algorithm> // remove function from std
#include <sstream> //ostringstream
#include <stdio.h> // used for popen()
#include <ctime>  // used: logger, randomString
#include <cstring> // used: logger
#include <iostream> // used: logger
#include <stdlib.h>  // used: get_transaction_id(rand), to_int(atoi)
#include <math.h>       /* floor */
#include <iomanip>      // std::setfill, std::setw
#include <vector>

#include <sys/stat.h> // used for is_file and is_dir, and for creating a dir
#include <dirent.h>   // used for folder_is_empty
#include <mntent.h>   // used for is_mount function
#include <sys/mount.h>
#include <errno.h>
#include <unistd.h> // for rmdir

namespace utility
{
	struct Volume {
    std::string id;
    std::string status;
    std::string attachedTo;
    std::string mountPoint;
    std::string device;
  };
    
  struct Port{
	 int portNo;
	 bool status;
	};

    
  struct Configuration {
	  std::string Hostname;
    int SnapshotFrequency;
    std::string SnapshotFile; 
	  int SnapshotMaxNumber;
	  std::string TargetFilesystem;
	  std::string TargetFilesystemMountPoint;
    std::string TargetFilesystemDevice;
	  std::string TempMountPoint;
	  std::string VolumeFilePath;
	  int MaxIdleDisk;
	  std::string ManagerLogFile;
	  std::string DispatcherLogPrefix;
	  std::string ClientLogFile;
	  
	  int DispatcherLoglevel;
	  int ClientLoglevel;
    
    // sync
	  std::string SyncLogPrefix;
		int         SyncLogLevel;
	  int         SyncServicePort;
    std::string SyncVolumes;
	  int         SyncVolumesInterval;
	  std::string SyncRequestsFile;
	  std::string SyncDatesFile;
	  std::string SyncErrorEmailTo;
	  std::string SyncOutputEmailTo;
    std::string EmailSyncOutput;
    std::string EmailSyncError;
	  std::string RemoteRsyncCommand;
	  std::string LocalRsyncCommand;
    std::string EmailPushOutput;
    std::string EmailPushError;
    std::string EmailPushEmail;

	};
	

	void clean_string(std::string& str);
	std::string get_dateTime();
  std::string unixTime ();
	std::string get_dateTime(int year, int month, int day, int hour, int min, int sec);
	std::string substract_dateTime(int min);
	int get_transaction_id();
	std::string to_string(const int value);
	int to_int(std::string str);
	int exec(std::string& results, std::string cmd);
	int exec1(std::string& results, std::string cmd);
	std::string get_instance_id();
	std::string randomString();
	
  
	int load_configuration(Configuration &conf, std::string conf_file);
	void print_configuration(Configuration &conf);
	int remove_element( std::vector<std::string>& v, std::string element);	
	void print_elements(std::vector<std::string>& v);
	std::string to_string(std::vector<std::string>& v);
	
	std::string datetime();
	int datetime_diff( std::string time1, std::string time2 );
	int is_root();

  bool is_file(const char* path);
	bool file_create(std::string path);
	bool is_exist(std::string path);
	bool is_exist(std::string path, std::string ip);
  bool is_empty(std::string path);
  
	bool is_dir(const char* path);
	bool folder_exist(std::string prefix);
  bool folders_create(std::string prefix, std::size_t pos = 1);
  bool folder_create(std::string path);
  bool folder_is_empty(const std::string dirname);
  bool folder_remove( const std::string dirname);
  bool mountfs ( std::string &output, std::string mountPoint, std::string device );
  bool umountfs( std::string &output, std::string mountPoint );
  bool is_mounted( const std::string dir);
  
	int send_email(std::string title, std::string message, std::string to);
	std::string rsync_errorCodetoString(int errorCode);
	std::string get_hostname();


}

#endif
