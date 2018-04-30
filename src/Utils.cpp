
#include <string>
#include "Utils.h"


namespace utility
{


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // CLEAN_STRING FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  void clean_string(std::string& str) {
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // GET_TRANSACTION_ID FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int get_transaction_id() {
    // This function uses:
    // #include <stdlib.h> // rand 
    srand(time(NULL));
    return rand() % 10000;  
  }
  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // TO_STRING FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string to_string(const int value){
    std::ostringstream convert;   // stream used for the conversion
    convert << value;      // insert the textual representation of 'Number' in the characters in the stream
    return convert.str(); // set 'Result' to the contents of the stream
  
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // TO_STRING FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  std::string to_string(std::vector<std::string>& v){
    std::string str;
    for(std::vector<std::string>::iterator it = v.begin(); it != v.end(); ++it) {
      if ( it == v.end() - 1 ){
        str.append(*it);
      } else {
        str.append(*it + " " );
      }
    }
    return str;
  }
  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // TO_INT FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int to_int(std::string str){
    return  atoi(str.c_str());
  }
  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // EXEC FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int exec(std::string& results, std::string cmd) {
    //std::cout << std::flush;
    FILE *in;
    char buff[512];
    
    cmd.append(" 2>&1");
    
    if(!(in = popen(cmd.c_str(), "r"))){
      // did not execut corredctly
      return false;
    }
  
    // convert to std::string type
    while(fgets(buff, sizeof(buff), in)!=NULL){
      results = results + buff + " ";
    }
    // success
    int exit_status = pclose(in);
    
    if (exit_status == 0) return true; // return true
    else return false; // retrun false
    
  }
 
   
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // EXEC FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int exec1(std::string& results, std::string cmd) {
    //std::cout << std::flush;
    FILE *in;
    char buff[512];
    
    cmd.append(" 2>&1");
    
    if(!(in = popen(cmd.c_str(), "r"))){
      // did not execut corredctly
      return false;
    }
  
    // convert to std::string type
    while(fgets(buff, sizeof(buff), in)!=NULL){
      results = results + buff + " ";
    }
    // success
    int exit_status = pclose(in);
    
    if (exit_status == 0) 
      return true; // return true
    else 
      return exit_status; // retrun false
    
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // GET INSTNACE ID FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string get_instance_id(){
    std::string output;
    exec(output, "curl -s http://169.254.169.254/latest/meta-data/instance-id");
    clean_string(output); 
    return output;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // GET INSTNACE AVAILABILIT ZONE
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string get_instance_zone(){
    std::string output;
    exec(output, "curl -s http://169.254.169.254/latest/meta-data/placement/availability-zone");
    clean_string(output); 
    return output;
  }



  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // GET INSTANCE TYPE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string get_instance_type(){
    std::string output;
    exec(output, "curl -s http://169.254.169.254/latest/meta-data/instance-type");
    clean_string(output); 
    return output;
  }  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // LOGGER FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string randomString() { 
    srand(time(NULL));
    std::string charIndex = "abcdefghijklmnaoqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    int length = 10; 
    // length of the string is a random value that can be up to 'l' characters.

    int ri[10]; 
    /* array of random values that will be used to iterate through random 
      indexes of 'charIndex' */

    for (int i = 0; i < length; ++i) 
      ri[i] = rand() % charIndex.length();
    // assigns a random number to each index of "ri"

    std::string rs = ""; 
    // random string that will be returned by this function

    for (int i = 0; i < length; ++i) 
      rs += charIndex[ri[i]];
  
    return rs;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // PRINT_CONFIGURATION FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  void print_configuration(Configuration& conf){
    
    std::cout << "\n=========================================================\n";
    std::cout << "Configurations\n";
    std::cout << "=========================================================\n";
    std::cout << "InstanceId:\t\t"        << utility::get_instance_id()      << std::endl;
    std::cout << "Hostname:\t\t"          << conf.Hostname                   << std::endl;
    std::cout << "snapshot freq:\t\t"     << conf.SnapshotFrequency          << std::endl;
    std::cout << "snapshot file:\t\t"     << conf.SnapshotFile               << std::endl;
    std::cout << "MaxIdleDisks:\t\t"      << conf.MaxIdleDisk                << std::endl;
    std::cout << "Taget Filesystme:\t"    << conf.TargetFilesystem           << std::endl;
    std::cout << "Taget volume mp:\t"     << conf.TargetFilesystemMountPoint << std::endl;
    std::cout << "Temp mounting point:\t" << conf.TempMountPoint             << std::endl;
    std::cout << "max Snapshot Number:\t" << conf.SnapshotMaxNumber          << std::endl;
    std::cout << "Volume file path:\t"    << conf.VolumeFilePath             << std::endl;
    std::cout << "Manager log file:\t"    << conf.ManagerLogFile             << std::endl;
    std::cout << "DispatcherLogPrefix:\t" << conf.DispatcherLogPrefix        << std::endl;
    std::cout << "Client log file:\t"     << conf.ClientLogFile              << std::endl;
    std::cout << "DispatcherLoglevel:\t" << conf.DispatcherLoglevel         << std::endl;
    std::cout << "Client loglevel:\t"     << conf.ClientLoglevel             << std::endl;
    std::cout << "=========================================================\n\n";
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // LOAD CONFIGURATIONS FUNCTION 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //
  int load_configurations ( Configuration &conf, std::string conf_file ){
    
    std::fstream myFile;
    myFile.open(conf_file.c_str());
    if (!myFile.is_open()) 
      return 0;

    std::string line;
    std::string key, value;
    while (std::getline(myFile, line)) {

      if ( (line[0] == '#') || (line[1] == '#') || (line[0] == '\n') || (line == "")){
        continue;
      }   

      line  = trim(line);
      // this will return the first space
      size_t fpos = line.find(" ");
      if ( fpos != std::string::npos ) {
        key   = line.substr(0, fpos);
        value = line.substr(fpos+1);
      
        key   = trim(key);
        value = trim(value);

        if ( key == "NFSMountFlags" ){
          //unsigned long mf; 
          //utility::convert_mount_flags(mf, value);
          //conf.NFSMountFlags = mf;
          conf.NFSMountFlags = value;
        } else if ( key == "ServerIP" ){
          conf.ServerIP = value;
        } else if ( key == "ServerPort" ){
          conf.ServerPort = utility::to_int(value);
        } else if ( key == "LogLevel" ){
          conf.LogLevel = utility::to_int(value);
        } else if ( key == "LogFile" ){
          conf.LogFile = value;
        } else if ( key == "TargetFSMountPoint" ){
          conf.TargetFSMountPoint = value;
        } else if ( key == "TargetFSDevice" ){
          conf.TargetFSDevice = value;
        } else if ( key == "TargetFSType" ){
          conf.TargetFSType = value;  
        } else if ( key == "ForceMount" ){
          conf.ForceMount = value;
        } else if ( key == "Aws_Cmd" ){
          conf.Aws_Cmd = value;
        } else if ( key == "Aws_Region" ){
          conf.Aws_Region = value;
        } else if ( key == "Aws_ConfigFile" ){
          conf.Aws_ConfigFile = value;
        } else if ( key == "Aws_CredentialsFile" ){
          conf.Aws_CredentialsFile = value;
        } else {
          continue;
        }
      }  

      

    }//~~End While Loop ~~//
  
    return 1;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // LOAD_CONFIGURATION FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int load_configuration(Configuration &conf, std::string conf_file){
    
    std::fstream myFile;
    myFile.open(conf_file.c_str());
    
    if (!myFile.is_open()) 
      return 0;
    
    
    std::string line;
    std::string key, value;
    while (std::getline(myFile, line))
    {
      if ( (line[0] == '#') || (line[1] == '#') || (line[0] == '\n') || (line == "")){
        continue;
      } 
        std::stringstream ss(line);
        ss >> key >> value;
      
      if ( key == "Hostname") {
        conf.Hostname = value;
      }else if (key == "MaxIdleDisk") 
        conf.MaxIdleDisk = utility::to_int(value);
      else if ( key == "TargetFilesystemMountPoint" ){
        if ( value[value.length()-1] != '/' )
          value = value + '/';
        conf.TargetFilesystemMountPoint = value;
      }else if (key == "TargetFilesystemDevice" ) 
        conf.TargetFilesystemDevice = value;
      else if (key == "TargetFilesystem" ) 
        conf.TargetFilesystem = value;
      else if ( key == "TempMountPoint" ) {
        if ( value[value.length()-1] != '/' )
          value = value + '/';
        conf.TempMountPoint = value;
      }else if ( key == "SnapshotFrequency" ) 
        conf.SnapshotFrequency = utility::to_int(value);
      else if ( key == "SnapshotFile" ) 
        conf.SnapshotFile = value;
      else if ( key == "SnapshotFileStorage" ) 
        conf.SnapshotFileStorage = utility::to_int(value);
      else if ( key == "SnapshotMaxNumber" ) 
        conf.SnapshotMaxNumber = utility::to_int(value);
      else if ( key == "VolumeFilePath" )  
        conf.VolumeFilePath = value;
      else if ( key == "ManagerLogFile" ) 
        conf.ManagerLogFile = value;
      else if ( key == "DispatcherLogPrefix" ) 
        conf.DispatcherLogPrefix = value;
      else if ( key == "ClientLogFile" ) 
        conf.ClientLogFile = value;
      else if ( key == "DispatcherLoglevel" ) 
        conf.DispatcherLoglevel = utility::to_int(value);
      else if ( key == "ClientLoglevel" ) 
        conf.ClientLoglevel = utility::to_int(value);
      else if ( key == "SyncVolumes" ) 
        conf.SyncVolumes = value;
      else if ( key == "SyncVolumesInterval" ) 
        conf.SyncVolumesInterval = utility::to_int(value);
      else if ( key == "SyncRequestsFile" ) 
        conf.SyncRequestsFile = value;
      else if ( key == "SyncLogPrefix" ) 
        conf.SyncLogPrefix = value;          
      else if ( key == "SyncLogLevel" ) 
        conf.SyncLogLevel = utility::to_int(value);          
      else if ( key == "SyncDatesFile" ) 
        conf.SyncDatesFile = value;
      else if ( key == "SyncServicePort" ) 
        conf.SyncServicePort = utility::to_int(value);
      else if ( key == "SyncOutputEmailTo" ) 
        conf.SyncOutputEmailTo = value;
      else if ( key == "SyncErrorEmailTo" ) 
        conf.SyncErrorEmailTo = value;
      else if ( key == "EmailSyncOutput" ) 
        conf.EmailSyncOutput = value;
      else if ( key == "EmailSyncError" ) 
        conf.EmailSyncError = value;
      else if ( key == "LocalRsyncCommand" ) 
        conf.LocalRsyncCommand = value;
      else if ( key == "RemoteRsyncCommand" ) 
        conf.RemoteRsyncCommand = value;
      else if ( key == "EmailPushOutput" ) 
        conf.EmailPushOutput = value;
      else if ( key == "EmailPushError" ) 
        conf.EmailPushError = value;
      else if ( key == "EmailPushEmail" ) 
        conf.EmailPushEmail = value;
      else if ( key == "RemoteMountPoint" ){
        if ( value[value.length()-1] != '/' )
          value = value + '/';
        conf.RemoteMountPoint = value;
      }else if ( key == "NFSMountFlags" ){
        value = trim(value);
        std::vector<std::string> v = explode( value, ',' );
        for (int i=0; i<v.size(); i++)
          std::cout << v[i] << '\n';
      }else 
        continue;

      line="";
      
    }
    
    myFile.close(); 
    
    return 1;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // REMOVE_ELEMENTS FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int remove_element(std::vector<std::string>& v, std::string element){
    std::vector<std::string>::iterator pos = std::find(v.begin(), v.end(), element);
    if (pos != v.end())
      v.erase(pos);
    return 1;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // PRINT_ELEMENTS FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // this is not used, keep it for future use
  void print_elements(std::vector<std::string>& v){
    for(std::vector<std::string>::iterator it = v.begin(); it != v.end(); ++it) {
      std::cout << *it << " ";
    }
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DATETIME FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // this is not used, keep it for future use
  std::string datetime (){
    time_t now_t = time(0);
    struct tm *now_tm = localtime( &now_t );
    
    char buffer [80];
    strftime (buffer,80,"%Y-%m-%d %H:%M:%S",now_tm);
    
    return std::string(buffer);
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Unix Time FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  std::string unixTime (){
    //std::time_t result = std::time(nullptr);
    std::time_t result = std::time(NULL);
    return to_string(result);
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DATETIME_DIFF FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // this is not used, keep it for future use
  int datetime_diff ( std::string time1, std::string time2 ) {
	  
    struct tm tm1, tm2; 
    time_t t1, t2; 

    //(1) convert `String to tm`:  (note: %T same as %H:%M:%S)  
    if(strptime(time1.c_str(), "%Y-%m-%d %H:%M:%S", &tm1) == NULL)
       printf("\nstrptime failed-1\n");          
    if(strptime(time2.c_str(), "%Y-%m-%d %H:%M:%S", &tm2) == NULL)
       printf("\nstrptime failed-2\n");

    //(2) convert `tm to time_t`:    
    t1 = mktime(&tm1);   
    t2 = mktime(&tm2);  

    
    return difftime(t2, t1);
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // IS_ROOT FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int is_root(){
    //TODO: 
    // there is a better way to do this
    // #include <unistd.h>
	// uid_t getuid(void);
	// https://stackoverflow.com/questions/4159910/check-if-user-is-root-in-c
	
    // check if running as root
    std::string output;
    utility::exec(output, "whoami");
    utility::clean_string(output);
    
    if (output == "root")
      return 1;
    else 
      return 0;
    
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // IS_FILE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool is_file(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISREG(buf.st_mode);
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // IS_DIR FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool is_dir(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISDIR(buf.st_mode);
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // SEND_MAIL FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // this is not used
  int send_email(std::string title, std::string message, std::string to){
    std::string output;
    int res = utility::exec(output, "echo \"" + message + "\" | tr -cd '\11\12\15\40-\176' | mail -s \"" + title + "\" " + to);
    
    if (res){
      return 1;
    }else{
      return 0;
    }
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // IS_EXIST FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // used to check if files/directories exist
  bool is_exist(std::string path) {
    struct stat buffer;   
    return (stat (path.c_str(), &buffer) == 0); 
  }
  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // IS_EXIST FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool is_exist(std::string path, std::string ip){
    std::string output;
    
    int res = utility::exec(output, "ssh -o StrictHostKeyChecking=no " + ip + " 'stat " + path + "'");
    
    if (res)
      return 1;
    else
      return 0;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Folders Exist FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool folders_create(std::string prefix, std::size_t pos) {

    if ( prefix[ prefix.length()-1 ] != '/' ) {
      prefix = prefix + '/';
    }

    std::size_t found = prefix.find('/', pos);

    while (found!=std::string::npos) {
      //std::cout << "found at pos: " <<  found << "\n";;
      std::string substr = prefix.substr(0, found);
      //std::cout << "substring string[" << substr <<"]\n";

     if (!is_exist(substr)){
        //std::cout << "[" << substr << "] is not found. Creating\n\n\n";
		if (!folder_create(substr)){
			return false;
		}
      }//else{
      //  std::cout << "[" << substr << "] is found.\n\n\n";
      //}
 
      found = prefix.find('/', found+1);
    }
    
    return true;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Folder Create FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool folder_create(std::string path) {
    const int dir_err = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == dir_err) {
      printf("error creating directory: %s\n", path.c_str());
      return false;
    }
    return true;
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Folder is Empty FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool folder_is_empty(const std::string dirname) {
    
    int n = 0;
    struct dirent *d;
    DIR *dir = opendir(dirname.c_str());
    if (dir == NULL) //Not a directory or doesn't exist
      return true;
    while ((d = readdir(dir)) != NULL) {
      if(++n > 2)
        break;
    }
    closedir(dir);
    if (n <= 2) //Directory Empty
      return true;
    else
      return false;
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MOUNT FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool mountfs( std::string &output, std::string mountPoint, std::string device ){
     
    const char* src  = device.c_str();
    const char* trgt = mountPoint.c_str();
    const char* type = "ext4";
    const unsigned long mntflags = 0;
    const char* opts = "";   /* 65534 is the uid of nobody */
    //std::cout << src << std::endl;
    //std::cout << trgt << std::endl;
    int result = mount(src, trgt, type, mntflags, opts);

    if (result == 0) {
      return true;
    } else {
      output = strerror(errno);
      return false;
    }

    return true;
  }
 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MOUNT FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool mountfs( std::string& error,   const char* t_src,      const char* t_target,
                const char* t_fsType, const char* t_mntflags, const char* t_opts ){

    // 1. first, we need to convert t_mnt_flags
    unsigned long mntflags = 0;
    char* fstype =  const_cast<char*>(t_fsType);
    
    if ( (t_mntflags != NULL) && (t_mntflags[0] != '\0') ){
      if (!utility::convert_mount_flags(mntflags, t_mntflags)){
        error = "unkown mounting flags options";
        return 0;
      }
    } else {
      mntflags = 0;
      fstype = (char*)"ext4";
    }
    
    // 2. now that they are converted, start mounting
    int result = mount(t_src, t_target, fstype, mntflags, t_opts);
    
    if (result == 0) {
      // if it is mounted, then an entry can be found in /proc/mounts file
      // finds it and get its options 
      struct mntent part;
      if ( mnt_find( part, "/proc/mounts", t_src, t_target ) ){
        mnt_add (&part, "/etc/mtab");
        mnt_add (&part, "/etc/fstab");
      }
      return true;
    } else {
      error = strerror(errno);
      return false;
    }       

    return true;
  }

  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MNT_ADD FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int mnt_add( struct mntent* part, std::string t_file ) {

    FILE * mtab = NULL; 
    //struct mntent* part = (struct mntent*)malloc(sizeof(struct mntent));
    
    //printf("Inside the mnt_add function. Adding\n");
    //printf("mnt_fsname: %s\n", part->mnt_fsname);
    //printf("mnt_dir: %s\n",    part->mnt_dir);
    //printf("mnt_type: %s\n",   part->mnt_type);
    //printf("mnt_opts: %s\n",   part->mnt_opts);
    
    int is_mounted = 0;

    //std::string file = "/etc/mtab";

    if ( ( mtab = setmntent (t_file.c_str(), "a") ) != NULL) 
    {
      //std::cout << "file was opened" << std::endl;

      int res = addmntent(mtab, part);  

      if (!res){ 
        //std::cout << "failed: " << res << "\n";
        return 0;
      } 

      endmntent ( mtab ); 
    } else {
      //std::cout << "could not open file\n";
      return 0;
    }

    return 1;
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MNT FIND FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int mnt_find( struct mntent& res, const std::string t_file, const std::string t_mnt_fsname, 
                const std::string t_mnt_dir ) {
      
    std::string mnt_dir = t_mnt_dir;
    if ( t_mnt_dir[t_mnt_dir.length()-1] == '/' ){
      mnt_dir = t_mnt_dir.substr(0, t_mnt_dir.length()-1);
    }
      
    
    FILE * mtab = NULL;
    int is_mounted = 0;
    struct mntent *part = NULL;
    if ( ( mtab = setmntent (t_file.c_str(), "r") ) != NULL) {
      while ( ( part = getmntent ( mtab) ) != NULL) {
        if ( ( strcmp ( part->mnt_dir   , mnt_dir.c_str() )    == 0 ) && 
            
               strcmp ( part->mnt_fsname, t_mnt_fsname.c_str() ) == 0 ) {
    
           
          endmntent ( mtab);
          res.mnt_fsname = part->mnt_fsname;
          res.mnt_dir = part->mnt_dir;
          res.mnt_type = part->mnt_type;
          res.mnt_opts = part->mnt_opts;
          res.mnt_freq = part->mnt_freq;
          res.mnt_passno = part->mnt_passno;
          return 1;
        }
      }
      endmntent ( mtab);
    }
    
    return 0;

  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // UMOUNT FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool umountfs( std::string &output, std::string mountPoint ){
    const char* trgt = mountPoint.c_str();
    int result = umount2(trgt, MNT_DETACH);

    if (result == 0) {
      // TODO: delete from /etc/mtab and /etc/fstab
      //mnt_delete( mountPoint );
      
      return true;
    } else {
      output = strerror(errno);
      return false;
    }

    return true;
  } 


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // is mounted FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool is_mounted( const std::string t_dir){
    std::string dir = t_dir;

    // if directory ends with '\', remove it
    if ( dir[dir.length()-1] == '/' )
      dir = dir.substr(0, dir.length()-1);

    FILE * mtab = NULL;
    struct mntent * part = NULL;
    int is_mounted = 0;
    if ( ( mtab = setmntent ("/etc/mtab", "r") ) != NULL) {
      while ( ( part = getmntent ( mtab) ) != NULL) {
        if ( ( part->mnt_dir != NULL )  && ( strcmp ( part->mnt_dir, dir.c_str() ) ) == 0 ) {
          is_mounted = 1;
        }
      }
      endmntent ( mtab);
    }

    return is_mounted;
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // is mounted FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool get_mntInfo( std::string& res, const std::string t_query, const std::string t_file, 
                    const std::string t_dir ){
    std::string dir = t_dir;

    // if directory ends with '\', remove it
    if ( dir[dir.length()-1] == '/' )
      dir = dir.substr(0, dir.length()-1);

    FILE * mtab = NULL;
    struct mntent * part = NULL;
    int is_mounted = 0;
    if ( ( mtab = setmntent (t_file.c_str(), "r") ) != NULL) {
      while ( ( part = getmntent ( mtab) ) != NULL) {
        if ( ( part->mnt_dir != NULL )  && ( strcmp ( part->mnt_dir, dir.c_str() ) ) == 0 ) {
          
          if (t_query == "device") {
            res = part->mnt_fsname;
          } 
          if (t_query == "flags") {
            res = part->mnt_opts;
          }  
          endmntent ( mtab);
          return 1;
          
        }
      }
      endmntent ( mtab);
    }

    return 0;
  }
  
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // File Create FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool file_create(std::string path){
    if (path == "")
      return false;
      
    
    std::size_t pos = path.rfind('/');
    //std::string file = path.substr(path.rfind('/')+1);
    std::string prefix   = path.substr(0, pos);
    
    // ensure that parent dir is created
    folders_create(prefix, 1);
    
    // create the file
    std::string output;
    int res = utility::exec(output, "touch " + path);
    
    if (res)
      return 1;
    else
      return 0;
      
    return 1;
  }
  
  bool create_file(std::string path, std::string contents){
    std::ofstream ofs;
    ofs.open (path.c_str(), std::ofstream::out | std::ofstream::trunc);
    ofs << contents;
    ofs.close();
  }
  
  bool get_volume_info( std::string path, std::string &volumeId, std::string &device){

      // open the file for reading
      std::ifstream ifs;
      ifs.open(path.c_str());
      if (ifs.is_open())
      {
        std::string line;
        getline(ifs, line);

        ifs.close();
        if (line.length() == 0 ) {
          return false;
        }

        std::vector<std::string> v = explode(line, ' ');
        
        volumeId = v[0];
        device   = v[1];
        return true;
      } else {
        return false;
      }

  }

  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // FOLDER REMOVE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // remove an emoty directory. If has some data, it will not be removed
  bool folder_remove( const std::string dirname){
    if ( rmdir(dirname.c_str()) == -1) {
      return false;
    }
    return true;
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // FOLDER REMOVE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // remove an none empty dir
  bool folder_remove2( const std::string dirname ){
    
    if ( ( dirname.empty() ) || ( dirname == "/" ) ) 
      return false;
      
    std::string output;
    int res = exec1(output, "rm -rf " + dirname );
    
    if ( ! res ) {
      return false;
    }
    return true;
  }
  
    
  bool is_empty(std::string path){
    std::fstream myFile;
    myFile.open(path.c_str());
    
    if ( myFile.peek() == std::ifstream::traits_type::eof() )
      return true;
    
    return false;  
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // GET_HOSTNAME FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string get_hostname(){
    std::string output;
    exec(output, "hostname -s");
    clean_string(output);
    return output;
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // EXPLODE FUNCTION 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::vector<std::string> explode( const std::string& str, const char token ){

    std::vector<std::string> v; 
    std::string substring;
    std::istringstream ss(str);

    while(std::getline(ss, substring, token)) {
      v.push_back(substring);
    }   

    return v; 

  }

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // TRIM FUNCTION
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clean white spaces only from fornt and end of a given string
  std::string trim(std::string const& str)
  {
    if(str.empty())
      return str;

    std::size_t firstScan = str.find_first_not_of(' ');
    std::size_t first     = firstScan == std::string::npos ? str.length() : firstScan;
    std::size_t last      = str.find_last_not_of(' ');
  
    return str.substr(first, last-first+1);
  } 

 
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // CONVERT MOUNT FLAGS
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  unsigned long convert_mount_flags ( unsigned long& res, const std::string& t_mountflags) {
    
    // string is comma separated, put it in to vector
    std::vector<std::string> v = explode(t_mountflags, ',');
    res = 0;
    for ( int i=0; i< v.size(); i++ ){
      if ( v[i] == "MS_REMOUNT" ){
        res = res | MS_REMOUNT;
      }else if ( v[i] == "MS_BIND" ){
        res = res | MS_BIND;
      }else if ( v[i] == "MS_SHARED" ){
        res = res | MS_SHARED;
      }else if ( v[i] == "MS_PRIVATE" ){
        res = res | MS_PRIVATE;
      }else if ( v[i] == "MS_SLAVE" ){
        res = res | MS_SLAVE;
      }else if ( v[i] == "MS_UNBINDABLE" ){
        res = res | MS_UNBINDABLE;
      }else if ( v[i] == "MS_MOVE" ){
        res = res | MS_MOVE;
      }else if ( v[i] == "MS_DIRSYNC" ){
        res = res | MS_DIRSYNC;
      //}else if ( v[i] == "MS_LAZYTIME" ){
      //  res = res | MS_LAZYTIME;
      }else if ( v[i] == "MS_MANDLOCK" ){
        res = res | MS_MANDLOCK;
      }else if ( v[i] == "MS_NOATIME" ){
        res = res | MS_NOATIME;
      }else if ( v[i] == "MS_NODEV" ){
        res = res | MS_NODEV;
      }else if ( v[i] == "MS_NODIRATIME" ){
        res = res | MS_NODIRATIME;
      }else if ( v[i] == "MS_NOEXEC" ){
        res = res | MS_NOEXEC;
      }else if ( v[i] == "MS_NOSUID" ){
        res = res | MS_NOSUID;
      }else if ( v[i] == "MS_RDONLY" ){
        res = res | MS_RDONLY;
      }else if ( v[i] == "MS_REC" ){
        res = res | MS_REC;
      }else if ( v[i] == "MS_RELATIME" ){
        res = res | MS_RELATIME;
      }else if ( v[i] == "MS_SILENT" ){
        res = res | MS_SILENT;
      }else if ( v[i] == "MS_STRICTATIME" ){
        res = res | MS_STRICTATIME;
      }else if ( v[i] == "MS_SYNCHRONOUS" ){
        res = res | MS_SYNCHRONOUS;
      }else {
        res = 0;
        return 0;
      }
    }
    
    return 1;
      
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // RSYNC ERROR FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // this is not used, but keep it for future 
  std::string rsync_errorCodetoString(int errorCode) 
  {
    switch(errorCode){
      case 0:
        return "Success";
      case 1:
        return "Syntax or usage error";
      case 2:
         return "Protocol incompatibility";
      case 3:
        return "Errors selecting input/output files, dirs";
      case 4:
        return "Requested  action not supported: an attempt was made to manipulate 64-bit files on a platform that cannot support them; or an option was specified that is supported by the client and not by the server.";
      case 5:
        return "Error starting client-server protocol";
      case 6:
        return "Daemon unable to append to log-file";
      case 10:
        return "Error in socket I/O";
      case 11:
        return "Error in file I/O";
      case 12:
        return "Error in rsync protocol data stream";
      case 13:
        return "Errors with program diagnostics";
      case 14:
        return "Error in IPC code";
      case 20:
        return "Received SIGUSR1 or SIGINT";
      case 21:
         return "Some error returned by waitpid()";
      case 22:
        return "Error allocating core memory buffers";
      case 231:
        return "Partial transfer due to error";
      case 24:
        return "Partial transfer due to vanished source files";
      case 25:
         return "The --max-delete limit stopped deletions";
      case 30:
        return "Timeout in data send/receive";
      case 35:
        return "Timeout waiting for daemon connection";
      default:
        return "unrecognized error code";
        
    }
  }

      
}
  





