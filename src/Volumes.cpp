#include "Volumes.h"


using namespace utility;

/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes ( std::string rmd, std::string volumeFile, int idleVolumes ){
  
  _rootMountDirectory = rmd;
  _volumeFile         = volumeFile;
  
  // Load Volumes information from file  
  load();
}

Volumes::Volumes( std::string rmd, std::string volumeFile ){
  _rootMountDirectory = rmd;
  _volumeFile = volumeFile;
}

/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes() {
  
}

/* =============================================================================
 * Class Destractor
 * =============================================================================*/
Volumes::~Volumes() {
  delete logger;
}


/* =============================================================================
 * Set Logger Attribute
 * =============================================================================*/
void Volumes::set_logger_att( bool toScreen, std::string logFile, int loglevel ) {
  logger = new Logger(toScreen, logFile, loglevel);
}


/* =============================================================================
 * Function: EBSVOLUME_CREATE
 * =============================================================================*/
int Volumes::ebsvolume_create(utility::Volume& volume, std::string& latestSnapshot, int transactionId, Logger& logger){

  // create a volume from latest snapshot
  int res = utility::exec(volume.id, "aws ec2 create-volume --region us-east-1 --availability-zone us-east-1a --snapshot-id " + latestSnapshot + " --volume-type gp2 --query VolumeId --output text");
  if (!res){
    logger.log("info", "infra1", "Volumes", transactionId, "failed to create volume, ExitCode(" + utility::to_string(res) + "). retry" + latestSnapshot + "]", "ebsvolume_create");
  }
  utility::clean_string(volume.id);

  // wait untill is created
  bool created = false;
  std::string status;
  sleep(2);

  utility::exec(status, "aws ec2 describe-volumes --volume-ids " + volume.id + " --region us-east-1  --query Volumes[*].State --output text");

  while (!created){
    if (status.find("available") != std::string::npos){
      created = true;

    }else {
      utility::exec(status, "aws ec2 describe-volumes --volume-ids " + volume.id + " --region us-east-1  --query Volumes[*].State --output text");
    }

  }

  //logger.log("info", "infra1", "Volumes", transactionId, "volume was created Successfully", "ebsvolume_create");
  volume.status     = "creating";

  return 1;
}


/* =============================================================================
 * Function: EBSVOLUME_CREATE
 * =============================================================================*/
int Volumes::create( std::string &t_volumeId, const std::string t_latestSnapshot, const int t_transactionId ){

  // create a volume from latest snapshot
  std::string newVolume;
  
  int res = utility::exec(newVolume, "aws ec2 create-volume --region us-east-1 --availability-zone us-east-1a --snapshot-id " + t_latestSnapshot + " --volume-type gp2 --query VolumeId --output text");
  if (!res){
    logger->log("info", "", "VolumesClass", t_transactionId, "failed to create volume, ExitCode(" + utility::to_string(res) + "). retry" + t_latestSnapshot + "]", "create");
  }
  utility::clean_string(newVolume);

  // wait untill is created
  bool created = false;
  std::string status;
  sleep(2);

  utility::exec(status, "aws ec2 describe-volumes --volume-ids " + newVolume + " --region us-east-1  --query Volumes[*].State --output text");

  while (!created){
    if (status.find("available") != std::string::npos){
      created = true;

    }else {
      utility::exec(status, "aws ec2 describe-volumes --volume-ids " + newVolume + " --region us-east-1  --query Volumes[*].State --output text");
    }

  }

  t_volumeId = newVolume;

  return 1;
}

/* =============================================================================
 * Function: EBSVOLUME_ATTACH
 * =============================================================================*/
int Volumes::ebsvolume_attach(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger){

  //logger.log("debug", "infra1", "Volumes", transactionId, "attaching volume:[" + volume.id + "]", "ebsvolume_attach");

  int retry=0;
  std::string output;
  bool attached = false;

  while (!attached) {
    int res = utility::exec(output, "/usr/bin/aws ec2 attach-volume --volume-id " + volume.id + " --instance-id " + ec2InstanceId + " --device /dev/" + volume.device + " --region us-east-1");
    utility::clean_string(output);
    if (retry == 30) {
      logger.log("error", "infra1", "Volumes", transactionId, "failed to attach volume. ExitCode(" + utility::to_string(res) + ").", "ebsvolume_attach");
      return 0;
    }
    if (!res) {
      logger.log("debug", "infra1", "Volumes", transactionId, "attching volume FAILED. ExitCode(" + utility::to_string(res) + ") retry..." , "ebsvolume_attach");
      logger.log("error", "infra1", "Volumes", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_attach");
      retry++;
    } else {
      //logger.log("debug", "infra1", "Volumes", transactionId, "volume attached successfully", "ebsvolume_attach");
      volume.attachedTo = "localhost";
      attached = true;
      return 2;
    }
    output = "";
    sleep(2);
  }



  return 1;
}


int Volumes::attach( const std::string t_volumeId, const std::string t_device, 
                     const std::string t_instanceId, const int t_transcation ) {
  

  int retry=0;
  std::string output;
  bool attached = false;

  while (!attached) {
    int res = utility::exec(output, "/usr/bin/aws ec2 attach-volume --volume-id " + t_volumeId + " --instance-id " + t_instanceId + " --device /dev/" + t_device + " --region us-east-1");
    utility::clean_string(output);
    if (retry == 30) {
      logger->log("error", "", "VolumesClass", t_transcation, "failed to attach volume. ExitCode(" + utility::to_string(res) + ").", "attach");
      return 0;
    }
    if (!res) {
      logger->log("debug", "", "VolumesClass", t_transcation, "attching volume FAILED. ExitCode(" + utility::to_string(res) + ") retry..." , "attach");
      logger->log("error", "", "VolumesClass", t_transcation, "AWSERROR:[" + output + "]", "ebsvolume_attach");
      retry++;
    } else {
      attached = true;
      return 1;
    }
    output = "";
    sleep(2);
  }



  return 1;
}

/* =============================================================================
 * Function: EBSVOLUME_MOUNT
 * =============================================================================*/
int Volumes::ebsvolume_mount(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger) {

  logger.log("debug", "infra1", "Volumes", transactionId, "Check if mountPoint:[" + volume.mountPoint + "] is valid one", "ebsvolume_mount");
  if (is_exist(volume.mountPoint, true, transactionId, logger)) {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint:[" + volume.mountPoint + "] cannot be used", "ebsvolume_mount");
    return 0;
  }

  // 2. check if mountPoint is already used
  logger.log("debug", "infra1", "Volumes", transactionId, "check if mountPoint:[" + volume.mountPoint + "] have a device mounted to it", "ebsvolume_mount");
  if (is_used(volume.mountPoint, true, transactionId, logger)) {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint:[" + volume.mountPoint + "] cannot be used, a device is mounted to it", "ebsvolume_mount");
    return 0;
  }


  logger.log("debug", "infra1", "Volumes", transactionId, "mounting volume:[" + volume.id + "] on device:[" + volume.device + "] on mountPoint:[" + volume.mountPoint + "]", "ebsvolume_mount");

  std::string output;
  bool mounted = false;
  int retry=0;

  while (!mounted) {
    int res = utility::exec(output, "mount /dev/" + volume.device + " " + volume.mountPoint); // return 0 for exist
    utility::clean_string(output);
    if (retry == 15) {
      logger.log("error", "infra1", "Volumes", transactionId, "failed to mount volume. EXIT(" + utility::to_string(res) + ").", "ebsvolume_mount");
      return 0;
    }
    if (!res) {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume was not mounted, EXIT(" + utility::to_string(res) + "). retry...", "ebsvolume_mount");
      logger.log("error", "infra1", "Volumes", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_mount");
      retry++;
    } else {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume was mounted successfully", "ebsvolume_mount");
      mounted = true;
    }
    output = "";
    sleep(2);

  }

  volume.status     = "idle";

  //sleep(3);
  return 1;
}


/* =============================================================================
 * Function: EBSVOLUME_MOUNT
 * =============================================================================*/
/*
//int Volumes::mount(utility::Volume& volume, int transactionId, Logger& logger) {
int Volumes::mount(utility::Volume& volume, int transactionId, Logger& logger) {
  
  // 1. check if the dir exist
  if (!utility::is_exist(volume.mountPoint)){
    // create the directory
    if (!utility::folder_create(volume.mountPoint)) {
      logger.log("debug", "infra1", "Volumes", transactionId, "cannot not create mountpoint:[" + volume.mountPoint+ "]", "mount");
      return 0;
    }
  } 
  
  // 2. check if it is used by another filesystem
  if (utility::is_mounted(volume.mountPoint)) {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountpoint cannot be used. It already been used by another filesystem", "mount");
    return 0;
  } 
  
  // 3. since it is exist, check if its already have data
  if (!utility::folder_is_empty(volume.mountPoint)) {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountpoint cannot be used. it has some data", "mount");
    return 0;
  }
  
  // all good, the start to mount the filesystem
  logger.log("debug", "infra1", "Volumes", transactionId, "mounting volume:[" + volume.id + "] on device:[" + volume.device + "] on mountPoint:[" + volume.mountPoint + "]", "mount");
  std::string output;
  if (!utility::mountfs( output, volume.mountPoint, "/dev/"+volume.device )){
	  logger.log("error", "infra1", "Volumes", transactionId, "failed to mount volume. " + output, "mount");
    return 0;
  }
  
  volume.status     = "idle";
  
  return 1;
}

*/
/* =============================================================================
 * Function: EBSVOLUME_MOUNT
 * =============================================================================*/


int Volumes::mount( const std::string t_volumeId, const std::string t_mountPoint, 
                    const std::string t_device, const int t_transactionId ) {
  
  // 1. check if the dir exist
  if (!utility::is_exist(t_mountPoint)){
    // create the directory
    if (!utility::folder_create(t_mountPoint)) {
      logger->log("debug", "", "VolumesClass", t_transactionId, "cannot not create mountpoint:[" + t_mountPoint + "]", "mount");
      return 0;
    }
  } 
  
  // 2. check if it is used by another filesystem
  if (utility::is_mounted(t_mountPoint)) {
    logger->log("debug", "", "VolumesClass", t_transactionId, "mountpoint cannot be used. It already been used by another filesystem", "mount");
    return 0;
  } 
  
  // 3. since it is exist, check if its already have data
  if (!utility::folder_is_empty(t_mountPoint)) {
    logger->log("debug", "", "VolumesClass", t_transactionId, "mountpoint cannot be used. it has some data", "mount");
    return 0;
  }
  
  // all good, the start to mount the filesystem
  logger->log("debug", "", "VolumesClass", t_transactionId, "mounting volume:[" + t_volumeId + "] on device:[" + t_device + "] on mountPoint:[" + t_mountPoint + "]", "mount");
  std::string output;
  if (!utility::mountfs( output, t_mountPoint, "/dev/"+t_device)){
	  logger->log("error", "", "VolumesClass", t_transactionId, "failed to mount volume. " + output, "mount");
    return 0;
  }
  
  
  
  return 1;
}


/* =============================================================================
 * Function: EBSVOLUME_DETACH
 * =============================================================================*/
int Volumes::ebsvolume_detach(utility::Volume& volume, int transactionId, Logger& logger){
  std::string output;
  logger.log("debug", "infra1", "Volumes", transactionId, "detaching EBS Volume:[" + volume.id + "] from instance", "ebsvolume_detach");

  int res = utility::exec(output, "/usr/bin/aws ec2 detach-volume --volume-id " + volume.id + " --region us-east-1");
  if (!res)   {
    logger.log("debug", "infra1", "Volumes", transactionId, "detaching volume[" + volume.id + "] failed. Exit(" + utility::to_string(res) + ")", "ebsvolume_detach");
    return 0;
  }

  int retry = 0;
  bool ready = false;

  // wait untill ready
  while (!ready){
    int res = utility::exec(output, "/usr/bin/aws ec2 describe-volumes --volume-id " + volume.id + " --query Volumes[*].State --output text --region us-east-1");
    utility::clean_string(output);

    if (retry == 15){
      logger.log("error", "infra1", "Volumes", transactionId, "volume:[" + volume.id + "] not detached. ExitCode(" + utility::to_string(res) + ")", "ebsvolume_detach");
      return 0;
    }

    if (!res){
      logger.log("error", "infra1", "Volumes", transactionId, "cannot describe volume", "ebsvolume_detach");
      logger.log("error", "infra1", "Volumes", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_detach");
      retry++;
    }
    if (output.find("in-use") != std::string::npos){
      logger.log("debug", "infra1", "Volumes", transactionId, "volume:[" + volume.id + "] is 'in-use'. retry ...", "ebsvolume_detach");
      retry++;
    } if (output.find("available") != std::string::npos){
      logger.log("debug", "infra1", "Volumes", transactionId, "volume[" + volume.id + "] was detached Successfuly", "ebsvolume_detach");
      ready = true;
      return 1;
    } else {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume[" + volume.id + "] was detached Successfuly", "ebsvolume_detach");
      ready = true;
      return 1;
    }
    output = "";
    sleep(2);
  }

  return 1;
}

int Volumes::detach( const std::string t_idleVolId, const int t_transactionId ){
  
  std::string output;
  logger->log("debug", "", "Volumes", t_transactionId, "detaching EBS Volume:[" + t_idleVolId + "] from instance", "");

  int res = utility::exec(output, "/usr/bin/aws ec2 detach-volume --volume-id " + t_idleVolId + " --region us-east-1");
  if (!res)   {
    logger->log("debug", "infra1", "Volumes", t_transactionId, "detaching volume[" + t_idleVolId + "] failed. Exit(" + utility::to_string(res) + ")", "ebsvolume_detach");
    return 0;
  }

  int retry = 0;
  bool ready = false;

  // wait untill ready
  while (!ready){
    int res = utility::exec(output, "/usr/bin/aws ec2 describe-volumes --volume-id " + t_idleVolId + " --query Volumes[*].State --output text --region us-east-1");
    utility::clean_string(output);

    if (retry == 15){
      logger->log("error", "", "Volumes", t_transactionId, "volume:[" + t_idleVolId + "] not detached. ExitCode(" + utility::to_string(res) + ")", "ebsvolume_detach");
      return 0;
    }

    if (!res){
      logger->log("error", "", "Volumes", t_transactionId, "cannot describe volume", "ebsvolume_detach");
      logger->log("error", "", "Volumes", t_transactionId, "AWSERROR:[" + output + "]", "ebsvolume_detach");
      retry++;
    }
    if (output.find("in-use") != std::string::npos){
      logger->log("debug", "", "Volumes", t_transactionId, "volume:[" + t_idleVolId + "] is 'in-use'. retry ...", "ebsvolume_detach");
      retry++;
    } if (output.find("available") != std::string::npos){
      logger->log("debug", "", "Volumes", t_transactionId, "volume[" + t_idleVolId + "] was detached Successfuly", "ebsvolume_detach");
      ready = true;
      return 1;
    } else {
      logger->log("debug", "", "Volumes", t_transactionId, "volume[" + t_idleVolId + "] was detached Successfuly", "ebsvolume_detach");
      ready = true;
      return 1;
    }
    output = "";
    sleep(2);
  }

  return 1;
}
 
/* =============================================================================
 * Function: EBSVOLUME_UMOUNT
 * =============================================================================*/
int Volumes::ebsvolume_umount(utility::Volume& volume, int transactionId, Logger& logger){
  std::string output;
  logger.log("debug", "infra1", "Volumes", transactionId, "umount volume:[" + volume.id + "] from:[" + volume.mountPoint + "]", "ebsvolume_umount");

  int retry = 0;
  bool umounted = false;

  while (!umounted) {
    int res = utility::exec(output, "umount -l " + volume.mountPoint + " 2>&1");
    utility::clean_string(output);

    if (retry == 15) {
      logger.log("error", "infra1", "Volumes", transactionId, "failed to umount volume. EXIT(" + utility::to_string(res) + ").", "ebsvolume_umount");
      return 0;
    }
    if (!res) {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume was not umounted, EXIT(" + utility::to_string(res) + "). retry...", "ebsvolume_umount");
      logger.log("error", "infra1", "Volumes", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_umount");
      retry++;
    } else {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume[" + volume.id + "] was unmounted Successfully. ExitCode(" + utility::to_string(res) + ")", "ebsvolume_umount");
      umounted = true;
      return 1;
    }
    output = "";
    sleep(2);

  }

  return 1;
}

bool Volumes::umount( const std::string t_volumeId, const std::string t_mountPoint, const int t_transactionId ){
  std::string output;
  logger->log("debug", "", "volumed", t_transactionId, "umount volume:[" + t_volumeId + "]", "umount");
  
  if (!utility::umountfs( output, t_mountPoint)) {
    logger->log("debug", "", "volumed", t_transactionId, "failed to umount volume:[" + t_volumeId + "]", "umount");
  }
  
  logger->log("debug", "", "volumed", t_transactionId, "umount volume:[" + t_volumeId + "] was successful", "umount");
  return true;
}



/* =============================================================================
 * Function: EBSVOLUME_DELETE
 * =============================================================================*/
int Volumes::ebsvolume_delete(utility::Volume& volume, int transactionId, Logger& logger){

  //logger.log("info", "infra1", "Volumes", transactionId, "deleting volume:[" + volume.id + "]", "ebsvolume_delete");
  std::string output;

  int retry = 0;
  bool deleted = false;

  while (!deleted) {
    int res = utility::exec(output, "aws ec2 delete-volume --volume-id " + volume.id + " --region us-east-1");
    utility::clean_string(output);
    if (retry == 15) {
      logger.log("error", "infra1", "Volumes", transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + ").", "ebsvolume_delete");
      return 0;
    }
    if (!res)   {
      logger.log("debug", "infra1", "Volumes", transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + "). retry...", "ebsvolume_delete");
      logger.log("error", "infra1", "Volumes", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_delete");
      retry++;
    } else {
      //logger.log("info", "infra1", "Volumes", transactionId, "volume was deleted, Exit(" + utility::to_string(res) + ").", "ebsvolume_delete");
      deleted = true;
      return 1;
    }
    output = "";
    sleep(2);
  }

  return 1;
}



int Volumes::del(const std::string t_volumeId, int t_transactionId){

  std::string output;

  int retry = 0;
  bool deleted = false;

  while (!deleted) {
    int res = utility::exec( output, "aws ec2 delete-volume --volume-id " + t_volumeId + " --region us-east-1");
    utility::clean_string(output);
    if (retry == 15) {
      logger->log("error", "", "VolumesClass", t_transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + ").", "del");
      return 0;
    }
    if (!res)   {
      logger->log("debug", "", "VolumesClass", t_transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + "). retry...", "del");
      logger->log("error", "", "VolumesClass", t_transactionId, "AWSERROR:[" + output + "]", "del");
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


/* =============================================================================
 * Function: EBSVOLUME_IDLE
 * =============================================================================*/
//bool Volumes::get_idle_volume(int &volumeIndex , int transactionId, Logger& logger) {
int Volumes::get_idle_volume( int &idleVolumeIndex, const int transactionId ) {
  
  // return an idle disk
  for (int i=0; i<m_volumes.size(); i++) {
    if ( m_volumes[i].status == "idle" ){
      logger->log("debug", "", "Volumes", transactionId, "volume found:","get_idle_volume");
      logger->log("debug", "", "Volumes", transactionId, "id:[" + m_volumes[i].id   + "] status:["     +
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


/* =============================================================================
 * Function: EBSVOLUME_IDLE_NUMBER
 * =============================================================================*/
int Volumes::get_idle_number() {
  
  int c =0;
  for (int i=0; i<m_volumes.size(); i++) {
    if ( m_volumes[i].status == "idle" ){
      c++;
    }
  }
  return c;  
}


/* =============================================================================
 * Function: EBSVOLUME_PRINT
 * =============================================================================*/
void Volumes::ebsvolume_print() {

  std::ifstream myFile;
  myFile.open(_volumeFile.c_str());

  if (!myFile.is_open()) {
    return;
  }
  std::cout << " ===================================================================\n";
  std::string line;
  while (std::getline(myFile, line)) {
    std::cout << line << "\n";
  }
  std::cout << " ===================================================================\n";

}


/* =============================================================================
 * Function: EBSVOLUME_LIST
 * =============================================================================*/
// std::string Volumes::ebsvolume_list(std::string select) {
std::string Volumes::ebsvolume_list(std::string select, std::string volumeFile) {

  std::string str;
  std::ifstream myFile;
  std::string line;

  myFile.open(volumeFile.c_str());

  if (!myFile.is_open()) {
    return "";
  }

  if ( select == "all") {
    while (std::getline(myFile, line)) {
      // get the whole line
      str.append(line + "\n");
    }
  } else if ( select == "local") {
    while (std::getline(myFile, line)) {
      if ( line.find("localhost") != std::string::npos ) {
  // get the id's only
  std::stringstream ss(line);
  std::string tmp_str;
  ss >> tmp_str; ss >> tmp_str; ss >> tmp_str; ss >> tmp_str;
  str.append(tmp_str + " ");
      }
    }
  } else if ( select == "remote") {
    while (std::getline(myFile, line)) {
      if ( line.find("localhost") == std::string::npos ) {
  // get the id's only
  std::stringstream ss(line);
  std::string tmp_str;
  ss >> tmp_str; ss >> tmp_str; ss >> tmp_str;
  str.append(tmp_str + " ");
      }
    }
  }
  return str;
}

bool Volumes::write_to_file( int transactionId ){
  // write back to file
  logger->log("debug", "", "volumed", transactionId, "writing changes to disk file", "write_to_file");
  
  std::ofstream myFileOut;
  myFileOut.open(_volumeFile.c_str(), std::fstream::out | std::fstream::trunc);

  if (!myFileOut.is_open()) {
    logger->log("error", "", "volumed", transactionId, "could not open disk file", "write_to_file");
    return false;
  }

  std::string line = "";
  
  for(std::vector<utility::Volume>::iterator it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    myFileOut << it->id << " " << it->status << " " << it->attachedTo 
              << " " << it->mountPoint << " " << it->device << "\n";
  }
  
  myFileOut.close();
  myFileOut.clear();
  
  return true;
}

/* =============================================================================
 * Function: EBSVOLUME_SETSTATUS
 * =============================================================================*/

bool Volumes::ebsvolume_setstatus(std::string op, std::string vol, std::string status, std::string ip, std::string mp, std::string d, int transactionId, Logger& logger){
/*
  // --------------------------------
  // Append to end of file
  // --------------------------------
  if (op == "add"){
    //std::cout << "EBSVOLUME_SETSTATUS:: ADD\n";
    logger.log("debug", "infra1", "Volumes", transactionId, "request to [add] volume to disk file", "ebsvolume_setstatus");
    std::ofstream out;
    out.open(_volumeFile.c_str(), std::ios::app);
    if (!out.is_open()){
      logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
      return 0;
    }
    std::string str = "I am here.";
    out << vol << " " << status << " " << ip << " " << mp << d << "\n";
    return 1;
  }

  // --------------------------------
  // Update record from file
  // --------------------------------
  if (op == "update")
  {
    //std::cout << "EBSVOLUME_SETSTATUS:: UPDATE\n";
    logger.log("debug", "infra1", "Volumes", transactionId, "request to [update] volume to disk file", "ebsvolume_setstatus");
    logger.log("debug", "infra1", "Volumes", transactionId, "updating: volId:[" + vol + "], status:[" + status + "], attachedTo:[" + ip + "], mountpoint:[" + mp + "], device:[" + d + "]", "ebsvolume_setstatus");
    std::ifstream myFile;
    std::string line;
    //int counter = 0;

    //utility::Volume *volumes = new utility::Volume[100];

    // 1) load into array
    //myFile.open(_volumeFile.c_str());
    //if (!myFile.is_open()){
    //  logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
    //  return 0;
    //}
    //while (std::getline(myFile, line)) {
    //  std::istringstream iss(line);
    //  std::string volId, status, attachedTo, mountPoint, device;
    //  iss >> volId >> status >> attachedTo >> mountPoint >> device;
    //  volumes[counter].id = volId;
    //  volumes[counter].status = status;
    //volumes[counter].attachedTo = attachedTo;
    //  volumes[counter].mountPoint = mountPoint;
    //  volumes[counter].device = device;
    //  counter++;
    //}
    //myFile.close();
    //myFile.clear();

    // 2) Look for the volume id to update
    for (int i=0; i<numOfElements; i++){
      if (vols[i].id == vol) {
        vols[i].status = status;
        vols[i].attachedTo = ip;
        vols[i].mountPoint = mp;
        vols[i].device = d;
      }
    }

    // 3) write back to file
    std::ofstream myFileOut;
    myFileOut.open(_volumeFile.c_str(), std::fstream::out | std::fstream::trunc);

    if (!myFileOut.is_open()) {
      logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
      return 0;
    }

    line = "";
    for (int i=0; i<numOfElements; i++){
      myFileOut << vols[i].id << " " << vols[i].status << " " << vols[i].attachedTo << " " << vols[i].mountPoint << " " << vols[i].device << "\n";
    }

    //delete[] volumes;
    myFileOut.close();
    return 1;
  }

  // --------------------------------
  // delete record from file
  // --------------------------------
  // remove an element from the array and shifts
  if (op == "delete"){
    logger.log("debug", "infra1", "Volumes", transactionId, " request to [delete] volume:[" + vol + "] from disk file", "ebsvolume_setstatus");
    std::ifstream myFile;
    std::string line;
    //int counter = 0;
    //utility::Volume *volumes = new utility::Volume[100];

    // 1) load into array
    //myFile.open(_volumeFile.c_str());
    //if (!myFile.is_open()){
     // logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
    //  return 0;
   // }

  //  while (std::getline(myFile, line))
  //  {
  //    std::istringstream iss(line);
  //    std::string volId, status, attachedTo, mountPoint, device;
  //    iss >> volId >> status >> attachedTo >> mountPoint >> device;
   //   volumes[counter].id = volId;
   //   volumes[counter].status = status;
  //    volumes[counter].attachedTo = attachedTo;
  //    volumes[counter].mountPoint = mountPoint;
  //    volumes[counter].device = device;
  //    counter++;
  //  }
  //  myFile.close();
  //  myFile.clear();


    // 2) Look for the volume id to update
    int location;
    for (int i=0; i<numOfElements; i++){
      if (vols[i].id == vol) {
        location = i;
      }
    }

    // 3) write back to file
    std::ofstream myFileOut;
    myFileOut.open(_volumeFile.c_str(), std::fstream::out | std::fstream::trunc);

    if (!myFileOut.is_open()) {
      logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
      return false;
    }

    line = "";
    for (int i=0; i<numOfElements; i++){
      if (i == location) {
        continue;
      }else{
        myFileOut << vols[i].id << " " << vols[i].status << " " << vols[i].attachedTo << " " << vols[i].mountPoint << " " << vols[i].device << "\n";
      }
    }
    //delete[] volumes;
    myFileOut.close();
    return 1;
  }
*/  
  return 1;
}



/* =============================================================================
 * Function: STATUS (NOT USED)
 * =============================================================================*/
int Volumes::status(std::string volume, bool d, int transactionId){
  std::string output;

  if (d) std::cout << " Volumes(" << transactionId << ")::STATUS: Checking Volume:" << volume << "] Status ...\n";
  int res = utility::exec(output, "/usr/bin/aws ec2 describe-volumes --volume-id " + volume + " --query Volumes[*].State --output text --region us-east-1");
  if (!res){
    if (d) std::cout << " Volumes(" << transactionId << ")::STATUS: ExitCode:" << res << ". Status command was not executed successfully\n\n";
    return 0;
  }
  utility::clean_string(output);
  if (output.find("in-use") != std::string::npos){
    if (d) std::cout << " Volumes(" << transactionId << ")::STATUS: Volume is 'in-use', this means it is attached\n";
    return 1;
  }else {
    if (d) std::cout << " Volumes(" << transactionId << ")::STATUS: Volume is not 'in-use', this means it is not attached\n";
    return 0;
  }
}


// -------------------------------------------------------------------------------------------------
// Function: Remove
// Used    : to remove a volumes from m_vectors
// -------------------------------------------------------------------------------------------------
int Volumes::remove ( const std::string t_volumeId, const int t_transactionId ){
  
  // delete volume
  for(std::vector<utility::Volume>::iterator it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    if ( it->id == t_volumeId )
      m_volumes.erase(it);
  }
    
  // write changes to disk
  m.lock();
  int res = write_to_file( transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "VolumesClass", t_transactionId, "failed to open file ", "remove");
    return 0;
  }
  
  return 1;
}


// -------------------------------------------------------------------------------------------------
// Function: Add
// Used    : to add a volumes to m_vectors
// -------------------------------------------------------------------------------------------------
int Volumes::add ( const utility::Volume t_volumes, const int t_transactionId ){
	
  // add to m_vectors
  m_volumes.push_back(t_volumes);
  
  // write changes to disk
  m.lock();
  int res = write_to_file( t_transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "volumed", t_transactionId, "failed to open file", "add");
    return 0;
  }
  
  return 1;
}


// -------------------------------------------------------------------------------------------------
// Function: update
// Used    : to update a volume in the m_vectors
// -------------------------------------------------------------------------------------------------
int Volumes::update ( const std::string t_volumeId, const std::string t_key, 
                      const std::string t_value,    const int t_transactionId){

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
    logger->log("debug", "", "volumed", t_transactionId, "could not find volume " + t_volumeId , "update");
    return 0;
  }
  
  // 2. update the value
  if ( t_key == "status") {
    m_volumes[i].status = t_value;
  } 
  
  // 3. write changes to disk
  m.lock();
  int res = write_to_file( t_transactionId );
  m.unlock();
  
  if (!res) {
    logger->log("debug", "", "volumed", t_transactionId, "failed to open file", "update");
    return 0;
  }
  
  return 1;
}


/* =============================================================================
 * Function: IS_EXIST
 * Description;
 *        Checks the mountpoint if exist or not. if not create it
 * =============================================================================*/
int Volumes::is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger){

  std::string output;
  // test1: check and see if mountPoint exists

  logger.log("debug", "infra1", "Volumes", transactionId, "checking If MountPoint:[" + mountPoint + "] exists" , "is_exist");
  //int res = utility::exec(output, "ls " + mountPoint + " 2>/dev/null"); // return 0 for exist
  int res = utility::exec(output, "ls " + mountPoint); // return 0 for exist
  // new code start here ------------
  // does not exist
  if (!res){
    logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint:[" + mountPoint + "] does not exist, creating ..." , "is_exist");

    std::string cmd = "mkdir " + mountPoint;
    system(cmd.c_str());
    return 0;
  }
  // Exist + empty = idle, ok to be used
  if ( (res) && (output.empty()) )  {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint exists and good to be used." , "is_exist");
    return 0;
  }
  // exist + not empty, means it is used by a filesystem
  if ( (res) && (!output.empty()) )  {
    logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint exist but it is used by another filesystem. Exit(" + utility::to_string(res) + ")" , "is_exist");
    return 1;
  }

  return 1;
}


/* =============================================================================
 * Function: IS_USED
 * =============================================================================*/
int Volumes::is_used(std::string mountPoint, bool d, int transactionId, Logger& logger){
  std::string output;

  //if (d) std::cout << " Volumes(" << transactionId << ")::IS_USED:: Checking If mountPoint:[" << mountPoint << "] is used by another filesystem\n";
  logger.log("debug", "infra1", "Volumes", transactionId, "checking If mountPoint:[" + mountPoint + "] is used by another filesystem" , "is_used");
  int res = utility::exec(output, "grep -qs " + mountPoint + " /proc/mounts");
  // check if mountPoint is already used
  if (res) {
    logger.log("error", "infra1", "Volumes", transactionId, "mountPoint is used by another filesystem. Exit(" + utility::to_string(res) + ")" , "is_used");
    //if (d) std::cout << " Volumes(" << transactionId << ")::IS_USED:: MountPoint:[" << mountPoint << "] is used by another filesystem. ExitCode(" << res << ")\n";
    return 1;
  }

  //if (d) std::cout << " Volumes(" << transactionId << ")::IS_USED:: MountPoint:[" << mountPoint << "] is available. ExitCode(" << res << ")\n";
  logger.log("debug", "infra1", "Volumes", transactionId, "mountPoint is available." , "is_used");
  return 0;
}


/* =============================================================================
 * Function: MK_FILESYSTEM
 * =============================================================================*/
int Volumes::make_filesystem(std::string device, bool d) {
  std::string output;
  // test1: check and see if mountPoint exists
  if (d) std::cout << " Disk::MAKE_FILESYSTEM: Making Filesystem on device:[" << device << "]\n";

  //int res = utility::exec(output, "mkfs -t ext4 " + device + " 2>/dev/null"); // return 0 for exist
  int res = utility::exec(output, "mkfs -t ext4 " + device); // return 0 for exist
  if (!res) {
    if (d) std::cout << " Disk::MAKE_FILESYSTEM: ExitCode:" << res << ". Failed to create a filesystem via mkfs exist\n";
    return 0;
  }
  if (d) std::cout << " Disk::MAKE_FILESYSTEM: ExitCode:" << res << ". a filesystem was made via mkfs succesfully\n";
  return 1;

}


/* =============================================================================
 * Function: RELEASE_VOLUME
 * =============================================================================*/
//int Volumes::release_volume( utility::Volume& volume, std::string instance_id, std::string mountPoint, bool d, int transactionId) {
int Volumes::release_volume( std::string& v, std::string instance_id, std::string mountPoint, bool d, int transactionId, Logger& logger) {


  std::string output;

  // 1. given a mountPoint, get the device name
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: Get the device name given mountPoint\n";
  int res = utility::exec(output,  "df " + mountPoint + " | grep -oP ^/dev/[a-z0-9/]+");
  if (!res) {
    if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". mountPoint does not exist\n";
    if (d) std::cout << " Volumes(" << transactionId << ")::" << "AWSERROR:[" << output << "]\n";

    return 0;
  }
  std::string device = output;
  output.clear();
  utility::clean_string(device);
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". device name found:[" << device << "]\n\n";


  // 2. given the device, get the volume id.
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: volume-id given device:[" << device << "] and instance-id:[" << instance_id << "]\n";
  std::string cmd = "aws ec2 describe-instances --instance-id " + instance_id + " --query \"Reservations[*].Instances[*].BlockDeviceMappings[\?DeviceName==\'" + device + "\'].Ebs.VolumeId\" --output text";
  if (d) std::cout << cmd << "\n";
  res = utility::exec(output,  cmd);
  if (!res) {
    if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". volume-id cout ne be aquired\n";
    return 0;
  }
  utility::clean_string(output);
  std::string vol = output;

  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". volume-id:[" << vol <<"]\n\n";


  utility::Volume volume;
  volume.id = vol;
  volume.device = device;
  volume.mountPoint = mountPoint;

  // now that we have the volume id, umount mountPoint and detach volume
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: Unmounting filesystem \n";
  if (!ebsvolume_umount(volume, transactionId, logger) ) {
    if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". filesystem could not be umounted\n";
    return 0;
  }
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". filesystem was umounted\n\n";

  // now detach file system
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: Attempting to detach Volume:[" << vol << "] from isntance:[" << instance_id << "]\n";
  if (!ebsvolume_detach(volume, transactionId, logger)) {
    if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". EBS was NOT detached\n";
    return 0;
  }
  if (d) std::cout << " Volumes(" << transactionId << ")::release_volume:: ExitCode:" << res << ". EBS was detached successfully\n\n";

  v = volume.id;
  return 1;
}


/* =============================================================================
 * Function: REMOVE_MOUNTPOINT
 * =============================================================================*/
int Volumes::remove_mountpoint(std::string mp, int transactionId, Logger& logger) {
  std::string output;
  logger.log("debug", "infra1", "Volumes", transactionId, "removig mountpoint:[" + mp + "]", "remove_mountpoint");

  int res = utility::exec(output, "rmdir " + mp );

  if (!res){
    logger.log("error", "infra1", "Volumes", transactionId, "failed to removig mountpoint:[" + mp + "]. Exit(" + utility::to_string(res) + ")", "remove_mountpoint");
    return 0;
  }

  logger.log("debug", "infra1", "Volumes", transactionId, "mountpoint:[" + mp + "] was removed Successfully", "remove_mountpoint");

  return 1;
}



/* =============================================================================
 * Function: GET_DEVICE
 * =============================================================================*/
std::string Volumes::get_device() {

  // get device already been used locally.
  std::string str;
  utility::exec(str, "lsblk | grep -oP xvd[a-z]*");
  str.append(" ");

  // get the devices from diskFile

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


/* =============================================================================
 * Function: GET_DEVICE
 * =============================================================================*/
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


int Volumes::volume_exist( const std::string volId ){

  std::vector<utility::Volume>::iterator it;
  for(it = m_volumes.begin(); it != m_volumes.end(); ++it) {
    if ( it->id == volId ) {
	  return 1;
	}
  }
  return 0;
}


int Volumes::release (std::string &volumeId, int transactionId) {
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 1. Get Idle Volume  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  int idleVolIndex;
  
  logger->log("info", "", "volumed", transactionId, "looking for idle volume");
  if ( !get_idle_volume( idleVolIndex, transactionId ) ) {
    logger->log("error", "", "volumed", transactionId, "no more idle volume available");
    return -1;
  } else {
    logger->log("info", "", "volumed", transactionId, "idle volume found:[" + m_volumes[idleVolIndex].id + "]");
    logger->log("info", "", "volumed", transactionId, "volume's status was changed to 'inprogress'");
    
    if (!update(m_volumes[idleVolIndex].id, "status", "inprogress", transactionId)) {
      logger->log("info", "", "volumed", transactionId, "failed to update volumes status");
    }
  }
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 2. unmount disk
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "volumed", transactionId, "umounting volume:[" + m_volumes[idleVolIndex].id + "]");
  
  if (!umount( m_volumes[idleVolIndex].id, m_volumes[idleVolIndex].mountPoint, transactionId )) {
    logger->log("error", "", "volumed", transactionId, "failed to umount volume");
    logger->log("info", "", "volumed", transactionId, "change the volume status to failedToUnmount");

    if (!update(m_volumes[idleVolIndex].id, "status", "failedToUnmount", transactionId)) {
      logger->log("info", "", "volumed", transactionId, "failed to update volumes status");
    }
    
    return -2;
  }
  logger->log("info", "", "volumed", transactionId, "volume umounted");
  
      
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 3. Detach the volume
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "Dispatcher", transactionId, "detaching volume");
  if (!detach(m_volumes[idleVolIndex].id, transactionId)) {
    logger->log("error", "", "Dispatcher", transactionId, "failed to detach volume");
        
    if (!update(m_volumes[idleVolIndex].id, "status", "failedToDetach", transactionId)) {
      logger->log("info", "", "volumed", transactionId, "failed to update volumes status");
    }
    return -3;
  }
  logger->log("info", "", "Dispatcher", transactionId, "volume was detached");

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  // 4. Remove MountPoint
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
  logger->log("info", "", "Dispatcher", transactionId, "remove mountpoint:[" + m_volumes[idleVolIndex].mountPoint +"]");
  if (!utility::folder_remove(m_volumes[idleVolIndex].mountPoint)){
    logger->log("error", "", "Dispatcher", transactionId, "could not remove mountpoint");
  }
  logger->log("info", "", "Dispatcher", transactionId, "mount point was removed");

  sleep(5);
  volumeId = m_volumes[idleVolIndex].id;
  
  return 1;
}


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


/* =============================================================================
 * Function: EBSVOLUME_PRINT
 * =============================================================================*/
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


/* =============================================================================
 * Function: remount
 * =============================================================================*/
void Volumes::remount(){
  std::string output;
  
  if ( m_volumes.size() == 0 ){
    logger->log("info", "infra1", "Volumes", 0, "there is no spare volumes to be mounted" , "remount");
    return;
  }
  
  // for each item in the vols, chck if mounted, if not, then remount
  for ( int i=0; i<m_volumes.size(); i++){
    
    if ( !utility::is_mounted(m_volumes[i].mountPoint) ){
    
      if ( !utility::mountfs(output, m_volumes[i].mountPoint, "/dev/"+m_volumes[i].device) ) {
        
        logger->log("info", "", "VolumesClass", 0, "cannot mount filesystem. " + output , "remount");
        // detach
        if ( !detach(m_volumes[i].id, 0) ) {
            logger->log("info", "", "VolumesClass", 0, "cannot detach volume. you have to do it manually" , "remount");
        }
        logger->log("info", "", "VolumesClass", 0, "volume was detach" , "remount");
        
        //remove from list
        if ( !remove(m_volumes[i].id, 0) ){
          logger->log("error", "", "VolumesClass", 0, "failed to write to file" , "remount");
        }
      }
    }
    
  }
  logger->log("info", "", "VolumesClass", 0, "all spare volumes are mounted successfully" , "remount");
}

int Volumes::acquire( const std::string t_targetFileSystem, const std::string t_snapshotId, 
                      const std::string t_rootMountsFolder, const std::string t_instanceId, 
                      const int         t_transaction 
                    ){
  
  utility::Volume v;
  
    
  // -----------------------------------------------------------------------------------------------   
  // 1) Prepare disk information.
  //------------------------------------------------------------------------------------------------
  
  // get a device. 
  logger->log("info", "", "volumesClass", t_transaction, "allocating a device.");
  v.device = get_device( m_devicesOnHold );
  m_devicesOnHold.push_back(v.device);
  logger->log("info", "", "volumesClass", t_transaction, "device allocated:[" + v.device + "]");
  logger->log("info", "", "volumesClass", t_transaction, "Devices List:[" + 
              utility::to_string(m_devicesOnHold) + "]"
             );
  
  // get mount point
  if ( t_rootMountsFolder[t_rootMountsFolder.length() - 1] != '/' ){
    v.mountPoint = t_rootMountsFolder + '/' + utility::randomString();
  } else {
    v.mountPoint = t_rootMountsFolder + utility::randomString();
  }
  
  logger->log("info", "", "volumesClass", t_transaction, "allocating mounting point:[" +  v.mountPoint + "]");
  

  
  // -----------------------------------------------------------------   
  // 2) Create Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volumesClass", t_transaction, "create Volume from latest snapshot:[" + t_snapshotId +"]");
  std::string newVolId;
  if ( !create( newVolId, t_snapshotId, t_transaction ) ){
    logger->log("error", "", "volumesClass", t_transaction, "FALIED to create a volume");
    //remove_device
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }
  v.id = newVolId;
  logger->log("info", "", "volumesClass", t_transaction, "volume:[" + v.id + "] creation was successful");
  
  // -----------------------------------------------------------------   
  // 3) Attach Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volumesClass", t_transaction, "attaching volume " + v.id);
  if ( !attach(v.id, v.device, t_instanceId, t_transaction ) ){
    //logger("error", hostname, "EBSManager::thread", transaction, "failed to attaching new disk to localhost. Removing...");
    logger->log("error", "", "volumesClass", t_transaction, "FAILED to attaching new disk to localhost. Removing  from AWS space");
    
    if (!del(v.id, t_transaction)){
    
      logger->log("error", "", "volumesClass", t_transaction, "FAILED to delete volume from AWS space");
    }
    logger->log("info", "", "volumesClass", t_transaction, "volume was removed from AWS space");
  
    //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
    logger->log("debug", "", "volumesClass", t_transaction, "removing device");
    //remove_device
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }
  v.attachedTo = "localhost";
  logger->log("info", "", "volumesClass", t_transaction, "volume was attached successfully");
  sleep(5);

  // -----------------------------------------------------------------   
  // 4) mount Volume
  // --------------------------------------------------------------- 
  logger->log("info", "", "volumesClass", t_transaction, "mounting volume into localhost");
  int retry=0;
  bool mounted = false;
  while (!mounted) {
    
    if ( !mount(v.id, v.mountPoint, v.device, t_transaction) ) {
      v.mountPoint = t_rootMountsFolder + utility::randomString();
      logger->log("info", "", "volumesClass", t_transaction, "FAILED to mount new volume. Retry with a new mountpoint " + v.mountPoint);
    } else {
      logger->log("info", "", "volumesClass", t_transaction, "new volume was mounted successfully");
      mounted = true;
    }
    
    if (retry == 5) {
      //logger("error", hostname, "EBSManager::thread", transaction, "failed to mount new disk to localhost. Detaching...");
      logger->log("error", "", "volumesClass", t_transaction, "FAILED to mount new disk to localhost. Detaching...");
      if (!detach(v.id, t_transaction))
        logger->log("error", "", "volumesClass", t_transaction, "FAILED to detach.");
  
        //logger("info", hostname, "EBSManager::thread", transaction, "removing device");
        logger->log("info", "", "volumesClass", t_transaction, "removing device");
        utility::remove_element(m_devicesOnHold, v.device);
        return 0;
    }
    
    retry++;
  }
  
  v.status     = "idle";
 
 
  std::cout << v.device << "\n";
  std::cout << v.mountPoint << "\n";
  std::cout << v.id << "\n";
  std::cout << v.attachedTo << "\n";
  std::cout << v.status << "\n";
 


  // -----------------------------------------------------------------------------------------------
  // 5. Sync Volume with the TargetFilesystemMountPoint
  // ----------------------------------------------------------------------------------------------- 
  logger->log("info", "", "volumesClass", t_transaction, "syncing new volume");
  std::string output;

  //logger->log("debug", "", "volumesClass", t_transcation, 
  //            "EXECMD:[echo 'sync " + v.mountPoint + " ' > /dev/tcp/infra1/" + 
  //            utility::to_string( conf.SyncerServicePort )
  //           );
              
  //int res = utility::exec(output, "echo 'sync " + v.mountPoint + "' > /dev/tcp/infra1/" + 
  //                        utility::to_string( conf.SyncerServicePort ) 
  //                       );
  
  Sync::synchronize( t_targetFileSystem, v.mountPoint, t_transaction, logger );
  
  int res;
  if (!res){
    logger->log("error", "", "volumesClass", t_transaction, "syntax error in command");
  }

    
  // ---------------------------------------------------------------------------------------------
  // 6. add to the m_vectors
  // ---------------------------------------------------------------------------------------------
  if ( !add( v, t_transaction ) ){
	logger->log("info", """", "volumesClass", t_transaction, "failed to add to file/vector");
    logger->log("info", "", "volumesClass", t_transaction, "removing on hold device");
    utility::remove_element(m_devicesOnHold, v.device);
    return 0;
  }
   
  // now that volume is mounted, there is no need to keep the device in the devices_list since the
  // get_device function (Disks Class) will read all of the mounted devices on the server.
  utility::remove_element(m_devicesOnHold, v.device);

  return 1;
}
