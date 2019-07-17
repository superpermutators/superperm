# Readme

Author:		Greg Egan  
Date:		17 July 2019
Version:		6.1

`FastDCMTest` is a C program that will test whether your computer can run the Chaffin Method calculations correctly and efficiently using its GPU, instead of
(or in addition to) the CPU.


## Building

You will need to download the **entire directory** containing the main source code file, `FastDCMTest.c`, and the subdirectories  `Headers` and `Kernels`.

To build the program under MacOS, make that downloaded directory your current directory, then type:

`gcc FastDCMTest.c -O3 -framework OpenCL -o FastDCMTest`

To build the program under Linux:

`gcc FastDCMTest.c -O3 -lm -lOpenCL -o FastDCMTest`

## Running

To run the program, it must be in the same directory as the original source code, with `Headers` and `Kernels` as subdirectories.  You can run it from the comand line with:

`.\FastDCMTest`

If your computer has multiple GPUs, you can tell the program which one to use with the `gpuName` command line argument, e.g.:

`.\FastDCMTest gpuName "AMD Radeon"`

In this case, only a GPU whose device name starts with "AMD Radeon" will be selected. You don't need to specify the full device name, just enough to distinguish between
the different ones on your system.

If your system has multiple GPUs with **identical names**, you can use the `gpuDeviceNumber` option, to specify the number of the device in the list the program produces, e.g.:

`.\FastDCMTest gpuDeviceNumber 2`

If you have multiple GPUs and you wish to test the performance when they are used simultaneously, you can run multiple instances of the program and choose
different GPUs for each instance.

Also, if your system has multiple implementations of the `OpenCL` protocol installed, known as "platforms", you can single one out with the `gpuPlatform` command line argument, e.g.:

`.\FastDCMTest gpuPlatform "NVIDIA"`


## Expected output

The program will start by listing details of all the GPUs it finds, and it will exit if no suitable devices are found.

If a suitable GPU is found, it will attempt to compile the "kernels":  the code that will run on the GPU itself.

If that succeeds, the program will attempt to validate its calculations, by doing a Chaffin Method search for wasted digits ranging from 1 to 90.
If any mistakes are found, the program will quit.

Finally, the program will proceed to give benchmarks of its performance, in terms of nodes searched per second. This will continue every 10 seconds indefinitely. You can
test this out to see how the program performs, and also how well you can perform other activities on your system while it is running. When you want to stop, just type CTRL-C
to kill the program.

The existing single-CPU client, `DistributedChaffinMethod`, reports "Nodes searched per second", which you can compare with these results to see if you would gain any
advantage by switching to your GPU (once the full client becomes available). 
