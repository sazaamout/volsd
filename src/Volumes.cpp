#include "Volumes.h"


using namespace utility;

/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes (bool debug, std::string rmd, std::string volumeFile, int idleVolumes){
  
  // this variable will not be used anymoe.
  _max_wait_time = 60;

  vols = new utility::Volume [100];

  _debug = debug;
  _rootMountDirectory = rmd;
  _volumeFile = volumeFile;
  
  // Load Volumes information from file  
  load();
  //printxyz();
}

Volumes::Volumes (bool debug, std::string rmd, std::string volumeFile){
  
  // this variable will not be used anymoe.
  _max_wait_time = 60;
  _debug = debug;
  _rootMountDirectory = rmd;
  _volumeFile = volumeFile;
}
/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes (bool debug) {
  //ebsvolume_load_from_file();
  _max_wait_time = 60;
  _debug = debug;
  _rootMountDirectory = "/mnt/diskManager/";
}

/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes () {
}

/* =============================================================================
 * Class Destractor
 * =============================================================================*/
Volumes::~Volumes() { 
  delete[] vols;
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


/* =============================================================================
 * Function: EBSVOLUME_IDLE
 * =============================================================================*/
bool Volumes::ebsvolume_idle(utility::Volume& v, int transactionId, Logger& logger) {
  logger.log("debug", "infra1", "Volumes", transactionId, "Searching for idle disk", "ebsvolume_idle");
  std::ifstream myFile;
  std::string line;
  myFile.open(_volumeFile.c_str());

  if (!myFile.is_open()){
      logger.log("error", "infra1", "Volumes", transactionId, "disk file faile to open", "ebsvolume_idle");
    return 0;
  }

  while (std::getline(myFile, line)) {
    if ( line.find("idle") != std::string::npos ){
      std::stringstream ss(line);
      ss >> v.id >> v.status >> v.attachedTo >> v.mountPoint >> v.device;
      logger.log("debug", "infra1", "Volumes", transactionId, "volume found:", "ebsvolume_idle");
      logger.log("debug", "infra1", "Volumes", transactionId, "id:[" + v.id   + "] status:["     +
                v.status     + "] attachedTo:[" +
            v.attachedTo + "] mountpoint:[" +
            v.mountPoint + "] device:["     +
            v.device     + "]",
            "ebsvolume_idle");
      return true;
    }
  }
  logger.log("error", "infra1", "Volumes", transactionId, "No Idle Disk was found", "ebsvolume_idle");
  myFile.close();
  myFile.clear();
  return false;

}


/* =============================================================================
 * Function: EBSVOLUME_IDLE_NUMBER
 * =============================================================================*/
int Volumes::ebsvolume_idle_number() {
  int c =0;

  std::ifstream myFile;
  myFile.open(_volumeFile.c_str());

  if (!myFile.is_open()) {
    return 0;
  }


  std::string line;
  while (std::getline(myFile, line)) {
    if (line.find("idle") != std::string::npos)
      c++;
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


/* =============================================================================
 * Function: EBSVOLUME_SETSTATUS
 * =============================================================================*/
bool Volumes::ebsvolume_setstatus(std::string op, std::string vol, std::string status, std::string ip, std::string mp, std::string d, int transactionId, Logger& logger){

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
    int counter = 0;

    utility::Volume *volumes = new utility::Volume[100];

    // 1) load into array
    myFile.open(_volumeFile.c_str());
    if (!myFile.is_open()){
      logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
      return 0;
    }
    while (std::getline(myFile, line)) {
      std::istringstream iss(line);
      std::string volId, status, attachedTo, mountPoint, device;
      iss >> volId >> status >> attachedTo >> mountPoint >> device;
      volumes[counter].id = volId;
      volumes[counter].status = status;
      volumes[counter].attachedTo = attachedTo;
      volumes[counter].mountPoint = mountPoint;
      volumes[counter].device = device;
      counter++;
    }
    myFile.close();
    myFile.clear();

    // 2) Look for the volume id to update
    for (int i=0; i<counter; i++){
      if (volumes[i].id == vol) {
  volumes[i].status = status;
  volumes[i].attachedTo = ip;
  volumes[i].mountPoint = mp;
  volumes[i].device = d;
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
    for (int i=0; i<counter; i++){
      myFileOut << volumes[i].id << " " << volumes[i].status << " " << volumes[i].attachedTo << " " << volumes[i].mountPoint << " " << volumes[i].device << "\n";
    }

    delete[] volumes;
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
    int counter = 0;
    utility::Volume *volumes = new utility::Volume[100];

    // 1) load into array
    myFile.open(_volumeFile.c_str());
    if (!myFile.is_open()){
      logger.log("error", "infra1", "Volumes", transactionId, "could not open disk file", "ebsvolume_setstatus");
      return 0;
    }

    while (std::getline(myFile, line))
    {
      std::istringstream iss(line);
      std::string volId, status, attachedTo, mountPoint, device;
      iss >> volId >> status >> attachedTo >> mountPoint >> device;
      volumes[counter].id = volId;
      volumes[counter].status = status;
      volumes[counter].attachedTo = attachedTo;
      volumes[counter].mountPoint = mountPoint;
      volumes[counter].device = device;
      counter++;
    }
    myFile.close();
    myFile.clear();

    // 2) Look for the volume id to update
    int location;
    for (int i=0; i<counter; i++){
      if (volumes[i].id == vol) {
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
    for (int i=0; i<counter; i++){
      if (i == location) {
        continue;
      }else{
        myFileOut << volumes[i].id << " " << volumes[i].status << " " << volumes[i].attachedTo << " " << volumes[i].mountPoint << " " << volumes[i].device << "\n";
      }
    }
    delete[] volumes;
    myFileOut.close();
    return 1;
  }
  
  return 1;
}

/* =============================================================================
 * Function: STATUS
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

  // given a mountPoint, get the device name
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


  // given the device, get the volume id.
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


/* =============================================================================
 * Function: EBSVOLUME_EXIST
 * Description:
 *        given a volumeId, checks if a volume exist or not in the disk file.
 * =============================================================================*/
int Volumes::ebsvolume_exist(std::string volId){

  std::ifstream myFile;
  std::string line;
  myFile.open(_volumeFile.c_str());
  if (!myFile.is_open())
    return 0;
  while (std::getline(myFile, line)) {
    if ( line.find(volId) !=std::string::npos )
      return 1;
  }
  myFile.close();
  myFile.clear();
  return 0;
}

/*
utility::ReturnValue Volumes::release () {
  // this will perform the following tasks
  // 1. get an idle volume
  // 2. umount that volume from the localhost
  // 3. detach that volume from localhost
  // 4. change the volume status to 'inprogress'
  
  std::string volId;
  utility::ReturnValue retval;
  
  retval.debug.push_back('searching for idle volume');
    
  
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // 1. Get Idle Volume  
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
    if ( ebsvolume_idle(v, transactionId, logger) ) {
      logger.log("error", hostname, "Dispatcher", transactionId, "no more idle volume available", "DiskRequestThread");
      return -1;
    } else {
      logger.log("info", hostname, "Dispatcher", transactionId, "idle volume found:[" + v.id + "]", "DiskRequestThread");
      logger.log("info", hostname, "Dispatcher", transactionId, "set new volume's status to 'inprogress'", "DiskRequestThread");
      m.lock();
      if (!dc.ebsvolume_setstatus( "update", v.id, "inprogress", "none", "none", "none", transactionId, logger)){
  logger.log("error", hostname, "Dispatcher", transactionId, "failed to update disk status", "DiskRequestThread");
      }
      m.unlock();
      
      logger.log("info", hostname, "Dispatcher", transactionId, "umounting volume", "DiskRequestThread");
      
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      // 2. unmount disk
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      if (!dc.ebsvolume_umount(v, transactionId, logger)) {
  logger.log("error", hostname, "Dispatcher", transactionId, "failed to umount volume", "DiskRequestThread");
  logger.log("info", hostname, "Dispatcher", transactionId, "remove volume from volume list", "DiskRequestThread");
  m.lock();
  if (!dc.ebsvolume_setstatus( "delete", v.id, "", "", "", "", transactionId, logger)) {
    logger.log("error", hostname, "Dispatcher", transactionId, "failed to update disk status", "DiskRequestThread");
  }
  m.unlock();
  return -2;
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "volume umounted", "DiskRequestThread");
    
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      // 3. Detach the volume
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  
      logger.log("info", hostname, "Dispatcher", transactionId, "detaching volume", "DiskRequestThread");
      if (!dc.ebsvolume_detach(v, transactionId, logger)) {
  logger.log("error", hostname, "Dispatcher", transactionId, "failed to detach volume", "DiskRequestThread");
  return -3;
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "volume was detached", "DiskRequestThread");
      
      logger.log("info", hostname, "Dispatcher", transactionId, "remove mountpoint:[" + v.mountPoint +"]", "DiskRequestThread");
      if (!dc.remove_mountpoint(v.mountPoint, transactionId, logger)){
  logger.log("error", hostname, "Dispatcher", transactionId, "could not remove mountpoint", "DiskRequestThread");
      }
      logger.log("info", hostname, "Dispatcher", transactionId, "mount point was removed", "DiskRequestThread");
    }

    sleep(5);
    return 1;
}
*/

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
  counter=0;
  while (std::getline(myFile, line)) {
      std::size_t vol_pos = line.find(' ', 0);
      vols[counter].id = line.substr(0, vol_pos);
      
      std::size_t status_pos = line.find(' ', vol_pos+1);
      vols[counter].status = line.substr(vol_pos+1, status_pos - vol_pos-1);
      
      std::size_t mountServer_pos = line.find(' ', status_pos+1);
      vols[counter].attachedTo = line.substr(status_pos+1, mountServer_pos-status_pos-1);
      
      std::size_t mountPoint_pos = line.find(' ', mountServer_pos+1);
      vols[counter].mountPoint = line.substr(mountServer_pos+1, mountPoint_pos-mountServer_pos-1);
      
      std::size_t device_pos = line.find(' ', mountPoint_pos+1);
      vols[counter].device = line.substr(mountPoint_pos+1, device_pos-mountPoint_pos-1);
  
      counter++;
  }
  
  
  myFile.close();
  myFile.clear();

  return 0;
	
}


/* =============================================================================
 * Function: EBSVOLUME_PRINT
 * =============================================================================*/
void Volumes::printxyz() {
  if (counter != 0)
    for (int i=0; i<counter; i++)
      std::cout << "VolId:[" << vols[i].id << "] statu:[" << vols[i].status << "] attac:[" << vols[i].attachedTo << "] mount:[" << vols[i].mountPoint << "] devic:[" << vols[i].device << "]\n";
}


/* =============================================================================
 * Function: remount
 * =============================================================================*/
void Volumes::remount(Logger &logger){
  std::string output;
  if (counter == 0){
    logger.log("info", "infra1", "Volumes", 0, "no volumes to mount" , "remount");
    return;
  }
  // for each item in the vols, chck if mounted, if not, then remount
  for ( int i=0; i<counter; i++){
    if (!utility::is_mounted(vols[i].mountPoint)){
      if (!utility::mountfs(output, vols[i].mountPoint, "/dev/"+vols[i].device)) {
        
        logger.log("info", "infra1", "Volumes", 0, "cannot mount filesystem. " + output , "remount");
        // detach
        if (!ebsvolume_detach(vols[i], 0, logger)) {
            logger.log("info", "infra1", "Volumes", 0, "cannot detach volume. you have to do it manually" , "remount");
        }
        logger.log("info", "infra1", "Volumes", 0, "volume was detach" , "remount");
        
        //remove from list
        ebsvolume_setstatus("delete", vols[i].id, vols[i].status, vols[i].attachedTo, vols[i].mountPoint, vols[i].device, 0, logger);
      }
    }
  }
  logger.log("info", "infra1", "Volumes", 0, "all volumes are mounted successfully" , "remount");
}
