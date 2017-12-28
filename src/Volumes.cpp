#include "Volumes.h"


using namespace utility;

// =================================================================================================
// Class Constructor
// =================================================================================================
Volumes::Volumes ( std::string rmd, std::string volumeFile, int idleVolumes ){
  
  _rootMountDirectory = rmd;
  _volumeFile         = volumeFile;
  
  // Load Volumes information from file  
  load();
}

// =================================================================================================
// Class Constructor 
// =================================================================================================
Volumes::Volumes( std::string rmd, std::string volumeFile ){
  // NOT USED, REMOVE
  _rootMountDirectory = rmd;
  _volumeFile = volumeFile;
}

// =================================================================================================
// Class Constructor
// =================================================================================================
Volumes::Volumes() {
  
}

// =================================================================================================
// Class desstructor
// =================================================================================================
Volumes::~Volumes() {
  delete logger;
}


// =================================================================================================
// Function: Set Logger att
// =================================================================================================
void Volumes::set_logger_att( bool toScreen, std::string logFile, int loglevel ) {
  logger = new Logger(toScreen, logFile, loglevel);
}


// =================================================================================================
// Function: Create
// =================================================================================================
int Volumes::create( std::string &t_volumeId, const std::string t_latestSnapshot, 
                     const int t_transactionId ){
  // create a volume from latest snapshot
  std::string newVolume;
  
  int res = utility::exec (
            newVolume, 
            "aws ec2 create-volume --region us-east-1 --availability-zone us-east-1a --snapshot-id " 
            + t_latestSnapshot + " --volume-type gp2 --query VolumeId --output text"
            );
  
  if (!res){
    logger->log("info", "", "volsd", t_transactionId, 
                "failed to create volume, ExitCode(" + utility::to_string(res) + "). retry" + 
                t_latestSnapshot + "]", "create");
  }
  utility::clean_string(newVolume);

  // wait untill is created
  bool created = false;
  std::string status;
  sleep(2);

  utility::exec(status, "aws ec2 describe-volumes --volume-ids " + newVolume + 
                " --region us-east-1  --query Volumes[*].State --output text");

  while (!created){
    if (status.find("available") != std::string::npos){
      created = true;

    }else {
      utility::exec(status, "aws ec2 describe-volumes --volume-ids " + newVolume + 
                    " --region us-east-1  --query Volumes[*].State --output text");
    }

  }

  t_volumeId = newVolume;

  return 1;
}


// =================================================================================================
// Function: Attch
// =================================================================================================
int Volumes::attach( const std::string t_volumeId, const std::string t_device, 
                     const std::string t_instanceId, const int t_transcation ) {
  
  std::string output;
  
  // 1. attach
  int res = utility::exec( output, 
                           "/usr/bin/aws ec2 attach-volume --volume-id " + t_volumeId +
                           " --instance-id " + t_instanceId + " --device /dev/" + 
                           t_device + " --region us-east-1");
  
  if (!res) {
    logger->log("debug", "", "volsd", t_transcation, "attaching volume[" + t_volumeId + 
                "] failed", "attach");
    logger->log("debug", "", "volsd", t_transcation, "AWS ERROR: " + output, "attach");
    return 0;
  }
  
              
  // problem: the command will return true when attaching the diskm but not yet fully attached. This
  // function will return true even thought status is attacjing. This cause the mount to fail.
  // use the command to get more accurate results
  

  
  bool ready = false;
  int  retry = 0;
  // wait untill ready
  while (!ready){
    
    res = utility::exec( output, 
                       "/usr/bin/aws ec2 describe-volumes  --volume-ids " + t_volumeId + 
                       " --query Volumes[*].Attachments[].State --output text --region us-east-1" );
    
    utility::clean_string(output);
    
    if (!res){
      logger->log("error", "", "volsd", t_transcation, "cannot describe volume", "attach");
      logger->log("error", "", "volsd", t_transcation, "AWSERROR:[" + output + "]", "attach");
      retry++;
    }
    
    if (retry == 15){
      logger->log("error", "", "volsd", t_transcation, "volume:[" + t_volumeId + 
                  "] could not attach. ExitCode(" + utility::to_string(res) + ")", 
                  "attach");
      return 0;
    }

    if (output.find("attaching") != std::string::npos){
      logger->log("debug", "", "volsd", t_transcation, "volume:[" + t_volumeId + 
                  "] is 'attaching'. retry ...", "attach");
    } 
    
    if (output.find("attached") != std::string::npos){
      logger->log("debug", "", "volsd", t_transcation, "volume[" + t_volumeId + 
                  "] was attached successfuly", "attach");
      ready = true;
      return 1;
    }
    
    output = "";
    retry++;
    sleep(2);
  }
  
  
}


// =================================================================================================
// Function: Mount
// =================================================================================================
int Volumes::mount( const std::string t_volumeId, const std::string t_mountPoint, 
                    const std::string t_device, const int t_transactionId ) {
  
  // 1. check if the dir exist
  if (!utility::is_exist(t_mountPoint)){
    // create the directory
    if (!utility::folder_create(t_mountPoint)) {
      logger->log("debug", "", "volsd", t_transactionId, "cannot not create mountpoint:[" + 
                  t_mountPoint + "]", "mount");
      return 0;
    }
  } 
  
  // 2. check if it is used by another filesystem
  if (utility::is_mounted(t_mountPoint)) {
    logger->log("debug", "", "volsd", t_transactionId, 
                "mountpoint cannot be used. It already been used by another filesystem", "mount");
    return 0;
  } 
  
  // 3. since it is exist, check if its already have data
  if (!utility::folder_is_empty(t_mountPoint)) {
    logger->log("debug", "", "volsd", t_transactionId, 
                "mountpoint cannot be used. it has some data", "mount");
    return 0;
  }
  
  // all good, the start to mount the filesystem
  logger->log("debug", "", "volsd", t_transactionId, 
              "mounting volume:[" + t_volumeId + "] on device:[" + t_device + "] on mountPoint:[" + 
              t_mountPoint + "]", "mount");
              
  std::string output;
  if (!utility::mountfs( output, t_mountPoint, "/dev/"+t_device)){
	  logger->log("error", "", "volsd", t_transactionId, "failed to mount volume. " + output, "mount");
    return 0;
  }
  
  return 1;
}

// =================================================================================================
// Function: Mount
// =================================================================================================
int Volumes::mount( const std::string t_volumeId,  const std::string t_mountPoint, 
                    const std::string t_device,    const std::string t_fsType, 
                    const std::string t_mntflags, const int t_transactionId ) {
  
  // 1. check if the dir exist
  if (!utility::is_exist(t_mountPoint)){
    // create the directory
    if (!utility::folder_create(t_mountPoint)) {
      logger->log("debug", "", "volsd", t_transactionId, "cannot not create mountpoint:[" + 
                  t_mountPoint + "]", "mount");
      return 0;
    }
  } 
  
  // 2. check if it is used by another filesystem
  if (utility::is_mounted(t_mountPoint)) {
    logger->log("debug", "", "volsd", t_transactionId, 
                "mountpoint cannot be used. It already been used by another filesystem", "mount");
    return 0;
  } 
  
  // 3. since it is exist, check if its already have data
  if (!utility::folder_is_empty(t_mountPoint)) {
    logger->log("debug", "", "volsd", t_transactionId, 
                "mountpoint cannot be used. it has some data", "mount");
    return 0;
  }
  
  // all good, the start to mount the filesystem
  logger->log("debug", "", "volsd", t_transactionId, 
              "mounting volume:[" + t_volumeId + "] on device:[" + t_device + "] on mountPoint:[" + 
              t_mountPoint + "]", "mount");
              
  std::string output;
  // use the new mount
  //std::string d = "/dev/"+t_device;
  if ( !utility::mountfs( output, 
                          ("/dev/"+t_device).c_str(), 
                          t_mountPoint.c_str(), 
                          t_fsType.c_str(), 
                          t_mntflags.c_str(), 
                          "" 
                        ) ){
	  logger->log("error", "", "volsd", t_transactionId, "failed to mount volume. " + output, "mount");
    return 0;
  }
  
  return 1;
}




// =================================================================================================
// Function: Detach
// =================================================================================================
int Volumes::detach( const std::string t_idleVolId, const int t_transactionId ){
  
  std::string output;
  logger->log("debug", "", "volsd", t_transactionId, "detaching EBS Volume:[" + 
              t_idleVolId + "] from instance", "");

  int res = utility::exec(output, "/usr/bin/aws ec2 detach-volume --volume-id " + 
            t_idleVolId + " --region us-east-1");
  if (!res)   {
    logger->log("debug", "", "volsd", t_transactionId, "detaching volume[" + t_idleVolId + 
                "] failed. Exit(" + utility::to_string(res) + ")", "detach");
    return 0;
  }

  int retry = 0;
  bool ready = false;

  // wait untill ready
  while (!ready){
    int res = utility::exec(output, "/usr/bin/aws ec2 describe-volumes --volume-id " + 
                            t_idleVolId + 
                            " --query Volumes[*].State --output text --region us-east-1");
    utility::clean_string(output);

    if (retry == 15){
      logger->log("error", "", "volsd", t_transactionId, "volume:[" + t_idleVolId + 
                  "] could not detached. ExitCode(" + utility::to_string(res) + ")", 
                  "detach");
      return 0;
    }

    if (!res){
      logger->log("error", "", "volsd", t_transactionId, "cannot describe volume", 
                  "ebsvolume_detach");
      logger->log("error", "", "volsd", t_transactionId, "AWSERROR:[" + output + 
                  "]", "detach");
    }
    
    if (output.find("in-use") != std::string::npos){
      logger->log("debug", "", "volsd", t_transactionId, "volume:[" + t_idleVolId + 
                  "] is 'in-use'. retry ...", "detach");
    } if (output.find("available") != std::string::npos){
      logger->log("debug", "", "volsd", t_transactionId, "volume[" + t_idleVolId + 
                  "] was detached Successfuly", "detach");
      ready = true;
      return 1;
    }
    output = "";
    retry++;
    sleep(2);
  }

  return 1;
}
 

// =================================================================================================
// Function: umount
// =================================================================================================
bool Volumes::umount( const std::string t_volumeId, const std::string t_mountPoint, const int t_transactionId ){
  std::string output;
  logger->log("debug", "", "volsd", t_transactionId, "umount volume:[" + t_volumeId + 
              "]", "umount");
  
  if (!utility::umountfs( output, t_mountPoint)) {
    logger->log("debug", "", "volsd", t_transactionId, "failed to umount volume:[" + t_volumeId + 
                "]", "umount");
  }
  
  logger->log("debug", "", "volsd", t_transactionId, "umount volume:[" + t_volumeId + 
              "] was successful", "umount");
  return true;
}


// =================================================================================================
// Function: Del
// =================================================================================================
int Volumes::del(const std::string t_volumeId, int t_transactionId){

  std::string output;

  int retry = 0;
  bool deleted = false;

  while (!deleted) {
    int res = utility::exec( output, "aws ec2 delete-volume --volume-id " + t_volumeId + 
                             " --region us-east-1");
    utility::clean_string(output);
    if (retry == 15) {
      logger->log("error", "", "volsd", t_transactionId, "volume failed to delete, Exit(" + 
                  utility::to_string(res) + ").", "del");
      return 0;
    }
    if (!res)   {
      logger->log("debug", "", "volsd", t_transactionId, "volume failed to delete, Exit(" + 
                   utility::to_string(res) + "). retry...", "del");
      logger->log("error", "", "volsd", t_transactionId, "AWSERROR:[" + output + "]", "del");
      retry++;
    } else {
      deleted = true;
      return 1;
    }
    output = "";
    sleep(2);
  }

  return 1;
}


// =================================================================================================
// Function: Get Idle Volume
// =================================================================================================
//bool Volumes::get_idle_volume(int &volumeIndex , int transactionId, Logger& logger) {
int Volumes::get_idle_volume( int &idleVolumeIndex, const int transactionId ) {
  
  // return an idle disk
  for (int i=0; i<m_volumes.size(); i++) {
    if ( m_volumes[i].status == "idle" ){
      logger->log("debug", "", "volsd", transactionId, "volume found:","get_idle_volume");
      logger->log("debug", "", "volsd", transactionId, "id:[" + m_volumes[i].id   + "] status:["     +
                  m_volumes[i].status     + "] attachedTo:[" +
                  m_volumes[i].attachedTo + "] mountpoint:[" +
                  m_volumes[i].mountPoint + "] device:["     +
                  m_volumes[i].device     + "]",
                  "get_idle_volume"
                 );
      idleVolumeIndex = i;
      return 1;
    }
  }
  return 0;  
}


// =================================================================================================
// Function: get Idle Number
// =================================================================================================
int Volumes::get_idle_number() {
  
  int c =0;
  for (int i=0; i<m_volumes.size(); i++) {
    if ( m_volumes[i].status == "idle" ){
      c++;
    }
  }
  return c;  
}


bool Volumes::write_to_file( int t_transactionId ){
  // write back to file
  logger->log("debug", "", "volsd", t_transactionId, 
              "writing changes to volumes file", 
              "write_to_file");
  
  std::ofstream myFileOut;
  myFileOut.open(_volumeFile.c_str(), std::fstream::out | std::fstream::trunc);

  if (!myFileOut.is_open()) {
    logger->log("error", "", "volsd", t_transactionId, 
                "could not open volumes file", 
                "write_to_file");
    return false;
  }

  
  
  for(std::vector<utility::Volume>::iterator it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    myFileOut << it->id << " " << it->status << " " << it->attachedTo 
              << " " << it->mountPoint << " " << it->device << "\n";
  }
  
  myFileOut.close();
  myFileOut.clear();
  
  return true;
}


// =================================================================================================
// Function: Remove
// =================================================================================================
int Volumes::remove ( const std::string t_volumeId, const int t_transactionId ){
  
  // delete volume
  for(std::vector<utility::Volume>::iterator it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    if ( it->id == t_volumeId ){
      m_volumes.erase(it);
      break;
     }
  }
    
  // write changes to disk file
  m.lock();
  int res = write_to_file( t_transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "volsd", t_transactionId, "failed to open file ", "remove");
    return 0;
  }
  
  return 1;
}


// =================================================================================================
// Function: Add
// =================================================================================================
int Volumes::add ( const utility::Volume t_volumes, const int t_transactionId ){
	
  // add to m_vectors
  m_volumes.push_back(t_volumes);
  
  // write changes to disk
  m.lock();
  int res = write_to_file( t_transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "volsd", t_transactionId, "failed to open file", "add");
    return 0;
  }
  
  return 1;
}


// =================================================================================================
// Function: Update
// =================================================================================================
int Volumes::update ( const std::string t_volumeId, const std::string t_key, 
                      const std::string t_value,    const int t_transactionId, 
                      std::string t_ip, std::string t_remoteMountPoint, std::string t_device ){

  // 1. find the volume record
  bool found = false;
  int i;
  for ( i=0; i<m_volumes.size() ; i++ ){
    if ( m_volumes[i].id == t_volumeId ){
      found = true;
      break;
    }
  }

  if (!found){
    logger->log("debug", "", "volsd", t_transactionId, 
                "could not find volume " + t_volumeId , "update");
    return 0;
  }
  
  // 2. update the value
  if ( t_key == "status") {
    m_volumes[i].status = t_value;
    
    if ( t_ip != "" ) {
      m_volumes[i].attachedTo = t_ip;
    }
    
    if ( t_remoteMountPoint != "" ) {
      m_volumes[i].mountPoint = t_remoteMountPoint;
    }
    if ( t_device != "" ) {
      m_volumes[i].device = t_device;
    }
  } 
  
  // 3. write changes to disk
  m.lock();
  int res = write_to_file( t_transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "volsd", t_transactionId, "failed to open file", "update");
    return 0;
  }
  
  return 1;
}


// =================================================================================================
// Function: Is Exist
// =================================================================================================
int Volumes::is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger){

  std::string output;
  // test1: check and see if mountPoint exists

  logger.log("debug", "", "volsd", transactionId, 
             "checking If MountPoint:[" + mountPoint + "] exists");

  int res = utility::exec(output, "ls " + mountPoint); // return 0 for exist
  // new code start here ------------
  // does not exist
  if (!res){
    logger.log("debug", "", "volsd", transactionId, "mountPoint:[" + mountPoint + 
               "] does not exist, creating ...");

    std::string cmd = "mkdir " + mountPoint;
    system(cmd.c_str());
    return 0;
  }
  // Exist + empty = idle, ok to be used
  if ( (res) && (output.empty()) )  {
    logger.log("debug", "", "volsd", transactionId, "mountPoint exists and good to be used.");
    return 0;
  }
  // exist + not empty, means it is used by a filesystem
  if ( (res) && (!output.empty()) )  {
    logger.log("debug", "", "volsd", transactionId, 
               "mountPoint exist but it is used by another filesystem. Exit(" + 
              utility::to_string(res) + ")");
    return 1;
  }

  return 1;
}


// =================================================================================================
// Function: Is Used
// =================================================================================================
int Volumes::is_used(std::string mountPoint, bool d, int transactionId, Logger& logger){
  std::string output;

  logger.log("debug", "", "volsd", transactionId, 
             "checking If mountPoint:[" + mountPoint + "] is used by another filesystem");
  int res = utility::exec(output, "grep -qs " + mountPoint + " /proc/mounts");
  // check if mountPoint is already used
  if (res) {
    logger.log("error", "", "volsd", transactionId, 
               "mountPoint is used by another filesystem. Exit(" + utility::to_string(res) + ")");
    return 1;
  }

  logger.log("debug", "", "volsd", transactionId, "mountPoint is available.");
  return 0;
}


// =================================================================================================
// Function: Is Attched
// =================================================================================================
int Volumes::is_attached( std::string t_instanceId, std::string t_volumeId, std::string t_device, 
                          int t_transactionId, Logger& logger){
  logger.log("debug", "", "volsd", transactionId, "checking if volume is attached");
  
  std::string output;
  
  int res = utility::exec(output, "aws ec2 describe-volumes --volume-id " + t_volumeId + " --query \"Volumes[].Attachments[].[State, InstanceId, Device]\" --output text --region us-east-1 --output text" );

  if (!res) {
    logger.log("info", "", "volsd", t_transactionId, "failed to get volume status", "is_attach");
    logger.log("debug", "", "volsd",t_transactionId, "AWS ERROR: " + output, "is_attached");
  }

  std::stringstream ss(output);
  std::string device, instanceId, state;
  ss >> state >> instanceId >> device;
 
  // first, check the status
  if (state.find("attached") != std::string::npos) {
    if (instanceId.find(t_instanceId) == std::string::npos) {
      logger.log("debug", "", "volsd", t_transactionId, "Volume is already attached Locally"); 
      return 1;
    } else {
      logger.log("error", "", "volsd", t_transactionId, "Volume is attached in another instance, please use different volume", "is_attach");
      return 0;
    }
  }else {
      std::cout << " > volume status is available\n";
      device = t_device.substr(t_device.find("/", 1)+1);
      if (! attach(t_volumeId, device, t_instanceId, t_transactionId) ) {
        return 0;
        
      }

  }
  
  
  return 1;
}


// =================================================================================================
// Function: RELEASE_VOLUME
// =================================================================================================
int Volumes::release_volume( std::string& v, std::string t_instanceId, std::string t_mountPoint, 
                             int t_transactionId ) {

  std::string output;

  // 1. given a mountPoint, get the device name
  logger->log("info", "", "volsd", t_transactionId, "get the device name for the mounted volume");
  int res = utility::exec(output,  "df " + t_mountPoint + " | grep " +  t_mountPoint + 
                          " | grep -oP ^/dev/[a-z0-9/]+");
  if (!res) {
    logger->log("error", "", "volsd", t_transactionId, "mountPoint does not exist");
    return 0;
  }
  std::string device = output;
  output.clear();
  
  utility::clean_string(device);
  logger->log("info", "", "volsd", t_transactionId, "device was found: " + device);


  // 2. given the device, get the volume id.
  logger->log("info", "", "volsd", t_transactionId, "acquiring the volume's Amazon Id");
  std::string cmd = "aws ec2 describe-instances --instance-id " + 
                    t_instanceId + 
                    " --query \"Reservations[*].Instances[*].BlockDeviceMappings[\?DeviceName==\'" + 
                    device + "\'].Ebs.VolumeId\" --output text --region us-east-1";
                    
  logger->log("debug", "", "volsd", t_transactionId, "Command: " + cmd);
  res = utility::exec(output,  cmd);
  if (!res) {
    logger->log("error", "", "volsd", t_transactionId, "command returned " + res);
    return 0;
  }
  utility::clean_string(output);
  std::string vol = output;
  logger->log("info", "", "volsd", t_transactionId, "volume id was acquired: " + vol );
  
  // now that we have the volume id, umount mountPoint and detach volume
  logger->log("info", "", "volsd", t_transactionId, "umounting volume");
  if (!umount(vol, t_mountPoint, t_transactionId) ) {
    logger->log("error", "", "volsd", t_transactionId, "failed to umount volume");
    return 0;
  }
  logger->log("info", "", "volsd", t_transactionId, "volume was unmounted");

  // now detach file system
  logger->log("info", "", "volsd", t_transactionId, "detaching volume");
  if ( !detach( vol, transactionId ) ) {
    logger->log("info", "", "volsd", t_transactionId, "failed to detach volume");
    //return 0;
  } else {
    logger->log("info", "", "volsd", t_transactionId, "detaching was successful");
    // now delete the volume
    logger->log("info", "", "volsd", t_transactionId, "removing volume from Amazon space");
    if (!del( vol, t_transactionId )){
      logger->log("error", "", "volsd", t_transactionId, "volume failed to be removed");
      return 0;
    }
    logger->log("info", "", "volsd", t_transactionId, "volume was deleted from Amazon space");
  }
      
  v = vol;
  return 1;
}


// =================================================================================================
// Function: REMOVE_MOUNTPOINT
// =================================================================================================
int Volumes::remove_mountpoint(std::string mp, int transactionId, Logger& logger) {
  std::string output;
  logger.log("debug", "", "volsd", transactionId, "removig mountpoint:[" + mp + "]");

  int res = utility::exec(output, "rmdir " + mp );

  if (!res){
    logger.log("error", "", "volsd", transactionId, "failed to removig mountpoint:[" + mp + 
                "]. Exit(" + utility::to_string(res) + ")");
    return 0;
  }

  logger.log("debug", "", "volsd", transactionId, "mountpoint:[" + mp + 
            "] was removed Successfully");

  return 1;
}



// =================================================================================================
// Function: GET_DEVICE
// =================================================================================================
std::string Volumes::get_device() {

  // get device already been used locally.
  std::string str;
  utility::exec(str, "lsblk | grep -oP xvd[a-z]*");
  utility::clean_string(str);

  std::string word;
  char c1, c2;
  bool flag = false; // assumes is taken
  // loop throught b-z. 'a' is reserved for root EBS
  for (int i=98; i<=122; i++){
    c1 = i;
    std::stringstream ss(str);
    //chcek if this char is taken
    flag = false;

    for (int ii = 0; ss >> word; ii++){
      // get last char
      c2 = word[word.length()-1];
      if (c1 == c2)
      flag = true;
    }

    if (!flag){
      break;
    }
  }

  word[word.length()-1] = c1;

  return word;
}


// =================================================================================================
// Function: GET_DEVICE
// =================================================================================================
std::string Volumes::get_device(std::vector<std::string>& list) {

  //std::cout << "\n Volumes:: GET_DEVICE\n";

  // get device already been used locally.
  std::string str;
  utility::exec(str, "lsblk | grep -oP xvd[a-z]*");
  //str.append(" ");


  for(std::vector<std::string>::iterator it = list.begin(); it != list.end(); ++it) {
    str.append(*it + " ");
  }

  utility::clean_string(str);

  std::string word;
  char c1, c2;
  bool flag = false; // assumes is taken
  // loop throught b-z. 'a' is reserved for root EBS
  for (int i=98; i<=122; i++){
    c1 = i;
    std::stringstream ss(str);
    //chcek if this char is taken
    flag = false;

    for (int ii = 0; ss >> word; ii++){
      // get last char
      c2 = word[word.length()-1];
      if (c1 == c2)
      flag = true;
    }

    if (!flag){
      break;
    }
  }

  word[word.length()-1] = c1;

  //std::cout << "  >> List of Devices:[" << str << "]\n";
  //std::cout << "  >> Seleclted device:[" << word << "]\n";
  return word;
}


// =================================================================================================
// Function: Volume Exist
// =================================================================================================
int Volumes::volume_exist( const std::string volId ){

  std::vector<utility::Volume>::iterator it;
  for(it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    if ( it->id == volId ) {
	  return 1;
	}
  }
  return 0;
}


// =================================================================================================
// Function: Release
// =================================================================================================
int Volumes::release (std::string &volumeId, int t_transactionId) {
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 1. Get Idle Volume  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  int idleVolIndex;
  utility::Volume v;
  
  logger->log("info", "", "volsd", t_transactionId, "looking for idle volume");
  if ( !get_idle_volume( idleVolIndex, t_transactionId ) ) {
    logger->log("error", "", "volsd", t_transactionId, "no more idle volume available");
    return -1;
  } else {
    // store the volume information,
    // the mvolumes vector get changes alot, we cannot use index to retried information since index 
    // might change where it is pointing to.
    v.id         = m_volumes[idleVolIndex].id;
    v.status     = m_volumes[idleVolIndex].status;
    v.attachedTo = m_volumes[idleVolIndex].attachedTo;
    v.mountPoint = m_volumes[idleVolIndex].mountPoint;
    v.device     = m_volumes[idleVolIndex].device;
  
    logger->log("info", "", "volsd", t_transactionId, 
                "idle volume found:[" + v.id + "]");
    
    if (!update(v.id, "status", "inprogress", t_transactionId)) {
      logger->log("info", "", "volsd", t_transactionId, "failed to update volumes status");
    }
    logger->log("info", "", "volsd", t_transactionId, "volume's status was changed to 'inprogress'");
  }

 
    
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 2. unmount disk
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "volsd", t_transactionId, 
              "umounting volume:[" + v.id + "]");
  
  if (!umount( v.id, v.mountPoint, t_transactionId )) {
    logger->log("error", "", "volsd", t_transactionId, "failed to umount volume");
    logger->log("info", "", "volsd", t_transactionId, "change the volume status to failedToUnmount");

    if (!update(v.id, "status", "failedToUnmount", t_transactionId)) {
      logger->log("info", "", "volsd", t_transactionId, "failed to update volumes status");
    }
    
    return -2;
  }
  logger->log("info", "", "volsd", t_transactionId, "volume umounted");
  
      
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 3. Detach the volume
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "volsd", t_transactionId, "detaching volume");
  if (!detach(v.id, t_transactionId)) {
    logger->log("error", "", "volsd", t_transactionId, "failed to detach volume");
        
    if (!update(v.id, "status", "failedToDetach", t_transactionId)) {
      logger->log("info", "", "volsd", t_transactionId, "failed to update volumes status");
    }
    
    return -3;
  }
  logger->log("info", "", "volsd", t_transactionId, "volume was detached");

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 4. Remove MountPoint
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "volsd", t_transactionId, 
              "remove mountpoint:[" + v.mountPoint +"]");
  
  if (!utility::folder_remove(v.mountPoint)){
    logger->log("error", "", "volsd", t_transactionId, "could not remove mountpoint");
  } else {
    logger->log("info", "", "volsd", t_transactionId, "mount point was removed");
  }

  sleep(5);
  volumeId = v.id;
  
  return 1;
}


// =================================================================================================
// Function: Load
// =================================================================================================
bool Volumes::load(){
  // open the file, and load all volume info in the array
  std::ifstream myFile;
  std::string line;
  myFile.open(_volumeFile.c_str());
  
  // check if file is empty
  if ( myFile.peek() == std::ifstream::traits_type::eof() ){
    return 0;
  }
      
  //load data
  utility::Volume v;
  while (std::getline(myFile, line)) {
      
      std::size_t vol_pos = line.find(' ', 0);
      v.id = line.substr(0, vol_pos);
      
      std::size_t status_pos = line.find(' ', vol_pos+1);
      v.status = line.substr(vol_pos+1, status_pos - vol_pos-1);
      
      std::size_t mountServer_pos = line.find(' ', status_pos+1);
      v.attachedTo = line.substr(status_pos+1, mountServer_pos-status_pos-1);
      
      std::size_t mountPoint_pos = line.find(' ', mountServer_pos+1);
      v.mountPoint = line.substr(mountServer_pos+1, mountPoint_pos-mountServer_pos-1);
      
      std::size_t device_pos = line.find(' ', mountPoint_pos+1);
      v.device = line.substr(mountPoint_pos+1, device_pos-mountPoint_pos-1);
      
      m_volumes.push_back(v);
  }
  
  myFile.close();
  myFile.clear();
  return 0;
	                                                                      
}


// =================================================================================================
// Function: Print
// =================================================================================================
void Volumes::printxyz() {
  
  for(std::vector<utility::Volume>::iterator it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    std::cout << "VolId:["   << it->id 
              << "] statu:[" << it->status 
              << "] attac:[" << it->attachedTo 
              << "] mount:[" << it->mountPoint 
              << "] devic:[" << it->device 
              << "]\n";
  }
}


// =================================================================================================
// Function: Remount
// =================================================================================================
void Volumes::remount(){
  std::string output;
  
  if ( m_volumes.size() == 0 ){
    logger->log("info", "", "volsd", 0, "there is no spare volumes to be mounted");
    return;
  }
  
  // for each item in the vols, chck if mounted, if not, then remount
  for ( int i=0; i<m_volumes.size(); i++){
    if (m_volumes[i].status == "idle" ){
      if ( !utility::is_mounted(m_volumes[i].mountPoint) ){
        if ( !utility::mountfs(output, m_volumes[i].mountPoint, "/dev/"+m_volumes[i].device) ) {
          
          logger->log("info", "", "volsd", 0, "cannot mount filesystem. " + output);
          // detach
          if ( !detach(m_volumes[i].id, 0) ) {
              logger->log("info", "", "volsd", 0, 
                          "cannot detach volume. you have to do it manually");
          }
          logger->log("info", "", "volsd", 0, "volume was detach");
          
          //remove from list
          if ( !remove(m_volumes[i].id, 0) ){
            logger->log("error", "", "volsd", 0, "failed to write to file");
          }
        }
      }
    }
    
    
  }
  logger->log("info", "", "volsd", 0, "all spare volumes are mounted successfully");
}


// =================================================================================================
// Function: Aquire
// =================================================================================================
int Volumes::acquire( const std::string t_targetFileSystem, const std::string t_snapshotId, 
                      const std::string t_rootMountsFolder, const std::string t_instanceId, 
                      const int         t_transaction 
                    ){
  
  utility::Volume v;
  
    
  // -----------------------------------------------------------------------------------------------   
  // 1) Prepare disk information.
  //------------------------------------------------------------------------------------------------
  
  // get a device. 
  logger->log("info", "", "volsd", t_transaction, "allocating a device.");
  v.device = get_device( m_devicesOnHold );
  m_devicesOnHold.push_back(v.device);
  logger->log("info", "", "volsd", t_transaction, "device allocated:[" + v.device + "]");
  logger->log("info", "", "volsd", t_transaction, "Devices List:[" + 
              utility::to_string(m_devicesOnHold) + "]"
             );
  
  // get mount point
  if ( t_rootMountsFolder[t_rootMountsFolder.length() - 1] != '/' ){
    v.mountPoint = t_rootMountsFolder + '/' + utility::randomString();
  } else {
    v.mountPoint = t_rootMountsFolder + utility::randomString();
  }
  
  logger->log("info", "", "volsd", t_transaction, 
              "allocating mounting point:[" +  v.mountPoint + "]");
  

  
  // -----------------------------------------------------------------   
  // 2) Create Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volsd", t_transaction, 
              "create Volume from latest snapshot:[" + t_snapshotId +"]");
  std::string newVolId;
  if ( !create( newVolId, t_snapshotId, t_transaction ) ){
    logger->log("error", "", "volsd", t_transaction, "FALIED to create a volume");
    //remove_device
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }
  v.id = newVolId;
  logger->log("info", "", "volsd", t_transaction, "volume:[" + v.id + "] creation was successful");
  
  // -----------------------------------------------------------------   
  // 3) Attach Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volsd", t_transaction, "attaching volume " + v.id);
  if ( !attach(v.id, v.device, t_instanceId, t_transaction ) ){
    logger->log("error", "", "volsd", t_transaction, 
                "FAILED to attaching new disk to localhost. Removing  from AWS space");
    
    if (!del(v.id, t_transaction)){
      logger->log("error", "", "volsd", t_transaction, "FAILED to delete volume from AWS space");
    } else {
      logger->log("info", "", "volsd", t_transaction, "volume was removed from AWS space");
      //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
      logger->log("debug", "", "volsd", t_transaction, "removing device");
    }
    //remove_device
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }

  v.attachedTo = "localhost";
  logger->log("info", "", "volsd", t_transaction, "volume was attached successfully");
  sleep(5);

  // -----------------------------------------------------------------   
  // 4) mount Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volsd", t_transaction, "mounting volume into localhost");
  int retry=0;
  bool mounted = false;
  while (!mounted) {
    
    if ( !mount(v.id, v.mountPoint, v.device, t_transaction) ) {
      v.mountPoint = t_rootMountsFolder + utility::randomString();
      logger->log("info", "", "volsd", t_transaction, 
                  "FAILED to mount new volume. Retry with a new mountpoint " + v.mountPoint);
    } else {
      logger->log("info", "", "volsd", t_transaction, "new volume was mounted successfully");
      mounted = true;
    }
    
    if (retry == 5) {
      logger->log("error", "", "volsd", t_transaction, 
                  "FAILED to mount new disk to localhost. Detaching...");
      if (!detach(v.id, t_transaction))
        logger->log("error", "", "volsd", t_transaction, "FAILED to detach.");
  
        //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
        logger->log("info", "", "volsd", t_transaction, "removing device");
        utility::remove_element(m_devicesOnHold, v.device);
        return 0;
    }
    
    retry++;
  }
  
  v.status     = "idle";

  // -----------------------------------------------------------------------------------------------
  // 5. Sync Volume with the TargetFilesystemMountPoint
  // ----------------------------------------------------------------------------------------------- 
  logger->log("info", "", "volsd", t_transaction, "syncing new volume");
  std::string output;

  int res = Sync::synchronize( t_targetFileSystem, v.mountPoint, t_transaction, logger );
  if (!res){
    logger->log("error", "", "volsd", t_transaction, "syntax error in command");
  } else {
    logger->log("info", "", "volsd", t_transaction, "sync was successful");
  }
    
  // ---------------------------------------------------------------------------------------------
  // 6. add to the m_vectors
  // ---------------------------------------------------------------------------------------------
  if ( !add( v, t_transaction ) ){
	logger->log("info", """", "volsd", t_transaction, "failed to add to file/vector");
    logger->log("info", "", "volsd", t_transaction, "removing on hold device");
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }
   
  // now that volume is mounted, there is no need to keep the device in the devices_list since the
  // get_device function (Disks Class) will read all of the mounted devices on the server.
  utility::remove_element(m_devicesOnHold, v.device);

  return 1;
}

std::vector<utility::Volume>& Volumes::get_list(){
  return m_volumes;
}  
