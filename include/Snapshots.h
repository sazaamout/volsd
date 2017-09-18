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

class Snapshots {
	private:
		int snapshotMaxNumber;
		int counter;
		int snapshotFreq;
		std::string snapshotFile;
		
		struct Snapshot{
			std::string id;
			std::string date;
			std::string time;
			std::string status;
			std::string is_latest;
		};
		
		int update_snapshots(std::string s_id, std::string datetime, std::string status, std::string latest, Logger& logger);
		std::string next_token(std::string& str, std::string delimiter);
		int snapshot_count();
		
	public:
		Snapshots();
		Snapshots(int snapshot_max_no, std::string snapshot_file, int snapshot_freq);
		~Snapshots();
		
		int load_snapshots(Logger& logger);
		int delete_snapshot();
		
		int create_snapshot(std::string TargetFilesystem, int freq, Logger& logger);
		void print_snapshots(Logger& logger);
		
		std::string get_latest(std::string select, Logger& logger);
};

#endif
