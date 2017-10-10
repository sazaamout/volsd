# Volsd (volume dispatcher) #

  *Volsd* is a thread-safe solution that replaces any type of centralized NAS/SAN storage when using Amazon Web Services. Using any type of cloud NAS that serves many webservers in an Amazon Autoscaling group will introduce latency to the client. Latency is due to the fact that data have to travelling via the network from the NAS/SAN storage to the web server that is processing the client request. The only way to reduce the latency when using such architecture, is to purchase stronger EC2 instance which could cost a lot of money.
  
  When using *Volsd*, newly created web servers will request a volume that is synced and up to date already and in few seconds the volume is mounted locally. The Idea behind *Volsd* is simple, *Volsd* creates and maintains a pre-set number of volumes and mount these volumes locally (where *Volsd* installed). It ensures that they are always identical by syncing them periodically. When *Volsd* receives a volume request from a production/webservers, it umount one of the local volume and sends it to the requestor, who intern mounts this volume locally using one of the *Volsd* component called client. To speed up the syncing process, VDC makes a snapshot of the target filesystem every predetermined interval so that when a new volume is created using the latest snapshot.  

  *Volsd* allow users to push/delete a file/directory to all of the maintained volumes, whether these volumes are mounted locally or mounted on a remote server. 

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
    -- This program must start when an instance bootup for the first time. It will send a DiskRequest to the *Volsd* to acquire a volume. The *Volsd* will detach a volume and send it that volume's id to the client. The client will mount the acquired volume As soon as it receives the volume Id.
    -- When the instance is terminated, the client program will release the mounted volume and send a "DiskRelease" message to the *Volsd* which in turn will remove that volume from the disks list.

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

  *Note: This program is tested on a centos 6.x EC2 Instance (t1.small)


# Future work #
  - Testing it on other linux operating systems
  - This program was done in a hurry, some of the code are not the most effecient
  - Add the ability to write volume informations to an s3 bucket
  - Add feedback between the *volsd-client* program and the *Volsd* when a push/delete of a file is issued
  - Uses AWS SDK rather than awscli
  - currently, volsd push changes to all servers mounting the needed filesystem. It would better if clients pull these changes from volsd's server
