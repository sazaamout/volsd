#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>  // used for system() and atoi
#include <stdio.h>   // userd for popen()
#include <algorithm> // used for remove

#include "ClientSocket.h"
#include "SocketException.h"
#include "Volumes.h"
#include "Utils.h"
#include "Logger.h"
#include "Config.h"



// ----------------------------------------------------------------------
// GLOBALS VARAIBLES AND STRUCTURES
// ----------------------------------------------------------------------
  std::string volume; // ? do we need?
  std::string instance_id;
  std::string _conffile;
  
  bool diskAcquireRequest = false;
  bool diskReleaseRequest = false;
  bool _onscreen          = false;
  bool _forceMount        = false;
  int _loglevel           = 3;
  std::string mountPoint, serverIP, serverPort, fsType, device, mountFlags;
  utility::Configuration conf;
// -----------------------------------------------------------------------------
// FUNCTIONS PROTOTYPE
// -----------------------------------------------------------------------------
int  mount_vol(Volumes &volumes, std::string t_volumeId, std::string t_mountPoint, Logger& logger);
bool disk_acquire(Volumes &volumes);
bool disk_release(Volumes &volumes);


void get_arguments( int argc, char **argv );
// -----------------------------------------------------------------------------
// MAIN PROGRAM
// -----------------------------------------------------------------------------
using namespace std;
using namespace utility;

int main ( int argc, char* argv[] )
{

 
  // user must be root
  if (!utility::is_root()){
    std::cout << "error: program must be ran as root\n";
    return 1;
  }
  
  // default value. This is going to be overridden if user specified -c option
  //_conffile = CLIENT_CONF_FILE;
  get_arguments( argc, argv );
    
  // Load the configurations from conf file
  //if ( !utility::load_configurations ( conf, _conffile ) ){
  //  std::cout << "error: cannot locate configuration file\n";
  //  return 1;
  //}
   
  // Collect instance information
  instance_id = utility::get_instance_id(); 
  
  Volumes volumes;
  volumes.set_logger_att ( _onscreen, "/var/log/messages", _loglevel );

  if ( diskAcquireRequest ) {
    if (!disk_acquire(volumes)){
      return 1; // exit with error
    }
  }
  
  if ( diskReleaseRequest ){
    if (!disk_release(volumes)){
      return 1; // exit with error
    }
    
  }
    
  return 0;
}

// -----------------------------------------------------------------------------
// FUNCTIONS
// -----------------------------------------------------------------------------

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DISK_REQUEST FUNCTION
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool disk_acquire(Volumes &volumes) {
    
    Logger logger(_onscreen, "/var/log/messages", 3);
    // ----------------------------------------------
    // 1) Get the port for communications with server
    // ----------------------------------------------
    
    std::string reply;
    try {
      logger.log("info", "", "volsd-client", 0, "requesting communication port from server from " + serverIP + ":" + serverPort);
      ClientSocket client_socket ( serverIP, utility::to_int(serverPort) );
      std::string msg, ack;

      try {
        msg = "DiskRequest";
        client_socket << msg;
        client_socket >> reply;
        logger.log("info", "", "volsd-client", 0, "allocated communication port:[" + reply + "]");
        
      } catch ( SocketException& ) {}
    }
    catch ( SocketException& e ){
      logger.log("error", "", "volsd-client", 0, "Exception was caught:" + e.description());
      return false;
    }

    // ----------------------------------------------
    // 2) Connect to the port and start reciving
    // ----------------------------------------------
    try {
      logger.log("info", "", "volsd-client", 0, "connecting to server " + serverIP + " via communication port:[" + 
                 reply + "]");

      sleep(2);
      ClientSocket client_socket ( serverIP, utility::to_int(reply) );
      
      std::string msg, ack;
      
      try {
        client_socket >> reply;

        logger.log("info", "", "volsd-client", 0, "response from server:[" + reply + "]");
 
        if (reply.compare("MaxDisksReached") == 0) {
          logger.log("error", "", "volsd-client", 0, "Maximum Number of volume reached");
          return false;
        
        } else if (reply.compare("umountFailed") == 0) {
          logger.log("error", "", "volsd-client", 0, "server was unable unmount volume");
          return false;

        } else if (reply.compare("detachFailed") == 0) {
          logger.log("error", "", "volsd-client", 0, "server was unable to detach volume");
          return false;

        } else {
          // At this point, volumesd server have sent us the volumes Id
          int res = mount_vol(volumes, reply, mountPoint, logger);
          // chcek if mount was successfyl. If not, return Failed so disk status can be 
          // set back to idle
          if (!res) {
            logger.log("error", "", "volsd-client", 0, "unable to mount volume, exit");
            ack = "FAILED";
            client_socket << ack;
            return false;
          } 
          // must return the mountpoint so that the server will store it.
          //ack = "OK";
          ack = "OK " + mountPoint;
          
          client_socket << ack;
        }
      } catch ( SocketException& ) {}
    } catch ( SocketException& e ){
      logger.log("error", "", "volsd-client", 0, "Exception was caught:" + e.description());
      return false;
    }
    

    return true;
  }
    
    
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MOUNT_VOL FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int mount_vol( Volumes &volumes, std::string t_volumeId, std::string t_mountPoint, 
                 Logger& logger) {
    
    // NOTE: This function will only work on instances that follows /dev/svdX formate. If you want to 
    // run you volsd server on a bigger instance, you have to fix this function.
    // 2018-04-30
    
    // NOTE: This function is now works with the new format. Atatching a volume must be in /dev/xvdX 
    //       and mounting a volumes can be done with /dev/xcdX as well as /dev/nvmeXn1.
  
  
    // -----------------------------------------------------------------------------------------------   
    // 1) Prepare disk information.
    //------------------------------------------------------------------------------------------------
  
    // get a device. 
    // device must be supplied when running the cleint
    //std::string device = volumes.get_device();
    
    logger.log("info", "", "volsd-client", 0, "allocated device:[" + device + "]");
    
    std::string mountPoint = t_mountPoint;
    if ( mountPoint[mountPoint.length() - 1] != '/' ){
      mountPoint.append("/");
    } 
    
    // -----------------------------------------------------------------   
    // 2) Attach Volume
    // --------------------------------------------------------------- 
    logger.log("info", "", "volsd-client", 0, "attaching volume " + t_volumeId);
    if ( !volumes.attach(t_volumeId, device, instance_id, 0 ) ){
      logger.log("error", "", "volsd-client", 0, 
                 "FAILED to attaching new disk to localhost. Removing  from AWS space");
      
      if (!volumes.del(t_volumeId, 0)){
        logger.log("error", "", "volsd-client", 0, "failed to delete volume from AWS space");
      }
      logger.log("info", "", "volsd-client", 0, "volume was removed from AWS space");
      return 0;
    }
    logger.log("info", "", "volsd-client", 0, "volume was attached successfully on " + device);

    // this sleep is needed since Amazon takes time to attach the volume
    sleep(5);
    
    // -----------------------------------------------------------------   
    // 3) mount Volume
    // --------------------------------------------------------------- 
    logger.log("info", "", "volsd-client", 0, "new volume will be mounted in:[" + mountPoint + "]");
    int retry=0;
    bool mounted = false;
    while (!mounted) {
      if ( !volumes.mount( t_volumeId, mountPoint, device, fsType, mountFlags, 0, _forceMount) ) {
        logger.log("info", "", "volsd-client", 0, "failed to mount new volume. Retry");
      } else {
        logger.log("info", "", "volsd-client", 0, "new volume was mounted successfully");
        mounted = true;
        // add the information of the volume in /etc/volume
        utility::create_file("/etc/volume", t_volumeId + " " + device + " " + mountPoint );
      }
      
      if ((retry == 5) && (!mounted)){
        logger.log("error", "", "volsd-client", 0, 
                   "failed to mount new volume to localhost. Detaching...");

        // detach volumes
        if (!volumes.detach(t_volumeId, 0)){
          logger.log("error", "", "volsd-client", 0, "failed to detach volume.");
          return 0;
        } else {
          sleep(5);
          logger.log("error", "", "volsd-client", 0, 
                     "detaching volume was successfult. Removing from Amazon space");
          // if detach successful, then delete the volume.
          if (!volumes.del(t_volumeId, 0)){
            logger.log("error", "", "volsd-client", 0, "failed to delete volume from Amazon space");
            return 0;
          }
          logger.log("info", "", "volsd-client", 0, "volume was removed from AWS space");
          return 0;
        }
      } 
      
      retry++;
    }

    
    return 1;
  }

  


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DISK_RELEASE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
  bool disk_release(Volumes &volumes) {
    
    Logger logger(_onscreen, "/var/log/messages", 3);
    
    std::string reply;
    
    try {
      
      if (!volumes.release_volume( volume, instance_id, mountPoint, 0 )){
        logger.log("info", "", "volsd-client", 0, "failed to release volume");
        return 0;
      }
      
      logger.log("info", "", "volsd-client", 0, "sends volume release request to server");
      ClientSocket client_socket ( serverIP, utility::to_int(serverPort) );                       
                             
      std::string request = "DiskRelease:" + volume;
    
      try {
        client_socket << request;
        client_socket >> reply;
        logger.log("info", "", "volsd-client", 0, "volumes release request sent");
      } 
      catch ( SocketException& ) {
        return 0;
      }
    }
    catch ( SocketException& e ){
      logger.log("info", "", "volsd-client", 0, "Exception was caught:" + e.description());
      return 0;
    }  
    
    return 1;
    
  }




  // -------------------------------------------------------------------
  // Get_Arguments 
  // -------------------------------------------------------------------
  void get_arguments( int argc, char **argv ) {

    int index;
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "a:r:i:p:m:t:d:fshv")) != -1) 
      switch (c) {   
        case 'h':
          printf("%s: -i 192.168.1.xx -p 9000 -d /dev/xvdf [options]\n", argv[0]);
          printf(" options:\n");
          printf("   -a  aquire a disk\n");
          printf("   -r  release a disk\n");
          printf("   -i  volsd's server IP address\n");
          printf("   -p  volsd's server port\n");
          //printf("   -c  configuration file path if somewhere else\n");
          printf("   -m  mount options\n");
          printf("   -d  device to be used to attach volume\n");
          printf("   -t  filesystem type (ex: ext3, ext4)\n");
          printf("   -f  forcemount: delete any data in mount point before mounting\n");
          printf("   -s  print logs to screen\n");
          printf("   -h  print this help menu\n");
          printf("   -v  version number\n");
          printf("Examples:\n");
          printf("volsd-client -i 10.2.1.150 -p 9000 -m \"MS_NODIRATIME,MS_NOATIME\" -t \"ext4\" -a /home/cde -d xvdf -f\n");
          exit (0);
        case 'v':
          printf("volsd-client version: %i.%i.%i\n", 
                  CLIENT_MAJOR_VERSION, CLIENT_MINOR_VERSION, CLIENT_PATCH_VERSION
          );
          exit (0);

        case 's':
          _onscreen = true;
           break;

        case 'a':
          diskAcquireRequest = true;
          mountPoint = optarg;
          break;

        case 'r':
          diskReleaseRequest = true;
          mountPoint = optarg;
          break;

        case 'i':
          serverIP = optarg;
          break;
        
        case 'p':
          serverPort = optarg;
          break;

        //case 'c':
        //  _conffile = optarg;
        //  break;

        case 'm':
          mountFlags = optarg;
          break;

        case 't':
          fsType = optarg;
          break;
          
       case 'd':
          device = optarg;
          break;

        case 'f':
          _forceMount = true;
          break;

        case '?':
          if ( (optopt == 'a') || (optopt == 'r') || (optopt == 'm') || (optopt == 't') || 
               (optopt == 'd') || (optopt == 'i') || (optopt == 'p') ){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (isprint (optopt)){
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else{
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }
          exit(1);
        default:
          exit(0); 
      }

    for (index = optind; index < argc; index++){
      printf ("Non-option argument %s\n", argv[index]);
      fprintf (stderr, "use -h for help.\n");
      exit(1);
    }

    if ( serverIP.empty() ){
      printf("must specify volsd server IP address. Use -h for more information\n");
      exit(1);
    }
    if ( serverPort.empty() ){
      printf("must specify volsd server port. Use -h for more information\n");
      exit(1);
    }
    if ( device.empty() ){
      printf("must specify device to attach the volime to. Use -h for more information\n");
      exit(1);
    }
    if ( ( ! diskAcquireRequest ) && ( ! diskReleaseRequest ) ) {
      printf("must specify operation. Use -h for more information\n");
      exit(1);
   }

 }
