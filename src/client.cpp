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


// ----------------------------------------------------------------------
// GLOBALS VARAIBLES AND STRUCTURES
// ----------------------------------------------------------------------
#define INSTANCE_VIRT_TYPE  HVM;
#define aws_region  "us_east_1";
std::string volume; // ? do we need?
std::string instance_id;

// -----------------------------------------------------------------------------
// FUNCTIONS PROTOTYPE
// -----------------------------------------------------------------------------
int  mount_vol(std::string volume, std::string mountPoint, Logger& logger);
bool disk_request(std::string mountPoint);
void disk_release(std::string mountPoint);
void disk_push(std::string mountPoint);

// -----------------------------------------------------------------------------
// MAIN PROGRAM
// -----------------------------------------------------------------------------
using namespace std;
using namespace utility;

int main ( int argc, char* argv[] )
{
  if (!utility::is_root()){
    std::cout << "user is not root\n";
    return 1;
  }
  std::string volume, output;
    
  std::string request = argv[1];
  std::string mountPoint = argv[2];
  // Collect instance information
  instance_id = utility::get_instance_id(); 
      
  if (request.compare("DiskRequest") == 0) {
    if (!disk_request(mountPoint)){
      return 1; // exit with error
    }
  } else if ( request.compare("DiskRelease") == 0){
    disk_release(mountPoint);
  } else {
    std::cout << "unknown request\n";
    return 0;
  }
    
  return 0;
}

// -----------------------------------------------------------------------------
// FUNCTIONS
// -----------------------------------------------------------------------------

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DISK_REQUEST FUNCTION
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bool disk_request(std::string mountPoint) {
    
    Logger logger(true, "/var/log/messages", 3);
    // ----------------------------------------------
    // 1) Get the port for communications with server
    // ----------------------------------------------
  
    std::string reply;
    try {
      std::cout << "Connecting to Port 90000\n"; 
      logger.log("info", "Unknown", "EBSCLient", 0, "connecting to port:[9000]", "Disk_request");
      ClientSocket client_socket ( "10.2.1.30", 9000 );
      std::string msg, ack;

      try {
        msg = "DiskRequest";
        client_socket << msg;
        client_socket >> reply;
        logger.log("info", "Unknown", "EBSCLient", 0, "response from the server:[" + reply + "]", "Disk_request");
        
      } catch ( SocketException& ) {}
    }
    catch ( SocketException& e ){
      logger.log("error", "Unknown", "EBSCLient", 0, "Exception was caught:" + e.description(), "Disk_request");
      return false;
    }

    // ----------------------------------------------
    // 2) Connect to the port and start reciving
    // ----------------------------------------------
    try {
      logger.log("info", "Unknown", "EBSCLient", 0, "Connecting to Port:[" + reply + "]", "Disk_request");

      sleep(2);
      ClientSocket client_socket ( "10.2.1.30", utility::to_int(reply) );
      
      std::string msg, ack;
      
      try {
        client_socket >> reply;

        logger.log("info", "Unknown", "EBSCLient", 0, "response from server:[" + reply + "]", "Disk_request");
 
        if (reply.compare("MaxDisksReached") == 0) {
          logger.log("error", "Unknown", "EBSCLient", 0, "Maimum Number of volume reached", "Disk_request");
          return false;
        
        }else if (reply.compare("umountFailed") == 0) {
          logger.log("error", "Unknown", "EBSCLient", 0, "server was unable unmount volume", "Disk_request");
          return false;

        }else if (reply.compare("detachFailed") == 0) {
          logger.log("error", "Unknown", "EBSCLient", 0, "server was unable to detach volume", "Disk_request");
          return false;

        }else {
          int res = mount_vol(reply, mountPoint, logger);
          // chcek if mount was successfyl. If not, return Failed so disk status can be 
          // set back to idle
          if (!res) {
            logger.log("error", "Unknown", "EBSCLient", 0, "unable to mount volume, exit", "Disk_request");
            ack = "FAILED";
            client_socket << ack;
            return false;
          } 
          ack = "OK";
          client_socket << ack;
        }
      } catch ( SocketException& ) {}
    } catch ( SocketException& e ){
      logger.log("error", "Unknown", "EBSCLient", 0, "Exception was caught:" + e.description(), "Disk_request");
      return false;
    }
    
    //logger.flush();
    return true;
  }
    
    
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // MOUNT_VOL FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  int mount_vol(std::string volume, std::string mountPoint, Logger& logger) {
  
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
        
    return 1;
  }

  


  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DISK_RELEASE FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
  void disk_release(std::string mountPoint) {
    
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
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // DISK_PUSH FUNCTION
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  void disk_push(std::string mountPoint){
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

  }
