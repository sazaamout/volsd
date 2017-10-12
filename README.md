# Volsd (volume dispatcher) #
  Built for systems that are hosted on Amazon Web Services, *Volsd* is a solution that can be used to substitute any type of a cloud NAS. Using a cloud NAS has some advantages, however, disadvantages such as higher latency is unavoidable. To reduce latency, you can upgrade the EC2 instances that your NAS's and the web servers use to instances with better network throughput. Upgrading the instances will reduce latency by a little, however, it will cost much more money. The best solution for such problem is to have the data stored locally in every web server. One problem with this approach is that ensuring consistency of data in all of the web servers is a tedious task. Another problem is the time it takes to copy these data to a newly created web server especially when using Amazon AutoScaling Service.
  
  *Volsd* solves these problems by maintaining a pre-set number of EBS Volumes that are synced periodically and up to date all the time. These (idle) EBS Volumes will be automatically mounted on a Newly created web server using a volsd-client program. Because the volumes are always synced, these is no need to wait for data to be copied and the newly created web server can start serving request immediately.  Pushing a changes such as new file/directory is created is an easy task and *Volsd* will ensure that your changes are applied to all web servers and to all idle volumes.


The system consist of the following components:
1. Volsd
   - Process "DiskAcquireRequest" from clients (EC2 Instances)
   - Process "DiskReleaseRequest" from clients
   - Process all of the administration request such as (list volumes, list volume status, etc.) 
   - maintains information about all of the created volumes such as status, mounting point, whether is mounted locally or at a remote server, and volume Id.
   - making snapshots periodically for the target file system.
   - Ensuring that we have the required number of disks at all time.
   - syncing all EBS volumes together whether they are mounted locally or mounted in a another ec2 instance. 
   - process the push and delete request from client 
   - process the sync request issued by Manager to sync a newly created volume.
2. Volsd-client.
   - This program must start when an instance bootup for the first time. It will send a DiskRequest to the *Volsd* to acquire a volume. The *Volsd* will detach a volume and send it that volume's id to the client. The client will mount the acquired volume As soon as it receives the volume Id.
   - When the instance is terminated, the client program will release the mounted volume and send a "DiskRelease" message to the *Volsd* which in turn will remove that volume from the disks list.

# Main features #
## Pre Installation ##
  1. ensure that the root user is set to perform passwordless ssh in to all target servers. This means you have to setup ssh keys for the root user. All target servers must have the root key when they are starting. You can configure one server with root keys and save it as image to be used in an autoscaling group.
  2. ensure that aws cli has the correct permissions to perform s3 and ec2 operations.
  
## Installation ##
  1. clone the project
  2. create a build directory anywhere you want. I perfer creating the build directory inside the root directory of the project
  3. inside the buid directory, do the following
  ```
  cmake /path/to/project/root/dir
  ```
  4. make; make install

## Using Volsd ##
  type the following commands:
  ```
  $> volsd [-s] [-c /path/to/config/file] 
  ```
## How to run the volsd-client? ##
  the client program (executable/binary) have to be part of the EC2 image. You can wither install it
  and save the image. Or you can put it on an s3 bucket and download to the instane being created 
  using the user-data. 
  

# Dependencies And System Requirements #
  The following packages are required to be installed on the machine:
  - AWS CLI version 1.10.60 (minimum) installed and configured for the root user
  - Rsync version 3.0.6
  - cmake version 2.8.12.2

  *Note: This program is tested on the following operating systems: 
    - Amazon Linux
    - Centos 6.x and 7.x
    - Ubuntu 16.4.
    - Fedora
    

# Future work #
  - Testing it on other linux operating systems
  - Use Amazon s3 object to store volumes and snapshots information rather than storing in the local disk.
  - Volsd syncs the volumes by pushing changes. It would be more better if clients servers pull these changes from volsd's server. This way, volsd server can focus on doing other tasks
  - Maintain client's server status so that Volsd pushes changes to all 'running' EC2 instances. Currently, volsd will attempt to push the changes without checking if ec2 instance is stopped which will fail and get logged. 
  - Add feedback between the *volsd-client* program and the *Volsd* when a push/delete of a file is issued
  - Uses AWS SDK rather than awscli
  
