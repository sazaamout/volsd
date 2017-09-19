# Volume Dispatcher (VD) #

  VolumeDispatcher is a solution that replaces any type of centralized NAS/SAN storage when using Amazon Web Services. Using any type of cloud NAS that serves many webservers in an Amazon Autoscaling group will introduce latency to the client. Latency is due to the fact that data have to travelling via the network from the NAS/SAN storage to the web server that is processing the client request. The only way to reduce the latency when using such architecture, is to purchase stronger EC2 instance which could cost a lot of money.
  
  When using VolumeDispatcher, newly created web servers will request a volume that is synced and up to date already and in few seconds the volume is mounted locally. The Idea behind VolumeDispatcher is simple, VolumeDispatcher creates and maintains a pre-set number of volumes and mount these volumes locally (where volumeDispatcher installed). It ensures that they are always identical by syncing them periodically. When volumeDispatcher receives a volume request from a production/webservers, it umount one of the local volume and sends it to the requestor, who intern mounts this volume locally using one of the volumeDispatcher component called client. To speed up the syncing process, VDC makes a snapshot of the target filesystem every predetermined interval so that when a new volume is created using the latest snapshot.  

  volumeDispatcher allow users to push/delete a file/directory to all of the maintained volumes, whether these volumes are mounted locally or mounted on a remote server. 

The system consist of the following components:
  1. Manager.
  2. dispatcher.
  3. Synchronized.
  4. client.

## 1. Manager ##
  - making snapshots periodically for the target file system.
  - Ensuring that we have the required number of disks at all time.

## 2. Dispatcher ## 
  - Process "DiskRequest" from the EBSClient 
  - Process "DiskRelease" from EBSClient 
  - Process all of the administration request such as (list volumes, list volume status, etc.) 
  - maintains information about all of the created volumes such as status, mounting point, whether is mounted locally or at a remote server, and volume Id.

## 3. Synchronizer ## 
  - syncing all EBS volumes together whether they are mounted locally or mounted in a another ec2 instance. 
  - process the push and delete request from client 
  - process the sync request issued by EBSManager to sync a newly created volume.

## 4. Client ## 
  - When exeuted at instance boot time, this progarm will send a request to EBSController to aquire a volume. EBSController will detch the volume and send it id to the EBSClient which in trune it will mount the given volume locally. 
  - When exeuteed at instance termination time, the script will release the mounted volume and send a "DiskRelease" message to the EBSController which intruen will remove that volume from the disks list.

