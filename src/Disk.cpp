#include "Disk.h"
#include <fstream>
#include <iostream> // used for debug, remove onece debug is no longer needed
#include <sstream>

using namespace utility;

Disk::Disk (bool d){
	_debug = d;
	max_wait_time = 15;
}

Disk::~Disk() { }

