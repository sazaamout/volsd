
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
  // LOGGER FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  std::string get_instance_id(){
    std::string output;
    exec(output, "curl -s http://169.254.169.254/latest/meta-data/instance-id");
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
    std::cout << "Master loglevel:\t"     << conf.MasterLoglevel             << std::endl;
    std::cout << "Manager loglevel:\t"    << conf.ManagerLoglevel            << std::endl;
    std::cout << "DispatcherLoglevel:\t" << conf.DispatcherLoglevel         << std::endl;
    std::cout << "Client loglevel:\t"     << conf.ClientLoglevel             << std::endl;
    std::cout << "=========================================================\n\n";
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // LOAD_CONFIGURATION FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int load_configuration(Configuration &conf, std::string conf_file){
    //std::cout << "loading start\n";
    std::fstream myFile;
    myFile.open(conf_file.c_str());
   
    if (!myFile.is_open()) 
      return 0;
    //std::cout << "file was opened\n";
    
    std::string line;
    
    while (std::getline(myFile, line)){
      //std::cout << "reading line: " << line << "\n";
      
      if ( (line[0] == '#') || (line[1] == '#') || (line[0] == '\n')){
        continue;
      }
      
      if (line.find("MaxIdleDisk") != std::string::npos)
        conf.MaxIdleDisk = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("TargetFilesystem ") != std::string::npos)
        conf.TargetFilesystem = line.substr(line.find(" ")+1);
      else if (line.find("TargetFilesystemMountPoint") != std::string::npos)
        conf.TargetFilesystemMountPoint = line.substr(line.find(" ")+1);
      else if (line.find("TargetFilesystemDevice") != std::string::npos)
        conf.TargetFilesystemDevice = line.substr(line.find(" ")+1);
      else if (line.find("TempMountPoint") != std::string::npos)
        conf.TempMountPoint = line.substr(line.find(" ")+1);
      else if (line.find("SnapshotFrequency") != std::string::npos)
        conf.SnapshotFrequency = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("Hostname") != std::string::npos)
        conf.Hostname = line.substr(line.find(" ")+1);
      else if (line.find("SnapshotFile") != std::string::npos)
        conf.SnapshotFile = line.substr(line.find(" ")+1);
      else if (line.find("SnapshotMaxNumber") != std::string::npos)
        conf.SnapshotMaxNumber = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("VolumeFilePath") != std::string::npos)
        conf.VolumeFilePath = line.substr(line.find(" ")+1);
      else if (line.find("ManagerLogFile") != std::string::npos)
        conf.ManagerLogFile = line.substr(line.find(" ")+1);
      else if (line.find("DispatcherLogPrefix") != std::string::npos)
        conf.DispatcherLogPrefix = line.substr(line.find(" ")+1);
      else if (line.find("ClientLogFile") != std::string::npos)
        conf.ClientLogFile = line.substr(line.find(" ")+1);
      else if (line.find("SyncerLogFile") != std::string::npos)
        conf.SyncerLogFile = line.substr(line.find(" ")+1);
      else if (line.find("Syncerloglevel") != std::string::npos)
        conf.Syncerloglevel = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("Masterloglevel") != std::string::npos)
        conf.MasterLoglevel = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("Managerloglevel") != std::string::npos)
        conf.ManagerLoglevel = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("DispatcherLoglevel") != std::string::npos)
        conf.DispatcherLoglevel = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("Clientloglevel") != std::string::npos)
        conf.ClientLoglevel = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("SyncFrequency") != std::string::npos)
        conf.SyncFrequency = utility::to_int(line.substr(line.find(" ")+1));
      else if (line.find("SyncRequestsFile") != std::string::npos)
        conf.SyncRequestsFile = line.substr(line.find(" ")+1);
                
      else if (line.find("SyncDatesFile") != std::string::npos)
        conf.SyncDatesFile = line.substr(line.find(" ")+1);
                
      else if (line.find("SyncerServicePort") != std::string::npos)
        conf.SyncerServicePort = utility::to_int(line.substr(line.find(" ")+1));
      
      else if (line.find("SynOutputEmailTo") != std::string::npos)
        conf.SynOutputEmailTo = line.substr(line.find(" ")+1);
      else if (line.find("SynErrorEmailTo") != std::string::npos)
        conf.SynErrorEmailTo = line.substr(line.find(" ")+1);  
      
      else if (line.find("EmailSynOutput") != std::string::npos)
        conf.EmailSynOutput = line.substr(line.find(" ")+1);
      else if (line.find("EmailSynError") != std::string::npos)
        conf.EmailSynError = line.substr(line.find(" ")+1);  

      else if (line.find("LocalRsyncCommand") != std::string::npos)
        conf.LocalRsyncCommand = line.substr(line.find(" ")+1);
      else if (line.find("RemoteRsyncCommand") != std::string::npos)
        conf.RemoteRsyncCommand = line.substr(line.find(" ")+1);  
                        
                        else if (line.find("EmailPushOutput") != std::string::npos)
                                 conf.EmailPushOutput = line.substr(line.find(" ")+1);
                        else if (line.find("EmailPushError") != std::string::npos)
                                 conf.EmailPushError = line.substr(line.find(" ")+1);
                        else if (line.find("EmailPushEmail") != std::string::npos)
                                 conf.EmailPushEmail = line.substr(line.find(" ")+1);

      else
        continue;
        
      line="";
    }
    myFile.close(); 
    
    //std::cout << "loading is done\n";
    //print_configuration(conf);
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
  void print_elements(std::vector<std::string>& v){
    std::cout << " =================================================\n ";
    for(std::vector<std::string>::iterator it = v.begin(); it != v.end(); ++it) {
      std::cout << *it << " ";
    }
    std::cout << " =================================================\n";
  }


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DATETIME FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // not used
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
    std::time_t result = std::time(nullptr);
    return to_string(result);
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DATETIME_DIFF FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // not used
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
  // used tocheck if files/directories exist
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

    if ( prefix.back() != '/' ) {
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
  bool umountfs( std::string &output, std::string mountPoint ){
    const char* trgt = mountPoint.c_str();
    int result = umount(trgt);

    if (result == 0) {
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
  bool is_mounted( const std::string dir){
	  
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
  // File Create FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool file_create(std::string path){
    if (path == "")
      return false;
      
    
    std::size_t pos = path.rfind('/');
    std::string file = path.substr(path.rfind('/')+1);
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
  
  
  bool folder_remove( const std::string dirname){
    if ( !rmdir(dirname.c_str())) {
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
  // RSYNC ERROR FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // not sure if this still used?
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
  





