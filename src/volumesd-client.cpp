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
// -----------------------------------------------------------------------------
// FUNCTIONS PROTOTYPE
// -----------------------------------------------------------------------------
int  mount_vol(std::string volume, std::string mountPoint, Logger& logger);
bool disk_acquire();
void disk_release(std::string mountPoint);
void disk_push(std::string mountPoint);

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
    
  std::string volume, output;
  
  // Collect instance information
  instance_id = utility::get_instance_id(); 
      
  if ( diskAcquireRequest ) {
    std::cout << "diskAcquire\n";
    if (!disk_acquire()){
      return 1; // exit with error
    }
  }
  
  if ( diskReleaseRequest ){
    std::cout << "diskRelease\n";
    //disk_release(mountPoint);
  }
    
  return 0;
}

// -----------------------------------------------------------------------------
// FUNCTIONS
// -----------------------------------------------------------------------------

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DISK_REQUEST FUNCTION
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool disk_acquire() {
    
    Logger logger(_onscreen, "/var/log/messages", 3);
    // ----------------------------------------------
    // 1) Get the port for communications with server
    // ----------------------------------------------
  
    std::string reply;
    try {
      std::cout << "Connecting to Port 90000\n"; 
      logger.log("info", "", "volumesd-client", 0, "connecting to port:[9000]");
      ClientSocket client_socket ( "10.2.1.30", 9000 );
      std::string msg, ack;

      try {
        msg = "DiskRequest";
        client_socket << msg;
        client_socket >> reply;
        logger.log("info", "", "volumesd-client", 0, "response from the server:[" + reply + "]");
        
      } catch ( SocketException& ) {}
    }
    catch ( SocketException& e ){
      logger.log("error", "", "volumesd-client", 0, "Exception was caught:" + e.description());
      return false;
    }

    // ----------------------------------------------
    // 2) Connect to the port and start reciving
    // ----------------------------------------------
    try {
      logger.log("info", "", "volumesd-client", 0, "Connecting to Port:[" + reply + "]");

      sleep(2);
      ClientSocket client_socket ( "10.2.1.30", utility::to_int(reply) );
      
      std::string msg, ack;
      
      try {
        client_socket >> reply;

        logger.log("info", "", "volumesd-client", 0, "response from server:[" + reply + "]");
 
        if (reply.compare("MaxDisksReached") == 0) {
          logger.log("error", "", "volumesd-client", 0, "Maimum Number of volume reached");
          return false;
        
        }else if (reply.compare("umountFailed") == 0) {
          logger.log("error", "", "volumesd-client", 0, "server was unable unmount volume");
          return false;

        }else if (reply.compare("detachFailed") == 0) {
          logger.log("error", "", "volumesd-client", 0, "server was unable to detach volume");
          return false;

        }else {
          int res = mount_vol(reply, mountPoint, logger);
          // chcek if mount was successfyl. If not, return Failed so disk status can be 
          // set back to idle
          if (!res) {
            logger.log("error", "", "volumesd-client", 0, "unable to mount volume, exit");
            ack = "FAILED";
            client_socket << ack;
            return false;
          } 
          ack = "OK";
          client_socket << ack;
        }
      } catch ( SocketException& ) {}
    } catch ( SocketException& e ){
      logger.log("error", "", "volumesd-client", 0, "Exception was caught:" + e.description());
      return false;
    }
    
    //logger.flush();
    return true;
  }
    
    
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MOUNT_VOL FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int mount_vol(std::string volume, std::string mountPoint, Logger& logger) {
    /* REMOVE TO BE ABLE TO START FROM SCRATCH
    std::string output;
        
    // TODO: fix this. passing the path here does not have effect since we are going to use a function that does not need that path
    // that fynction take v startuct 
    Volumes dc(true, "asdfasdf", "asdfasdf");
        
    int transaction = utility::get_transaction_id();
        
    utility::Volume v;
        
    v.id = volume;
        
    // check if mount point have "/"
    if ( mountPoint[mountPoint.length()-1] != '/' ) {
      mountPoint.append("/");
    }
    v.mountPoint = mountPoint;
    v.device = utility::get_device(); 
        
    // really there is not need for these varaibles
    v.status = "inprogress";
    v.attachedTo = "localhost";
                
    if (!dc.ebsvolume_attach(v, instance_id, transaction, logger))
      return 0;
      
    sleep(5);
    
    if (!dc.ebsvolume_mount(v, instance_id, transaction, logger)){
      dc.ebsvolume_detach(v, transaction, logger);
      dc.ebsvolume_delete(v, transaction, logger);
      return 0;
    }
        */
    return 1;
  }

  


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DISK_RELEASE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
  void disk_release(std::string mountPoint) {
    /* REMOVE TO BE ABLE TO START FROM SCRATCH
    Logger logger(true, "/var/log/messages", 3);
    
    std::string reply;
    Volumes dc(true);
    try {
      logger.log("info", "Unknown", "EBSCLient", 0, "connecting to port:[" + reply + "]", "Disk_release");
      ClientSocket client_socket ( "10.2.1.30", 9000 );
      int res = dc.release_volume(volume, instance_id, mountPoint, true, 0, logger);
        
      std::string request = "DiskRelease:" + volume;
    
      try {
      client_socket << request;
      client_socket >> reply;
      } 
      catch ( SocketException& ) {
        // retry
      }
    }
    catch ( SocketException& e ){
      logger.log("info", "Unknown", "EBSCLient", 0, "Exception was caught:" + e.description(), "Disk_release");
      return;
    }  
    
    //logger.flush();
    * */
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DISK_PUSH FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  void disk_push(std::string mountPoint){
   /* REMOVE TO BE ABLE TO START FROM SCRATCH
    std::cout <<"Disk_PUSH\n";
    std::string reply;
    Volumes dc(true);
    
    try {
      std::cout << "Connecting to Port " << reply << "\n";
      ClientSocket client_socket ( "10.2.1.30", 9000 );
      
      std::string request = "pushRequest" + volume;
    
      try {
      client_socket << request;
      client_socket >> reply;
      } 
      catch ( SocketException& ) {
        // retry
      }
    }
    catch ( SocketException& e ){
      std::cout << "Exception was caught:" << e.description() << "\n";
      return;
    }  
*/
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
          printf("volumesd-client version: %i.%i.%i\n", 
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
