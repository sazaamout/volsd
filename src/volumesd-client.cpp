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
  std::string  mountPoint;
  std::string  serverIP;
  std::string  serverPort;
  int _loglevel = 3;
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
  _conffile = CLIENT_CONF_FILE;
  get_arguments( argc, argv );
    
  // Load the configurations from conf file
  if ( !utility::load_configurations ( conf, _conffile ) ){
    std::cout << "error: cannot locate configuration file\n";
    return 1;
  }
  
  std::cout << setw(20) << "NFSMountFlags" << "  " << conf.NFSMountFlags << "\n";
  std::cout << setw(20) << "ServerIP" << "  " << conf.ServerIP << "\n";
  std::cout << setw(20) << "ServerPort" << "  " << conf.ServerPort << "\n";
  std::cout << setw(20) << "LogLevel" << "  " << conf.LogLevel << "\n";
  std::cout << setw(20) << "LogFile" << "  " << conf.LogFile << "\n";
  std::cout << setw(20) << "TargetFSMountPoint" << "  " << conf.TargetFSMountPoint << "\n";
  std::cout << setw(20) << "TargetFSDevice" << "  " << conf.TargetFSDevice << "\n";
  std::cout << setw(20) << "ForceMount" << "  " << conf.ForceMount << "\n";
  std::cout << setw(20) << "Aws_Cmd" << "  " << conf.Aws_Cmd << "\n";
  std::cout << setw(20) << "Aws_Region" << "  " << conf.Aws_Region << "\n";
  std::cout << setw(20) << "Aws_ConfigFile" << "  " << conf.Aws_ConfigFile << "\n";
  std::cout << setw(20) << "Aws_CredentialsFile" << "  " << conf.Aws_CredentialsFile << "\n";
  
  return 0;  
  
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
      logger.log("error", "", "volsd-client", 0, 
                 "FAILED to attaching new disk to localhost. Removing  from AWS space");
      
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
      if ( !volumes.mount( t_volumeId, mountPoint, device, 0 ) ) {
        logger.log("info", "", "volsd-client", 0, "failed to mount new volume. Retry");
      } else {
        logger.log("info", "", "volsd-client", 0, "new volume was mounted successfully");
        mounted = true;
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

    while ((c = getopt (argc, argv, "a:r:c:i:p:shv")) != -1) 
      switch (c) {   
        case 'h':
          printf("%s: -i 192.168.1.xx -p 9000 [options]\n", argv[0]);
          printf(" options:\n");
          printf("   -a  aquire a disk\n");
          printf("   -r  release a disk\n");
          printf("   -i  volsd's server IP address\n");
          printf("   -p  volsd's server port\n");
          printf("   -c  configuration file path if somewhere else\n");
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

        case 'i':
          serverIP = optarg;
          break;
        
        case 'p':
          serverPort = optarg;
          break;
        case 'c':
          _conffile = optarg;
          break;
        case '?':
          if (optopt == 'a'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (optopt == 'r'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
         }else if (optopt == 'c'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (optopt == 'i'){
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            fprintf (stderr, "use -h for help.\n");
          }else if (optopt == 'p'){
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
    if ( ( ! diskAcquireRequest ) && ( ! diskReleaseRequest ) ) {
      printf("must specify operation. Use -h for more information\n");
      exit(1);
   }

 }
