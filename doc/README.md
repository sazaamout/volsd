# Overview #

## VolumeDispatcher (VD) ##

  *VolumeDispatcher* is a solution that replaces any type of centralized NAS/SAN storage when using Amazon Web Services. Using any type of cloud NAS that serves many webservers in an Amazon Autoscaling group will introduce latency to the client. Latency is due to the fact that data have to travelling via the network from the NAS/SAN storage to the web server that is processing the client request. The only way to reduce the latency when using such architecture, is to purchase stronger EC2 instance which could cost a lot of money.
  
  When using *VolumeDispatcher*, newly created web servers will request a volume that is synced and up to date already and in few seconds the volume is mounted locally. The Idea behind *VolumeDispatcher* is simple, *VolumeDispatcher* creates and maintains a pre-set number of volumes and mount these volumes locally (where *VolumeDispatcher* installed). It ensures that they are always identical by syncing them periodically. When *VolumeDispatcher* receives a volume request from a production/webservers, it umount one of the local volume and sends it to the requestor, who intern mounts this volume locally using one of the *VolumeDispatcher* component called client. To speed up the syncing process, VDC makes a snapshot of the target filesystem every predetermined interval so that when a new volume is created using the latest snapshot.  

  *VolumeDispatcher* allow users to push/delete a file/directory to all of the maintained volumes, whether these volumes are mounted locally or mounted on a remote server. 

The system consist of the following components:
  1. Manager.
  2. dispatcher.
  3. Synchronized.
  4. client.

### 1. Manager ###
  - making snapshots periodically for the target file system.
  - Ensuring that we have the required number of disks at all time.

### 2. Dispatcher ### 
  - Process "DiskRequest" from clients 
  - Process "DiskRelease" from clients
  - Process all of the administration request such as (list volumes, list volume status, etc.) 
  - maintains information about all of the created volumes such as status, mounting point, whether is mounted locally or at a remote server, and volume Id.

### 3. Synchronizer ###
  - syncing all EBS volumes together whether they are mounted locally or mounted in a another ec2 instance. 
  - process the push and delete request from client 
  - process the sync request issued by Manager to sync a newly created volume.

### 4. Client ### 
  - This program must start when an instance bootup for the first time. It will send a DiskRequest to the *dispatcher* to acquire a volume. The *dispatcher* will detach a volume and send it that volume's id to the client. The client will mount the acquired volume As soon as it receives the volume Id.
  - When the instance is terminated, the client program will release the mounted volume and send a "DiskRelease" message to the *dispatcher* which in turn will remove that volume from the disks list.

# Dependencies And System Requirements #
  The following packages are required to be installed on the machine
    1. AWS CLI version 1.10.60 (minimum)
    2. rsync version 3.0.6
    *Note: This program is tested on a centos 6.x EC2 Instance (t1.small)

# Installation #
  1. clone the project
  2. Go to build directory
  3. cmake /path/to/the/root/of/this/project (cmake ../)
  4. make

# How to run it? #
  type the following commands:
  ```
  dispatcher --conf=/path/to/config/file [--screen]
  manager --conf=/path/to/config/file [--screen]
  synchronizer --conf=/path/to/config/file [--screen]
  ```
# Future work #
  - Testing it on other linux operating systems
  - Improve some of functionality such as sync 
  - This program was done in a hurry, some of the code are not the most effecient
  - Add the ability to write volume informations to an s3 bucket
  - Add feedback between the *client* program and the *syncer* when a push/delete of a file is issued
  - Uses AWS SDK rather than awscli

