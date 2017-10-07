// Definition of the ServerSocket class

#ifndef Snapshots_class
#define Snapshots_class

#include <string>
#include <iostream>
#include <fstream>
//#include <stdio.h> // userd for popen()
//#include <algorithm> // used for remove. for string_clean function
#include "Utils.h" 
#include "Logger.h" 
#include <unistd.h> // used for sleep

class Snapshots {
	private:
		int m_snapshotMaxNumber;
    int m_snapshotFreq;
		std::string m_snapshotFile;
    std::string m_snapshotFileStorage;
		int counter;
		
    
		struct Snapshot{
			std::string id;
			std::string timestamp;
			std::string status;
			std::string is_latest;
		};
    
    std::vector<Snapshot> m_snapshots;
		Logger *logger;
    
		int update_snapshots( const Snapshot t_snapshot );
		std::string next_token(std::string& str, std::string delimiter);
		
		int write_to_file();
    
	public:
		Snapshots(){};
		Snapshots( const int t_snapshotMaxNo, const std::string t_snapshotFile, 
               const int t_snapshotFreq,  const std::string t_snapshotFileStorage );
		~Snapshots();
		void set_logger_att( bool toScreen, std::string logFile, int loglevel );
    
		int load_snapshots(Logger& logger);
		int delete_snapshot();
		
		int create_snapshot( const std::string t_targetFilesystem, int t_frequency );
		
		
		//std::string get_latest(std::string select, Logger& logger);
    int latest( std::string &t_snapshotId );
    std::string latest_date( );
    int timeToSnapshot();
    int load();
    int size();
    void print();
};

#endif
