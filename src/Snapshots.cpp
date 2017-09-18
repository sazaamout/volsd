
#include "Snapshots.h"

Snapshots::Snapshots(){
	
}

Snapshots::Snapshots(int snapshot_max_no, std::string snapshot_file, int snapshot_freq){
	snapshotMaxNumber = snapshot_max_no;
	snapshotFile = snapshot_file;
	snapshotFreq = snapshot_freq;
	
	//counter = 0;
}

Snapshots::~Snapshots(){}
		

int Snapshots::snapshot_count(){
	int no=0;
	std::fstream myFile;
	std::string line;
	myFile.open(snapshotFile.c_str());
 
	if (!myFile.is_open())
		return -1;
		
    // check if file is empty
    if ( myFile.peek() == std::ifstream::traits_type::eof() )
		return 0;
    
	while (std::getline(myFile, line)){
		no++;
	}
	myFile.close(); 
	return no;
}


int Snapshots::create_snapshot(std::string TargetFilesystem, int frequency, Logger& logger){
	
	
	std::string output, snapshot_id;
	
	// get the current time 
	std::string current_datetime = utility::datetime();
	
	// get the creation date of the latest snapshot
	std::string snapshot_datetime = get_latest("date", logger);
	
	// finds the difference insecond
	int diff = utility::datetime_diff( snapshot_datetime, current_datetime );
	
	if (diff < 0) diff = diff * (-1);
	
	if ( diff < ( frequency * 60 ) ){
		logger.log("info", "infra1", "SnapshotClass", 0, "no need to create a snapshot ...", "create_snapshot");	
		return 0;
	}
		
	// create a new snapshot	
	logger.log("debug", "infra1", "SnapshotClass", 0, "creating new snapshot ...", "create_snapshot");	
	int res = utility::exec(output, "aws ec2 create-snapshot --volume-id " + TargetFilesystem + " --description \"This is my root volume snapshot\" --region us-east-1 --query SnapshotId --output text");
	if (!res){
		logger.log("error", "infra1", "SnapshotClass", 0, "Failed to create new snapshot", "create_snapshot");	
		logger.log("error", "infra1", "SnapshotClass", 0, "AWSERROR:[" + output + "]", "create_snapshot");	
		return 0;
	}
	snapshot_id = output; 
	utility::clean_string(snapshot_id);
	
	output = "";
	utility::exec(output, "aws ec2 describe-snapshots --snapshot-id " + snapshot_id + " --region us-east-1 --query Snapshots[].State --region us-east-1 --output text");
	utility::clean_string(output);
	
	while (output.compare("pending") == 0) {
		logger.log("debug", "infra1", "SnapshotClass", 0, "awaiting for [" + snapshot_id + "] to be ready ...", "create_snapshot");	
		output = "";
		utility::exec(output, "aws ec2 describe-snapshots --snapshot-id " + snapshot_id + " --region us-east-1 --query Snapshots[].State --region us-east-1 --output text");
		utility::clean_string(output);
		sleep(1);
	}
	
	logger.log("info", "infra1", "SnapshotClass", 0, "Snapshot [" + snapshot_id + "] was created", "create_snapshot");	
	
	//updating snapshot file
	update_snapshots(snapshot_id, current_datetime ,"idle", "true", logger);
	//print_snapshots();
	return 1;
}


void Snapshots::print_snapshots(Logger& logger) {
	
	std::fstream myFile;
	myFile.open(snapshotFile.c_str());
	
	if (!myFile.is_open()){
		logger.log("error", "infra1", "SnapshotClass", 0, "could not open snapshot file", "get_latest");	
		return;
	}

	std::cout << "\n PRINT_SNAPSHOT:: SNAPSHOTS LIST" << std::endl;
	std::cout << " ==========================================================" << std::endl;
	
	std::string line;
	while (std::getline(myFile, line)){
		std::cout << line << "\n";
	}
	std::cout << " ==========================================================\n\n";

	myFile.close();
}


int Snapshots::update_snapshots(std::string s_id, std::string datetime, std::string status, std::string latest, Logger& logger) {
	
	std::ofstream out;
	
	// 1) appned the new snapshot to file
	logger.log("debug", "infra1", "SnapshotClass", 0, "append the new snapshot to file", "update_snapshots");	
	out.open(snapshotFile.c_str(), std::ios_base::app);
	if (!out.is_open()){
		logger.log("error", "infra1", "SnapshotClass", 0, "could not open snapshot file", "get_latest");	
		return 0;
	}
	//std::cout << "inserting: " << s_id << " " << datetime << " " << status << " " << latest << "\n";
	out << s_id << " " << datetime << " " << status << " " << latest << "\n";
	out.close();
	
	

	// 2) count snapshots to determine if they exceed the needed number
	std::fstream myFile;
	myFile.open(snapshotFile.c_str());
	
	int counter = 0;
	std::string line;
	while (std::getline(myFile, line))
		counter++;
	myFile.close();
	myFile.clear();
	logger.log("debug", "infra1", "SnapshotClass", 0, "snapshots number:[" + utility::to_string(counter) + "]", "update_snapshots");		

	// 3) Load data into structure and mainain number of snapshpt
	logger.log("debug", "infra1", "SnapshotClass", 0, "loading snapshot data from file", "update_snapshots");	
	int skip = 0;
	Snapshot s[ counter - skip ];

	if (counter > snapshotMaxNumber){ 
		skip = counter - snapshotMaxNumber;
		logger.log("debug", "infra1", "SnapshotClass", 0, "exceeding Maximum nmber of snapshot allowed:[" + utility::to_string(counter) + "/" + utility::to_string(snapshotMaxNumber) + "]", "update_snapshots");		
	}
	// reads file, line by line. Delete the skipped one and load the array with the other lines
	myFile.open(snapshotFile.c_str());
	std::string ss_id, ss_date, ss_time, ss_status, ss_latest, output;
	
 	int sk = 0, i = 0;	
 	while ( std::getline( myFile, line ) ){
		
		if ( sk < skip ){
			// remove the snapsho from this line
			std::stringstream ss(line);
			ss >> ss_id;
			utility::exec(output, "aws ec2 delete-snapshot --snapshot-id " + ss_id + " --region us-east-1");
			sk++;
		} else {
			// copy line into array
			std::stringstream ss(line);
			ss >> ss_id >> ss_date >> ss_time >> ss_status >> ss_latest;
			s[i].id = ss_id;
			s[i].date = ss_date; 
			s[i].time = ss_time;
			s[i].status = ss_status;
			s[i].is_latest = "false";
			i++;
		}
	}
	
	// mark the last snapshot as latest
	s[i-1].is_latest = "true";
	myFile.close();
	myFile.clear();
	
	// 4) clear the file contents and dump back the array into the file
	logger.log("debug", "infra1", "SnapshotClass", 0, "Updating snapshot file", "update_snapshots");	
	myFile.open(snapshotFile.c_str(), std::fstream::out | std::fstream::trunc);
 
	for (int ii=0; ii<i; ii++) {
		//std::cout << s[i].id << "|" << s[i].date << "|" << s[i].time << "|" << s[i].status << "|" << s[i].is_latest << "\n";
		myFile << s[ii].id << " " << s[ii].date << " " << s[ii].time << " " << s[ii].status << " " << s[ii].is_latest << "\n";
	}
	myFile.close();	
	
	logger.log("debug", "infra1", "SnapshotClass", 0, "snapshot file was updated", "update_snapshots");	
  
	
	return 1;
}


std::string Snapshots::get_latest(std::string select, Logger& logger){
	
	logger.log("debug", "infra1", "SnapshotClass", 0, "looking for latest snapshot", "get_latest");	
	
	std::fstream myFile;
	myFile.open(snapshotFile.c_str());
 
	if (!myFile.is_open()){
		logger.log("error", "infra1", "SnapshotClass", 0, "could not open snapshot file", "get_latest");	
		return 0;
	}
	
	std::string line, snapshotId, date, time, status, latest;
	
	while (std::getline(myFile, line)){
		
		//std::cout << "line: " << line << "\n";
		
		if ( line.find("true") != std::string::npos ) {
			std::stringstream ss(line);
			ss >> snapshotId >> date >> time >> status >> latest;
		
			if (select == "id"){ 
				logger.log("debug", "infra1", "SnapshotClass", 0, "latest snapshot id:[" + snapshotId + "]", "get_latest");
				return snapshotId;
			}else{
				logger.log("debug", "infra1", "SnapshotClass", 0, "latest snapshot date:[" + date + "] time:[" + time + "]", "get_latest");
				return date + " " + time;
			}
		}
	}
	
	myFile.close();
	return "";
	
}
