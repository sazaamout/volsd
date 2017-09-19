
#include "SocketException.h"
#include "Disks.h"
#include "ServerSocket.h"
#include "Utils.h"
#include "Logger.h"
#include "Config.h"

#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <fstream>
#include <mutex>

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 GLOBALS VARAIBLES AND STRUCTURES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ 
	utility::Port *portsArray;
	utility::Configuration conf;
	
	std::string hostname; 
	std::string instance_id;
	
	std::mutex m;
	
	std::vector<std::string>  devices_list;
	
	bool _onscreen = false;
	std::string _conffile;
	int _loglevel;

	
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	void disk_request_task(int portNo, std::string request, std::string ip, Disks& d, int transId);
	void disk_release_task(Disks &diskcontroller, std::string volId, int transId);
	void debug_task(int portNo, Disks& d);
	void EBSVolumeManager_task();
	void EBSVolumeSync_task();
	void populate_port_array();
	void print_ports();

	std::string get_ports();
	int get_available_port();
	int disk_prepare(utility::Volume& volume, Disks& dc, int transactionId, Logger& logger);
	//int parse_arguments(int argc, char* argv[]);
        void get_arguments( int argc, char **argv );

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 MAIN PROGRAM
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
using namespace std;
using namespace utility;
	
int main ( int argc, char* argv[] ) 
{
		
	std::string request, ack, cip;
	std::string msg;
	int transId;


        // this function will overwite the _conffile varibles if user specified one, otherwise, it will use the default.
        _conffile = DISPATCHER_CONF_FILE;
        get_arguments( argc, argv );

        if (!utility::is_root()){
          std:: cout << "program must be ran as root\n";
          return 1;
        }

	std:: cout << "program starts...\n";
        return 1;

	//if (!parse_arguments(argc, argv)){
	//  return 1;
	//}
	
	
	// load the configurations 
	utility::load_configuration(conf, _conffile);
	_loglevel = conf.ControllerLoglevel;
	//utility::print_configuration(conf);

	
	Logger logger(_onscreen, conf.ControllerLogFile, _loglevel);
	
	hostname    = utility::get_hostname();
	instance_id = utility::get_instance_id();

	Disks diskcontroller(true, conf.TempMountPoint, conf.VolumeFilePath);

	// Populating ports array
	portsArray = new Port[10];
	populate_port_array();
	//print_ports();

	// start the debug thread
	logger.log("info", hostname, "EBSController", 0, "starting the debug thread");
	thread debug_thread(debug_task, 8000, std::ref(diskcontroller));
	debug_thread.detach();
  
	try {
		// Create the socket
		ServerSocket server ( 9000 );

		while ( true ) {

			transId = utility::get_transaction_id();
			logger.log("debug", hostname, "EBSController", 0, "waiting for requests from clients ...");
			ServerSocket new_sock;
			// here, make a new threads. Every new clinet needs a threds. 
			server.accept ( new_sock );
		  
			cip = server.client_ip();

			logger.log("info", hostname, "EBSController", transId, "incoming connection accepted from " + cip);
			try {
				while ( true ) {
					new_sock >> request;
					utility::clean_string(request);

					if ( request.compare("DiskRequest") == 0 ){
						logger.log("info", hostname, "EBSController", transId, "request:[" + request + "] from client:[" + cip + "]");
						int availablePort = get_available_port();
						new_sock << utility::to_string(availablePort);

						logger.log("debug", hostname, "EBSController", transId, "reserving port:[" + utility::to_string(availablePort) + "] to client:[" + cip + "]");
					
						new_sock.close_socket();

						thread t1(disk_request_task, availablePort, request, cip, std::ref(diskcontroller), transId);
						t1.detach();
						break;

					}else if ( request.find("DiskRelease") != std::string::npos ) {
						// Message Format:  DiskRelease:volId
						logger.log("info", hostname, "EBSController", transId, "request:[" + request + "] from client:[" + cip + "]");
						std::string volId = request.substr( request.find(":")+1, request.find("\n") );	
					
						if ( (volId == "") || (volId == " ") ) {
							//utility::logger("info", hostname,"", "server", transId, "" );
							logger.log("error", hostname, "EBSController", transId, "client did not supply volume id to release");
							new_sock.close_socket();
							break;
						}
					
						thread disk_release_thread(disk_release_task, std::ref(diskcontroller), volId, transId);
						disk_release_thread.detach();
					
						new_sock.close_socket();
						break;
				
					}else if ( request.find("pushRequest") != std::string::npos ) {
						logger.log("info", hostname, "EBSController", transId, "request:[" + request + "] from client:[" + cip + "]");
						//extract push path
						sleep(60);
						msg = "request recived";
						new_sock << msg;
					
					}else {
						
						msg = "unknown request, shutting down connection\n";
						logger.log("error", hostname, "EBSController", transId, "unknown request:[" + request + "], shutting down connection");
						new_sock << msg;
						break;
					}
				} // end while
			} // end try
			catch ( SocketException& e) {
				logger.log("error", hostname, "EBSController", transId, "Exception was caught: " + e.description());
			}
		} // end of outer while
	} // end outer try
	catch ( SocketException& e ) {
		logger.log("error", hostname, "EBSController", transId, "Exception was caught: " + e.description());
	}

	return 0;
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 FUNCTION PROTOTYPES
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	/* -----------------------------------------------------------------
       DISK_REQUEST_TASK
	----------------------------------------------------------------- */
	void disk_request_task(int portNo, std::string request, std::string ip, Disks& dc, int transId) 
	{
		Logger logger(_onscreen, conf.ControllerLogFile, _loglevel);
		
		logger.log("debug", hostname, "EBSController", transId, "awaiting clinet to connect tp port:[" + utility::to_string(portNo) + "]", "DiskRequestThread");
		
		//int transaction = utility::get_transaction_id();   
		//int transaction = utility::get_transaction_id();   
		
		utility::Volume v;
		std::string msg, ack;
		
		ServerSocket server ( portNo );
		
		ServerSocket s;
		server.accept ( s );
		
		int res = disk_prepare(v, dc, transId, logger);
		if (res == -1) {
			s << "MaxDisksReached";
			portsArray[portNo-9000-1].status=false;
			return;
		}
			
		if (res == -2){
			s << "umountFailed";
			portsArray[portNo-9000-1].status=false;
			return;
		}
			
		if (res == -3){
			s << "detachFailed";
			portsArray[portNo-9000-1].status=false;
			return;
		}
		
		logger.log("info", hostname, "EBSController", transId, "sending to client volume:[" + v.id + "]", "DiskRequestThread");
		
		try {
		  s << v.id;

		  logger.log("info", hostname, "EBSController", transId, "waiting for ACK from Client", "DiskRequestThread");
		  s >> ack;
		  
		} catch ( SocketException& e) {
		  ack = "FAILED";
		  logger.log("error", hostname, "EBSController", transId, "connection closed: " + e.description(), "DiskRequestThread");		  
		  portsArray[portNo-9000-1].status=false;
		  return;
		}

		if (ack.compare("OK") == 0){
		  logger.log("info", hostname, "EBSController", transId, "ACK recived from client: [OK]" , "DiskRequestThread");		  

		  //label disk as used
		  logger.log("info", hostname, "EBSController", transId, "set volume's status to 'used' by:[" + ip +"]" , "DiskRequestThread");		  
		  
		  m.lock();
			int res = dc.ebsvolume_setstatus( "update", v.id, "used", ip, "/home/cde", "none", transId, logger );
		  m.unlock();
		  
		  if (!res) {
			m.lock();
				dc.ebsvolume_setstatus( "update", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger );
			m.unlock();
			logger.log("error", hostname, "EBSController", transId, "ACK [OK] was recived but failed to write to volume file" , "DiskRequestThread");		  
			logger.log("error", hostname, "EBSController", transId, "aborting" , "DiskRequestThread");		  
		  }else {
			  logger.log("info", hostname, "EBSController", transId, "disk:[" + v.id + "] is mounted on client machine" , "DiskRequestThread");		  
		  }
		  
		}else {
		 /*  Ideally, we want to put disk back, but for now, just delete the disk and the EBSmanager will create another one.
		  */
		  logger.log("info", hostname, "EBSController", transId, "ACK from clinent was [" + ack + "]" , "DiskRequestThread");	
		  logger.log("info", hostname, "EBSController", transId, "delete volume from volumes list" , "DiskRequestThread");	
		  m.lock();
			dc.ebsvolume_setstatus( "delete", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger);
		  m.unlock();
		  		  
		  logger.log("info", hostname, "EBSController", transId, "aborting ..." , "DiskRequestThread");		  
		  
		  portsArray[portNo-9000-1].status=false;
		  return;
		}
		// labling port as not used
		portsArray[portNo-9000-1].status=false;
		
		//logger.flush();
	}


	/* -----------------------------------------------------------------
	   DISK_PREPARE
	----------------------------------------------------------------- */
	int disk_prepare(utility::Volume& v, Disks& dc, int transactionId, Logger& logger)
	{
		std::string volId;
		logger.log("info", hostname, "EBSController", transactionId, "get idle volume from volumes file", "DiskRequestThread");
		if (!dc.ebsvolume_idle(v, transactionId, logger)) {
			logger.log("error", hostname, "EBSController", transactionId, "no more idle volume available", "DiskRequestThread");
			return -1;

		} else {
			logger.log("info", hostname, "EBSController", transactionId, "idle volume found:[" + v.id + "]", "DiskRequestThread");
			
			logger.log("info", hostname, "EBSController", transactionId, "set new volume's status to 'inprogress'", "DiskRequestThread");
			m.lock();
				if (!dc.ebsvolume_setstatus( "update", v.id, "inprogress", "none", "none", "none", transactionId, logger)){
					logger.log("error", hostname, "EBSController", transactionId, "failed to update disk status", "DiskRequestThread");
				}
			m.unlock();
			
			logger.log("info", hostname, "EBSController", transactionId, "umounting volume", "DiskRequestThread");
			// unmount disk and send it to client
			if (!dc.ebsvolume_umount(v, transactionId, logger)) {
				logger.log("error", hostname, "EBSController", transactionId, "failed to umount volume", "DiskRequestThread");
				logger.log("info", hostname, "EBSController", transactionId, "remove volume from volume list", "DiskRequestThread");
				m.lock();
					if (!dc.ebsvolume_setstatus( "delete", v.id, "", "", "", "", transactionId, logger))
						logger.log("error", hostname, "EBSController", transactionId, "failed to update disk status", "DiskRequestThread");
				m.unlock();
				return -2;
			}
			logger.log("info", hostname, "EBSController", transactionId, "volume umounted", "DiskRequestThread");
			
			
			logger.log("info", hostname, "EBSController", transactionId, "detaching volume", "DiskRequestThread");
			if (!dc.ebsvolume_detach(v, transactionId, logger)) {
				logger.log("error", hostname, "EBSController", transactionId, "failed to detach volume", "DiskRequestThread");
				return -3;
			}
			logger.log("info", hostname, "EBSController", transactionId, "volume was detached", "DiskRequestThread");
			
			logger.log("info", hostname, "EBSController", transactionId, "remove mountpoint:[" + v.mountPoint +"]", "DiskRequestThread");
			if (!dc.remove_mountpoint(v.mountPoint, transactionId, logger)){
				logger.log("error", hostname, "EBSController", transactionId, "could not remove mountpoint", "DiskRequestThread");
			}
			logger.log("info", hostname, "EBSController", transactionId, "mount point was removed", "DiskRequestThread");
		}

		sleep(5);

		return 1;
	}
 
	
	/* -----------------------------------------------------------------
	   DISK_RELEASE_TASK
	----------------------------------------------------------------- */
	void disk_release_task(Disks &dc, std::string volId, int transId){
		
		Logger logger(_onscreen, conf.ControllerLogFile, _loglevel);
		
		utility::Volume v;
		v.id = volId;
		
		//check if vol exist
		logger.log("info", hostname, "EBSController", transId, "checks if volume:[" + volId + "] exist in disk file", "DiskReleaseThread");
		if (!dc.ebsvolume_exist(v.id) || (v.id == "") || (v.id == " ") ){
			logger.log("info", hostname, "EBSController", transId, "volume was not found in diskfile", "DiskReleaseThread");
			return;
		}
		logger.log("info", hostname, "EBSController", transId, "volume was found.", "DiskReleaseThread");
		
		logger.log("info", hostname, "EBSController", transId, "removing volume:[" + v.id + "] from disk file", "DiskReleaseThread");
		m.lock();
			dc.ebsvolume_setstatus( "delete", v.id, "idle", v.attachedTo, v.mountPoint, v.device, transId, logger);
		m.unlock();
        
        logger.log("info", hostname, "EBSController", transId, "deleting volume:[" + v.id + "]", "DiskReleaseThread");
        if (!dc.ebsvolume_delete(v, transId, logger)){
			logger.log("error", hostname, "EBSController", transId, "failed to delete volume:[" + v.id + "] from amazon site", "DiskReleaseThread");
			return;
		}
		logger.log("info", hostname, "EBSController", transId, "volume:[" + v.id + "] was deleted", "DiskReleaseThread");
        //logger.flush();    
	}


	/* -----------------------------------------------------------------
       POPULATE_PORT_ARRAY
	----------------------------------------------------------------- */
	void populate_port_array() {
		for (int i=0; i<10; i++) {
			portsArray[i].portNo = 9000 + 1 + i;
			portsArray[i].status = false;
		}
	}


	/* -----------------------------------------------------------------
       GET_AVAILABLE_PORT
	----------------------------------------------------------------- */
	int get_available_port() {
		for (int i=0; i<10; i++) {
			if (portsArray[i].status == false){
				portsArray[i].status = true;
				return portsArray[i].portNo;
			}
		}
		return -1;
	}


	/* -----------------------------------------------------------------
       PRINT_PORTS
	----------------------------------------------------------------- */
	void print_ports() {
		std::cout << " | ";
		for (int i=0; i<10; i++) {
			std::cout << portsArray[i].portNo << "-" << portsArray[i].status << " | ";
		}
		std::cout << "\n";
	}


	/* -----------------------------------------------------------------
       GET_PORTS
	----------------------------------------------------------------- */
	std::string get_ports() {
		std::string str;
		stringstream ss1, ss2;
		for (int i=0; i<10; i++) {
			ss1 << portsArray[i].portNo;
			ss2 << portsArray[i].status;
			str = str + "[" + ss1.str()  + ":" + ss2.str() + "]\n";
			ss1.str(""); ss2.str("");
		}
		return str;
	}


	/* -----------------------------------------------------------------
       DEBUG_TASK
	----------------------------------------------------------------- */
	void debug_task(int portNo, Disks& dc) {
	  
		Logger logger(_onscreen, conf.ControllerLogFile, _loglevel);
		  
		std::string request, ack, reply;
		ServerSocket debugThread ( portNo );

		while ( true ) {
		  ServerSocket new_sock;
		  // here, make a new threads. Every new clinet needs a threds. 
		  debugThread.accept ( new_sock );
		  
		  try {
			while ( true ) {
			  try {
				new_sock  >> request;
				utility::clean_string(request);
				
				logger.log("info", hostname, "EBSController", 0, "Request recived [" + request + "]", "Debug");
				
				if ( request.compare("portlist") == 0 ) {
				  //std::cout << "  DEBUG: portlist request Recived" << std::endl;
				  reply = get_ports();
				  new_sock << reply;
				
				}else if ( ( request.compare("help") == 0 ) || 
												  ( request.compare("h") == 0 ) ) {
				  	//std::cout << "  DEBUG help request Recived" << std::endl;
				  	reply = "Commands List:\nvollist\nvolsetid\nportlist\ebssync\nebspush\nexit\n";
				  	new_sock << reply;
					
					//new_sock.close_socket();
					//break;				
				}else if ( request.compare("vollist") == 0 ) {
					// TODO
					reply = dc.ebsvolume_list("all", conf.VolumeFilePath);
					new_sock << reply;
				
					//new_sock.close_socket();
					//break;	
				} else if ( request.find("snapshot_create") != std::string::npos ) {
					// TODO
					// this will create a snapshot from the target point and set it to latest. 
					
					std::string volId = request.substr( request.find(" ")+1, request.find("\n") );
					//new_sock.close_socket();
					//break;
				} else if ( request.compare("volsetidle") == 0 ) {
				  	std::cout << "debug" << std::endl;
					reply = "volume ID:"; 
					new_sock << reply;
				  
					new_sock >> reply;
					utility::clean_string(reply);
								
					dc.ebsvolume_setstatus("update", reply, "idle", "none", "none", "none", 0, logger);
					reply = "Volume [" + reply + "] status was chagned\n";
					new_sock << reply;

				  	//new_sock.close_socket();
					//break;	 
				}else if (( request.compare("exit") == 0 ) || ( request.compare("quit") == 0 )) {
				  //std::cout << "  DEBUG Client request to Exit\n\n";
				  new_sock.close_socket();
				  break;

				} else if ( request.compare("listRemoteServers") == 0 ) {
					reply = "";
					std::string remote = dc.ebsvolume_list("all", conf.VolumeFilePath);
					
					std::istringstream iss(remote);

					for (std::string line; std::getline(iss, line); )
					{
						if (line.find("used") != std::string::npos ){
							std::stringstream ss(line);
							std::string a,b,c;
							ss >> a >> b >> c;
							reply += c + "\n";
						}
						
					}
					new_sock << reply;

					//new_sock.close_socket();
					//break;	 

				}else {
				  
				  reply = "unknown request, type help for list of commands\n\n";
				  new_sock << reply;
				  //break;
				}
			  } catch ( SocketException& e) {
				ack = "FAILED";
				//std::cout << "  DEBUG: Connection Closed: " << e.description() << "\n\n";
				logger.log("error", hostname, "EBSController", 0, "nkown error, connection Closed", "Debug");
				new_sock.close_socket();
				break;
			  } // ~~~ end of inner try
			} // ~~~ end of inner while
		  } // ~~~end of outer try
		  catch ( SocketException& e) {
		  }
		} // ~~~ end of outer while
	} // ~~~ end of function

	
	/* -----------------------------------------------------------------
       PARSE_ARGUMENTS
	----------------------------------------------------------------- */
/*	int parse_arguments(int argc, char* argv[]){

		std::string str;
		for (int i=0; i<argc; i++){
			str = argv[i];
			if ( (str == "--version") || (str == "-v") ){
                          std::cout << "Dispatcher version: " << DISPATCHER_MAJOR_VERSION << '.' 
                                                              << DISPATCHER_MINOR_VERSION << '.' 
                                                              << DISPATCHER_PATCH_VERSION << std::endl;
                          return false;
                        }

			if (str == "--screen")
				_onscreen = true;
				
			if ( str.find("--conf") != std::string::npos )
				_conffile = str.substr(str.find("=")+1, str.find("\n"));
		}
		if (_conffile == ""){
                  std::cout << "cannot locate conf file\n";
		  return false;
                }
			
		return true;
	}

*/
       void get_arguments( int argc, char **argv ){
          int index;
          int c;

          opterr = 0;

          while ((c = getopt (argc, argv, "hvsc:")) != -1) 
            switch (c) {   
              case 'h':
                printf("%s: [options]\n", argv[0]);
                printf(" options:\n");
                printf("   -v   version number\n");
                printf("   -c   config file path\n");
                printf("   -s   dump stdout and stderr to the screen\n");
                printf("   -h   print this help menu\n");
                exit (0);
              case 'v':
                printf("Dispatcher version: %i.%i.%i\n", 
                      DISPATCHER_MAJOR_VERSION, DISPATCHER_MINOR_VERSION, DISPATCHER_PATCH_VERSION);
                exit (0);
              case 's':
                _onscreen = 1;
                break;
              case 'c':
                _conffile = optarg;
                break;
              case '?':
                if (optopt == 'c'){
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

