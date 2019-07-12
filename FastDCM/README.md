# Readme

Author:		Greg Egan  
Date:		11 July 2019
Version:		6.0

`FastDCM` is a client for the [Distributed Chaffin Method search](https://github.com/superpermutators/superperm/wiki/The-Distributed-Chaffin-Method-Search) that uses
the Graphics Processing Unit (GPU) of your computer, rather than the CPU, for the bulk of its calculations.  Depending on the precise specifications of your system,
it might run several times faster than one instance of the single-CPU client, `DistributedChaffinMethod`.

For example, on a 2017 27-inch iMac with a 4.2GHz Intel Core i7 CPU and an AMD Radeon Pro 580 GPU, `FastDCM` runs 4-5 times faster than one instance
of `DistributedChaffinMethod` (which uses a single core of the CPU).

## Known issues

`FastDCM` will currently not work with the integrated Intel graphics that run the display on some Mac laptops, as opposed to using a discrete GPU. If your system has *both*
integrated Intel graphics and a discrete GPU, you can select the discrete GPU using the `gpuName` option described below.

## Building

You will need to download the **entire directory** containing the main source code file, `FastDCM.c`, and the subdirectories  `Headers` and `Kernels`.

To build the program under MacOS, make that downloaded directory your current directory, then type:

`gcc FastDCM.c -O3 -framework OpenCL -o FastDCM`

To build the program under Linux:

`gcc FastDCM.c -O3 -lm -lOpenCL -o FastDCM`

Building under Windows is still experimental. You will probably need to download an SDK (software development kit) that offers support for the `OpenCL` protocol
from the manufacturer of your GPU, such as NVidia or AMD, which will contain the libraries and header files that your compiler needs to build the program.

## Running

To run the program, it **must** be in the same directory as the original source code, with `Headers` and `Kernels` as subdirectories.

Before accepting any tasks from the server, the program will attempt to validate its calculations by doing a Chaffin Method search for wasted digits ranging from 1 to 90.
If any mistakes are found, the program will quit.

All the options available to the `DistributedChaffinMethod` client also work with `FastDCM`. These are documented in the
[README file](https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/README.md) for that program.

One significant difference is that `FastDCM` will run for longer before splitting the task it is working on, and once it does start splitting the
task, it will take longer to do so. It is best **not** to run `FastDCM` unless you are able to commit to running it for a substantial period of time
(at least a few hours) without interruption.

`FastDCM` has two additional options.  If your computer has multiple GPUs, you can tell the program which one to use with the `gpuName` command line argument, e.g.:

`.\FastDCM gpuName "AMD"`

In this case, only a GPU whose device name starts with "AMD" will be selected. You don't need to specify the full device name, just enough to distinguish between
the different ones on your system.

Also, if your system has multiple implementations of the `OpenCL` protocol installed, known as "platforms", you can single one out with the `gpuPlatform` command line argument, e.g.:

`.\FastDCM gpuPlatform "NVIDIA"`
