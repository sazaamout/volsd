# Volsd (volume dispatcher) #
Built for systems that are hosted on Amazon Web Services, *Volsd* is a solution that can be used to substitute any type of a cloud NAS. While using a cloud NAS offers great advantages, disadvantages such as higher latency is unavoidable. To reduce latency, ones can upgrade the EC2 instances to instaces with better network throughput. Upgrading the instances will reduce latency by a little, however, it will cost much more money. The best solution for such problem is to have the data stored locally in every web server. One problem with this approach is that ensuring consistency of data in all of the web servers is a tedious task. Another problem is the time it takes to copy the data to a newly created web server especially when using Amazon AutoScaling Service keeping in mind that the copy/clone the data requires human intervention.
 
*Volsd* solves these problems by maintaining a pre-set number of EBS Volumes that are synced periodically and up to date all the time. These (idle) EBS Volumes will be automatically mounted on a Newly created web server using a volsd-client program. Because the volumes are always synced, these is no need to wait for data to be copied and the newly created web server can start serving request immediately.  Pushing a changes such as new file/directory is created is an easy task and *Volsd* will ensure that your changes are applied to all web servers and to all idle volumes.

The system consist of the following components:
### *Volsd* ### 
   - Process "DiskAcquireRequest" from clients (EC2 Instances)
   - Process "DiskReleaseRequest" from clients
   - Process all of the administration request such as (list volumes, list volume status, etc.) 
   - maintains information about all of the created volumes such as status, mounting point, whether is mounted locally or at a remote server, and volume Id.
   - making snapshots periodically for the target file system.
   - Ensuring that we have the required number of disks at all time.
   - syncing all EBS volumes together whether they are mounted locally or mounted in a another ec2 instance. 
   - process the push and delete request from client 
   - process the sync request issued by Manager to sync a newly created volume.
### *Volsd-client* ###
   - This program must start when an instance bootup for the first time. It will send a DiskRequest to the *Volsd* to acquire a volume. The *Volsd* will detach a volume and send it that volume's id to the client. The client will mount the acquired volume As soon as it receives the volume Id.
   - When the instance is terminated, the client program will release the mounted volume and send a "DiskRelease" message to the *Volsd* which in turn will remove that volume from the disks list.

## Pre Installation ##
  1. Ensure that the root user is able to perform passwordless ssh in to all target servers. To allow root user to perform ssh without password, follow the instruction on the following link. http://www.linuxproblem.org/art_9.html
  2. Ensure that aws-cli package is installed and configured. This means that the access-code and the secret keys have to be supplied. Make sure that aws-cli is alloed to perform s3 and ec2 operations.
  
## Installation ##
  1. Clone the project using *git clone* command
  2. There are two differet CMakeLists.txt files,
     - If you are only testing the project, then rename CMakeLists.txt.dev to CMakeListiuis.txt. This will install binaries and configurations file inside your build folder in a dorectory called *install*.
     - If you want to use it in production, then rename CMakeLists.txt.prod to CMakeLists.txt. This will install binaries in /usr/local/bin/ and the configurations file inside /etc/volsd. 
  3. Create a build directory anywhere you want. I perfer creating the build directory inside the root directory of the cloned project
  4. Inside the build directory, type the following:
  ```
  cmake /path/to/project/root/dir
  make
  sudo make install
  ```
  
## Using Volsd ##
  *volsd* can be started using the following command: 
  ```
  $> volsd [-s] [-c /path/to/config/file] 
  ```
  [-s] : Optional, is used to print output to stdout. Use this if you want to debug some issues.
  [-c] : Optional, used this option only if you changed the location if the volsd.conf file. the default is /etc/volsd/volsd.conf
    
## How to run the volsd-client? ##
  You can get the *volsd-client* program from the build directory. There are many scenarios that can be used with the client
  1. place the client program in an s3 bucket and download it in to the newly lunched ec2 instance using the instance’s User Data. For more information on using 'User Data'  please visit the following link. You can place the following code in your instance's 'user data' section
  ```
  cd /usr/local/src
  aws s3 cp s3://location/of/client/program/in/s3/volsd-client .
  chmod u+x volsd-client
  # run the client program to aquire a volume for this instance
  volsd-client -a /directory/to/be/used/to/mount/the/volume
  ```
  2. The client program (executable/binary) have to be part of the EC2 image. You can either install it and create an image of that instance. Or you can put it on an s3 bucket and download to the insane being created using the user-data. 
  

# Dependencies And System Requirements #
  The following packages are required to be installed on the machine:
  - AWS CLI version 1.10.60 (minimum) installed and configured for the root user
  - Rsync version 3.0.6
  - cmake version 2.8.12.2
  - minimum gcc version: 4.4.7

  *Note: This program is tested on the following operating systems: 
    - Amazon Linux
    - Centos 6.x and 7.x
    - Ubuntu 16.4.
    - Fedora
    

# Future work #
  - Setup volsd to use pull updat rather than push update to the clients.
  - Testing it on other linux operating systems
  - Use Amazon s3 object to store volumes and snapshots information rather than storing in the local disk.
  - Volsd syncs the volumes by pushing changes. It would be more better if clients servers pull these changes from volsd's server. This way, volsd server can focus on doing other tasks
  - Maintain client's server status so that Volsd pushes changes to all 'running' EC2 instances. Currently, volsd will attempt to push the changes without checking if ec2 instance is stopped which will fail and get logged. 
  - Add feedback between the *volsd-client* program and the *Volsd* when a push/delete of a file is issued
  - Uses AWS SDK rather than awscli
  
