USE-IT
======

The Ultimate Share-Everything sImulaTion platform

## Brief
The Ultimate Share Everything Simulator is an x86-64 Open Source,
parallel simulation platform developed using C/POSIX
technology. The platform transparently supports all the mechanisms 
associated with parallelization and optimistic synchronization 
(e.g., state recoverability).

The programming model supported by USE-IT allows the simulation-model
developer to use a simple application-callback function named
`ProcessEvent()` as the event handler, whose parameters determine which
simulation object is currently taking control for processing its next
event, and where the state of this object is located in memory. In
USE-IT, a simulation object is a data structure, whose state can be
scattered on dynamically allocated memory chunks, hence the memory
address passed to the callback locates a top level data structure
implementing the object state-layout.

## Requirements

1. USE-IT is written in C standard.
2. USE-IT targets x86 platforms.

## Dependecies

* git, cmake, lcov, libnuma-dev

## Build USE and its models

1. Create a directory ```<dir>``` for binaries
2. ```cd <dir>```
3. ```cmake <path to USE folder> -CMAKE_BUILD_TYPE=<Release|Debug>``` 
4. ```make -j<num_cores>``` 


## Run a model

Step 4 builds an executable ```test_<model>``` for each model in ```<dir>/test/test_<model>``` folder.
To run a model with 10 cores and 1024 lps:

5. ```<dir>/test/test_<model> -c 10 -p 1024```

6. ```<dir>/test/test_<model> -?``` to inspect both USE and model options.


## Publications and Reproducibility

This repository is associated to several publications.


The main one is the following:

1. M. Ianni, R. Marotta, D. Cingolani, A. Pellegrini, and F. Quaglia, **The Ultimate Share-Everything PDES System**, SIGSIM-PADS '18. https://doi.org/10.1145/3200921.3200931


Other publications:

2. M. Ianni, R. Marotta, A. Pellegrini and F. Quaglia, **Towards a fully non-blocking share-everything PDES platform**, DS-RT '17. https://ieeexplore.ieee.org/abstract/document/8167663
3. M. Ianni, R. Marotta, D. Cingolani, A. Pellegrini and F. Quaglia, **Optimizing Simulation On Shared-Memory Platforms: The Smart Cities Case**, WSC '18. https://ieeexplore.ieee.org/abstract/document/8632301
4. E. Silvestri, C. Milia, R. Marotta, A. Pellegrini, and F. Quaglia, **Exploiting Inter-Processor-Interrupts for Virtual-Time Coordination in Speculative Parallel Discrete Event Simulation** SIGSIM-PADS '20. https://doi.org/10.1145/3384441.3395985
5. S. Conoci, M. Ianni, R. Marotta, and A. Pellegrini, **Autonomic Power Management in Speculative Simulation Runtime Environments**, SIGSIM-PADS '20. https://doi.org/10.1145/3384441.3395980
6. F. Montesano, R. Marotta, and F. Quaglia, **Spatial/Temporal Locality-based Load-sharing in Speculative Discrete Event Simulation on Multi-core Machines**, SIGSIM-PADS '22. https://doi.org/10.1145/3518997.3531026
7. R. Marotta, F. Montesano, and F. Quaglia, **Effective Access to the Committed Global State in Speculative Parallel Discrete Event Simulation on Multi-core Machines** SIGSIM-PADS '23. https://doi.org/10.1145/3573900.3591117
8. R. Marotta, F. Montesano, A. Pellegrini and F. Quaglia, **Incremental Checkpointing of Large State Simulation Models with Write-Intensive Events via Memory Update Correlation on Buddy Pages**, DS-RT '23. https://ieeexplore.ieee.org/document/10305756
  
