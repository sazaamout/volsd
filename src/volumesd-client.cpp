#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h> // used for system() and atoi
#include <stdio.h> // userd for popen()
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
//#define INSTANCE_VIRT_TYPE  HVM;
//#define aws_region  "us_east_1";
std::string volume; // ? do we need?
std::string instance_id;

bool diskAcquireRequest = false;
bool diskReleaseRequest = false;
bool _onscreen          = false;
std::string  mountPoint;
int _loglevel = 3;

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
  get_arguments( argc, argv );
  
  // Collect instance information
  instance_id = utility::get_instance_id(); 
  
  Volumes volumes;
  volumes.set_logger_att ( _onscreen, "/var/log/messages", _loglevel );

  if ( diskAcquireRequest ) {
    std::cout << "diskAcquire\n";

    if (!disk_acquire(volumes)){
      return 1; // exit with error
    }
  }
  
  if ( diskReleaseRequest ){
    std::cout << "diskRelease\n";
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
    std::cout << "4\n";
    std::string reply;
    try {
      std::cout << "Connecting to Port 90000\n"; 
      logger.log("info", "", "volsd-client", 0, "requesting communication port from server.");
      ClientSocket client_socket ( "10.2.1.147", 9000 );
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
      logger.log("info", "", "volsd-client", 0, "connecting to server via communication port:[" + reply + "]");

      sleep(2);
      ClientSocket client_socket ( "10.2.1.147", utility::to_int(reply) );
      
      std::string msg, ack;
      
      try {
        client_socket >> reply;

        logger.log("info", "", "volsd-client", 0, "response from server:[" + reply + "]");
 
        if (reply.compare("MaxDisksReached") == 0) {
          logger.log("error", "", "volsd-client", 0, "Maimum Number of volume reached");
          return false;
        
        }else if (reply.compare("umountFailed") == 0) {
          logger.log("error", "", "volsd-client", 0, "server was unable unmount volume");
          return false;

        }else if (reply.compare("detachFailed") == 0) {
          logger.log("error", "", "volsd-client", 0, "server was unable to detach volume");
          return false;

        }else {
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
          ack = "OK";
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
  int mount_vol(Volumes &volumes, std::string t_volumeId, std::string t_mountPoint, Logger& logger) {
    
    // -----------------------------------------------------------------------------------------------   
    // 1) Prepare disk information.
    //------------------------------------------------------------------------------------------------
  
    // get a device. 
    std::string device = volumes.get_device();
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
      logger.log("error", "", "volsd-client", 0, "FAILED to attaching new disk to localhost. Removing  from AWS space");
      
      if (!volumes.del(t_volumeId, 0)){
        logger.log("error", "", "volsd-client", 0, "failed to delete volume from AWS space");
      }
      logger.log("info", "", "volsd-client", 0, "volume was removed from AWS space");
      return 0;
    }
    logger.log("info", "", "volsd-client", 0, "volume was attached successfully");

    // this sleep is needed since Amazon takes time to attach the volume
    sleep(5);
    
    // -----------------------------------------------------------------   
    // 3) mount Volume
    // --------------------------------------------------------------- 
    logger.log("info", "", "volsd-client", 0, "new volume will be mounted in:[" + mountPoint + "]");
    int retry=0;
    bool mounted = false;
    while (!mounted) {
      if ( !volumes.mount(t_volumeId, mountPoint, device, 0) ) {
        logger.log("info", "", "volsd-client", 0, "failed to mount new volume. Retry");
      } else {
        logger.log("info", "", "volsd-client", 0, "new volume was mounted successfully");
        mounted = true;
      }
      
      if ((retry == 5) && (!mounted)){
        logger.log("error", "", "volsd-client", 0, "failed to mount new volume to localhost. Detaching...");

        // detach volumes
        if (!volumes.detach(t_volumeId, 0)){
          logger.log("error", "", "volsd-client", 0, "failed to detach volume.");
          return 0;
        } else {
          sleep(5);
          logger.log("error", "", "volsd-client", 0, "detaching volume was successfult. Removing from Amazon space");
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
      logger.log("info", "", "volsd-client", 0, "connecting to port:[" + reply + "]");
      ClientSocket client_socket ( "10.2.1.147", 9000 );
      
      if (!volumes.release_volume( volume, instance_id, mountPoint, 0 )){
        logger.log("info", "", "volsd-client", 0, "failed to release volume");
        return 0;
      }
      
      
                             
                             
      std::string request = "DiskRelease:" + volume;
    
      try {
      client_socket << request;
      client_socket >> reply;
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

    while ((c = getopt (argc, argv, "a:r:shv")) != -1) 
      switch (c) {   
        case 'h':
          printf("%s: [options]\n", argv[0]);
          printf(" options:\n");
          printf("   -a  aquire a disk\n");
          printf("   -r  release a disk\n");
          printf("   -s  print logs to screen\n");
          printf("   -h  print this help menu\n");
          printf("   -v  version number\n");
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
        case '?':
          if (optopt == 'a'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (optopt == 'r'){
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
  }
