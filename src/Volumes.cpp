#include "Volumes.h"


using namespace utility;

/* =============================================================================
 * Class Constructor
 * =============================================================================*/
Volumes::Volumes (bool debug, std::string rmd, std::string volumeFile){
        // Load Volumes information from file


        //counter = 0;

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
        // Load Volumes information from file
        //volumes = new utility::Volume[20];
        //counter = 0;

        //ebsvolume_load_from_file();
        _max_wait_time = 60;
        _debug = debug;
        _rootMountDirectory = "/mnt/diskManager/";
}


/* =============================================================================
 * Class Destractor
 * =============================================================================*/
Volumes::~Volumes() { }


/* =============================================================================
 * Function: EBSVOLUME_CREATE
 * =============================================================================*/
int Volumes::ebsvolume_create(utility::Volume& volume, std::string& latestSnapshot, int transactionId, Logger& logger){

        //if (_debug) std::cout << " Volumes(" << transactionId << ")::Ebsvolume_create: creating a new volume from lates snapshot:[" << latestSnapshot << "]\n";
        //logger.log("debug", "infra1", "VolumesClass", transactionId, "creating a new volume from latest snapshot:[" + latestSnapshot + "]", "ebsvolume_create");



        // create a volume from latest snapshot
        int res = utility::exec(volume.id, "aws ec2 create-volume --region us-east-1 --availability-zone us-east-1a --snapshot-id " + latestSnapshot + " --volume-type gp2 --query VolumeId --output text");
        if (!res){
                logger.log("info", "infra1", "VolumesClass", transactionId, "failed to create volume, ExitCode(" + utility::to_string(res) + "). retry" + latestSnapshot + "]", "ebsvolume_create");
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

        //logger.log("info", "infra1", "VolumesClass", transactionId, "volume was created Successfully", "ebsvolume_create");
        volume.status     = "creating";

        return 1;
}


/* =============================================================================
 * Function: EBSVOLUME_ATTACH
 * =============================================================================*/
int Volumes::ebsvolume_attach(utility::Volume& volume, std::string& ec2InstanceId, int transactionId, Logger& logger){

        //logger.log("debug", "infra1", "VolumesClass", transactionId, "attaching volume:[" + volume.id + "]", "ebsvolume_attach");

        int retry=0;
        std::string output;
        bool attached = false;

        while (!attached) {
                int res = utility::exec(output, "/usr/bin/aws ec2 attach-volume --volume-id " + volume.id + " --instance-id " + ec2InstanceId + " --device /dev/" + volume.device + " --region us-east-1");
                utility::clean_string(output);
                if (retry == 30) {
                        logger.log("error", "infra1", "VolumesClass", transactionId, "failed to attach volume. ExitCode(" + utility::to_string(res) + ").", "ebsvolume_attach");
                        return 0;
                }
                if (!res) {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "attching volume FAILED. ExitCode(" + utility::to_string(res) + ") retry..." , "ebsvolume_attach");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_attach");
                        retry++;
                } else {
                        //logger.log("debug", "infra1", "VolumesClass", transactionId, "volume attached successfully", "ebsvolume_attach");
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

        logger.log("debug", "infra1", "VolumesClass", transactionId, "Check if mountPoint:[" + volume.mountPoint + "] is valid one", "ebsvolume_mount");
        if (is_exist(volume.mountPoint, true, transactionId, logger)) {
                logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint:[" + volume.mountPoint + "] cannot be used", "ebsvolume_mount");
                return 0;
        }

        // 2. check if mountPoint is already used
        logger.log("debug", "infra1", "VolumesClass", transactionId, "check if mountPoint:[" + volume.mountPoint + "] have a device mounted to it", "ebsvolume_mount");
        if (is_used(volume.mountPoint, true, transactionId, logger)) {
                logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint:[" + volume.mountPoint + "] cannot be used, a device is mounted to it", "ebsvolume_mount");
                return 0;
        }


        logger.log("debug", "infra1", "VolumesClass", transactionId, "mounting volume:[" + volume.id + "] on device:[" + volume.device + "] on mountPoint:[" + volume.mountPoint + "]", "ebsvolume_mount");

        std::string output;
        bool mounted = false;
        int retry=0;

        while (!mounted) {
                int res = utility::exec(output, "mount /dev/" + volume.device + " " + volume.mountPoint); // return 0 for exist
                utility::clean_string(output);
                if (retry == 15) {
                        logger.log("error", "infra1", "VolumesClass", transactionId, "failed to mount volume. EXIT(" + utility::to_string(res) + ").", "ebsvolume_mount");
                        return 0;
                }
                if (!res) {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume was not mounted, EXIT(" + utility::to_string(res) + "). retry...", "ebsvolume_mount");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_mount");
                        retry++;
                } else {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume was mounted successfully", "ebsvolume_mount");
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
 * Function: EBSVOLUME_DETACH
 * =============================================================================*/
int Volumes::ebsvolume_detach(utility::Volume& volume, int transactionId, Logger& logger){
        std::string output;
        logger.log("debug", "infra1", "VolumesClass", transactionId, "detaching EBS Volume:[" + volume.id + "] from instance", "ebsvolume_detach");

        int res = utility::exec(output, "/usr/bin/aws ec2 detach-volume --volume-id " + volume.id + " --region us-east-1");
        if (!res)   {
                logger.log("debug", "infra1", "VolumesClass", transactionId, "detaching volume[" + volume.id + "] failed. Exit(" + utility::to_string(res) + ")", "ebsvolume_detach");
                return 0;
        }

        int retry = 0;
        bool ready = false;

        // wait untill ready
        while (!ready){
                int res = utility::exec(output, "/usr/bin/aws ec2 describe-volumes --volume-id " + volume.id + " --query Volumes[*].State --output text --region us-east-1");
                utility::clean_string(output);

                if (retry == 15){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "volume:[" + volume.id + "] not detached. ExitCode(" + utility::to_string(res) + ")", "ebsvolume_detach");
                        return 0;
                }

                if (!res){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "cannot describe volume", "ebsvolume_detach");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_detach");
                        retry++;
                }
                if (output.find("in-use") != std::string::npos){
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume:[" + volume.id + "] is 'in-use'. retry ...", "ebsvolume_detach");
                        retry++;
                } if (output.find("available") != std::string::npos){
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume[" + volume.id + "] was detached Successfuly", "ebsvolume_detach");
                        ready = true;
                        return 1;
                } else {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume[" + volume.id + "] was detached Successfuly", "ebsvolume_detach");
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
        logger.log("debug", "infra1", "VolumesClass", transactionId, "umount volume:[" + volume.id + "] from:[" + volume.mountPoint + "]", "ebsvolume_umount");

        int retry = 0;
        bool umounted = false;

        while (!umounted) {
                int res = utility::exec(output, "umount -l " + volume.mountPoint + " 2>&1");
                utility::clean_string(output);

                if (retry == 15) {
                        logger.log("error", "infra1", "VolumesClass", transactionId, "failed to umount volume. EXIT(" + utility::to_string(res) + ").", "ebsvolume_umount");
                        return 0;
                }
                if (!res) {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume was not umounted, EXIT(" + utility::to_string(res) + "). retry...", "ebsvolume_umount");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_umount");
                        retry++;
                } else {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume[" + volume.id + "] was unmounted Successfully. ExitCode(" + utility::to_string(res) + ")", "ebsvolume_umount");
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

        //logger.log("info", "infra1", "VolumesClass", transactionId, "deleting volume:[" + volume.id + "]", "ebsvolume_delete");
        std::string output;

        int retry = 0;
        bool deleted = false;

        while (!deleted) {
                int res = utility::exec(output, "aws ec2 delete-volume --volume-id " + volume.id + " --region us-east-1");
                utility::clean_string(output);
                if (retry == 15) {
                        logger.log("error", "infra1", "VolumesClass", transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + ").", "ebsvolume_delete");
                        return 0;
                }
                if (!res)   {
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume failed to delete, Exit(" + utility::to_string(res) + "). retry...", "ebsvolume_delete");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "AWSERROR:[" + output + "]", "ebsvolume_delete");
                        retry++;
                } else {
                        //logger.log("info", "infra1", "VolumesClass", transactionId, "volume was deleted, Exit(" + utility::to_string(res) + ").", "ebsvolume_delete");
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
        logger.log("debug", "infra1", "VolumesClass", transactionId, "Searching for idle disk", "ebsvolume_idle");
        std::ifstream myFile;
        std::string line;
        myFile.open(_volumeFile.c_str());

        if (!myFile.is_open()){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "disk file faile to open", "ebsvolume_idle");
                return 0;
        }

        while (std::getline(myFile, line)) {
                if ( line.find("idle") != std::string::npos ){
                        std::stringstream ss(line);
                        ss >> v.id >> v.status >> v.attachedTo >> v.mountPoint >> v.device;
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "volume found:", "ebsvolume_idle");
                        logger.log("debug", "infra1", "VolumesClass", transactionId, "id:[" + v.id         + "] status:["     +
                                                                                                                                                                            v.status     + "] attachedTo:[" +
                                                                                                                                                                        v.attachedTo + "] mountpoint:[" +
                                                                                                                                                                        v.mountPoint + "] device:["     +
                                                                                                                                                                        v.device     + "]",
                                                                                                                                                                        "ebsvolume_idle");
                        return true;
                }
        }
        logger.log("error", "infra1", "VolumesClass", transactionId, "No Idle Disk was found", "ebsvolume_idle");
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
                logger.log("debug", "infra1", "VolumesClass", transactionId, "request to [add] volume to disk file", "ebsvolume_setstatus");
                std::ofstream out;
                out.open(_volumeFile.c_str(), std::ios::app);
                if (!out.is_open()){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "could not open disk file", "ebsvolume_setstatus");
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
                logger.log("debug", "infra1", "VolumesClass", transactionId, "request to [update] volume to disk file", "ebsvolume_setstatus");
                logger.log("debug", "infra1", "VolumesClass", transactionId, "updating: volId:[" + vol + "], status:[" + status + "], attachedTo:[" + ip + "], mountpoint:[" + mp + "], device:[" + d + "]", "ebsvolume_setstatus");
                std::ifstream myFile;
                std::string line;
                int counter = 0;

                utility::Volume *volumes = new utility::Volume[100];

                // 1) load into array
                myFile.open(_volumeFile.c_str());
                if (!myFile.is_open()){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "could not open disk file", "ebsvolume_setstatus");
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
                        logger.log("error", "infra1", "VolumesClass", transactionId, "could not open disk file", "ebsvolume_setstatus");
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
        int index;
        if (op == "delete"){
                logger.log("debug", "infra1", "VolumesClass", transactionId, " request to [delete] volume:[" + vol + "] from disk file", "ebsvolume_setstatus");
                std::ifstream myFile;
                std::string line;
                int counter = 0;
                utility::Volume *volumes = new utility::Volume[100];

                // 1) load into array
                myFile.open(_volumeFile.c_str());
                if (!myFile.is_open()){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "could not open disk file", "ebsvolume_setstatus");
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
                        logger.log("error", "infra1", "VolumesClass", transactionId, "could not open disk file", "ebsvolume_setstatus");
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
 *              Cgecks the mountpoint if exist or not. if not create it
 * =============================================================================*/
int Volumes::is_exist(std::string mountPoint, bool d, int transactionId, Logger& logger){

        std::string output;
        // test1: check and see if mountPoint exists

        logger.log("debug", "infra1", "VolumesClass", transactionId, "checking If MountPoint:[" + mountPoint + "] exists" , "is_exist");
        //int res = utility::exec(output, "ls " + mountPoint + " 2>/dev/null"); // return 0 for exist
        int res = utility::exec(output, "ls " + mountPoint); // return 0 for exist
        // new code start here ------------
        // does not exist
        if (!res){
                logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint:[" + mountPoint + "] does not exist, creating ..." , "is_exist");

                std::string cmd = "mkdir " + mountPoint;
                system(cmd.c_str());
                return 0;
        }
        // Exist + empty = idle, ok to be used
        if ( (res) && (output.empty()) )  {
                logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint exists and good to be used." , "is_exist");
                return 0;
        }
        // exist + not empty, means it is used by a filesystem
        if ( (res) && (!output.empty()) )  {
                logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint exist but it is used by another filesystem. Exit(" + utility::to_string(res) + ")" , "is_exist");
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
        logger.log("debug", "infra1", "VolumesClass", transactionId, "checking If mountPoint:[" + mountPoint + "] is used by another filesystem" , "is_used");
        int res = utility::exec(output, "grep -qs " + mountPoint + " /proc/mounts");
        // check if mountPoint is already used
        if (res) {
                logger.log("error", "infra1", "VolumesClass", transactionId, "mountPoint is used by another filesystem. Exit(" + utility::to_string(res) + ")" , "is_used");
                //if (d) std::cout << " Volumes(" << transactionId << ")::IS_USED:: MountPoint:[" << mountPoint << "] is used by another filesystem. ExitCode(" << res << ")\n";
                return 1;
        }

        //if (d) std::cout << " Volumes(" << transactionId << ")::IS_USED:: MountPoint:[" << mountPoint << "] is available. ExitCode(" << res << ")\n";
        logger.log("debug", "infra1", "VolumesClass", transactionId, "mountPoint is available." , "is_used");
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
        logger.log("debug", "infra1", "VolumesClass", transactionId, "removig mountpoint:[" + mp + "]", "remove_mountpoint");

        int res = utility::exec(output, "rmdir " + mp );

        if (!res){
                logger.log("error", "infra1", "VolumesClass", transactionId, "failed to removig mountpoint:[" + mp + "]. Exit(" + utility::to_string(res) + ")", "remove_mountpoint");
                return 0;
        }

        logger.log("debug", "infra1", "VolumesClass", transactionId, "mountpoint:[" + mp + "] was removed Successfully", "remove_mountpoint");

        return 1;
}


/* =============================================================================
 * Function: SYNC_FILESYSTEM
 * =============================================================================*/
 /*
int Volumes::ebsvolume_sync(std::stringstream& ss, std::string op, std::string path, std::string source, Logger& logger){
        //Logger logger(true);

        logger.log("debug", "infra1", "VolumesClass", transactionId, "pushing:[" + path + "] from admin to all production volumes", "ebsvolume_sync");
        std::string o, mp, att, output;

        // 1) sync targetfile system
        int res = utility::exec(output, "/home/cde/saad/code/DiskManager/bin/local-push.sh " + path + " /mnt/production/cde " + source);
        utility::clean_string(output);
        if (!res){
                logger.log("error", "infra1", "VolumesClass", transactionId, "puhsing failed for:[/mnt/production/cde]", "ebsvolume_sync");
                logger.log("error", "infra1", "VolumesClass", transactionId, "CMDERROR:[" + output + "]", "ebsvolume_sync");
        }
        // for some reason, there is always extra space appended to the ss. I had to do it this way so I can eleminate the space
        //ss << output.substr(0, output.length()-1);
        ss << output;
        output.clear();


        // 2) sync local volumes.
        //    the sync will alwasy be from admin filesystem to production file system
        //./remote-push.sh /home/cde/folder/file /mnt/diskManager/xYx89jsoI9/ /mnt/production/cde/
        std::stringstream l_ss(ebsvolume_list("local"));

        for (int ii = 0; l_ss >> mp; ii++){
                int res = utility::exec(output, "/home/cde/saad/code/DiskManager/bin/local-push.sh " + path + " " + mp + " " + source);
                utility::clean_string(output);
                if (!res){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "puhsing failed for:[" +  mp +"]", "ebsvolume_sync");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "CMDERROR:[" + output + "]", "ebsvolume_sync");
                }
                // for some reason, there is always extra space appended to the ss. I had to do it this way so I can eleminate the space
                //ss << output.substr(0, output.length()-1);
                ss << output;
                output.clear();
        }

        // 3) sync globally
        //./remote-push.sh /home/cde/htdocs /mnt/production/cde/
        std::stringstream g_ss(ebsvolume_list("remote"));

        for (int ii = 0; g_ss >> att; ii++){
                //ss << att << "\n";
                int res = utility::exec(output, "/home/cde/saad/code/DiskManager/bin/remote-push.sh " + path + " " + att);
                utility::clean_string(output);
                if (!res){
                        logger.log("error", "infra1", "VolumesClass", transactionId, "puhsing failed for:[" +  mp +"]", "ebsvolume_sync");
                        logger.log("error", "infra1", "VolumesClass", transactionId, "CMDERROR:[" + output + "]", "ebsvolume_sync");
                }
                // for some reason, there is always extra space appended to the ss. I had to do it this way so I can eleminate the space
                //ss << output.substr(0, output.length()-1);
                ss << output;
                output.clear();
        }
        logger.log("debug", "infra1", "VolumesClass", transactionId, "pushing:[" + path + "] is done", "ebsvolume_sync");


}

*/
/*
int Volumes::ebsvolume_sync(std::string destination, std::string source, int transcationId, Logger& logger){
        std::string output;

        // ensure that source and destination always have '/' at the ned of it.
        if ( source[source.length()-1] != '/' )
                source.append("/");

        if ( destination[destination.length()-1] != '/' )
                destination.append("/");

        logger.log("debug", "infra1", "VolumesClass", transactionId, "rsync -avlpt " + source + " " + destination, "ebsvolume_sync1");

        int res = utility::exec(output, "rsync -avlpt " + source + " " + destination);
        if (!res)
                return 0;

        return 1;
}
*/
/*
int Volumes::ebsvolume_sync(std::string source, int transcationId, Logger& logger){

        logger.log("debug", "infra1", "VolumesClass", transactionId, "syncing all local and remote volumes to the target filesystem", "ebsvolume_sync");

        if ( source[source.length()-1] != '/' )
                source.append("/");

        std::string o, mp, att, output;
        std::stringstream l_ss(ebsvolume_list("local"));

        // 1) sync locally
        for (int ii = 0; l_ss >> mp; ii++){
                if ( mp[mp.length()-1] != '/' )
                mp.append("/");
                logger.log("debug", "infra1", "VolumesClass", transactionId, "CMD:[rsync -avlpt " + source + " " + mp, "ebsvolume_sync");
                int res = utility::exec(output, "rsync -avlpt --delete" + source + " " + mp);
                if (!res)
                        return 0;
        }

        // 2) sync globally
        std::stringstream g_ss(ebsvolume_list("remote"));

        for (int ii = 0; g_ss >> att; ii++){

        }
        logger.log("info", "infra1", "VolumesClass", transactionId, "syncing is done", "ebsvolume_sync");

}
*/

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
 *              given a volumeId, checks if a volume exist or not in the disk file.
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
