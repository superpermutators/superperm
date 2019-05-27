# Readme

Author:	Robin Kay, Harika Technology
Date:	26 May 2019

## DistributedChaffinMethod

Visit <https://github.com/superpermutators/superperm/tree/master/DistributedChaffinMethod> or <http://www.supermutations.net/> for an overview of the project.

## Windows 64-bit client

## Installation:

Download `DCMWin64.zip`.

Extract the contents of `DCMWin64.zip` into a folder on your local hard drive (e.g. `c:\temp`).

Navigate to the `DCMWin64` folder.

The first time you run each application, you may see the message "Windows Protected Your PC.  Windows Defender Smart Screen prevented an unrecognized app from starting..."

* Click on More info, check that the publisher is "Harika Technology", then press Run Anyway.  
* You won't be prompted again when running the same application.
	
If you want to eliminate this warning altogether for this and future versions, do the following:

1. Right-click either of the EXE files and choose Properties
2. Select the Digital Signatures tab
3. Select Harika Technology and click Details
4. Click View Certificate, Install Certificate... Next, Next and Finish
5. If you see "Import Successful" you won't be warned again for applications signed by Harika Technology
	
## Configuration

1. You can run the `DistributedChaffinMethod.exe` from a command line, as described in the `README.md` at
<https://github.com/superpermutators/superperm/tree/master/DistributedChaffinMethod> 

AND/OR

Optionally, generate and use Windows command files:

2. To generate the command files, run `MakeDCMScripts.exe`
3. Then edit `StartDCM.cmd` to set your TEAM NAME and select how many processes to run.  Instructions are in the comments in the `StartDCM.cmd` file.

## Starting the processes

You can START calculating superpermutations by double-clicking `StartDCM`

## Stopping the processes

To stop all instances in an orderly fashion (finishing their current calculations), double-click `StopDCM`.  This can take up to an hour to complete.

To stop all instances quickly (abandoning their current calculations), double-click `QuitDCM`. 

These methods notify the server so that it can reassign the calculations to other clients.  Please don't terminate the processes any other way.
