Reproducibility PADS 2022
=========================

This documents describes how to reproduce the results discussed in the paper:

"Spatial/Temporal Locality-based Load-sharing in Speculative
Discrete Event Simulation on Multi-core Machine"

To be published in ACM SIGSIM PADS 2022

Authors & Contacts
------------------

Federica Montesano <>
Romolo Marotta <romolo.marotta@gmail.com>
Francesco Quaglia <>

Requirements for running the tests
----------------------------------

* x86_64 hardware
* Unix system with gcc toolchain

Requirements for generating figures
-----------------------------------

* Python3, matplotlib and seaborn
* bash


Configuration
-------------

USE does not need any manual configuration. Type the following

* `make config`

and everything should go smoothly.
Such a phase is required to generate necessary information about cache layout and clocks.
In particular, the folder `include-gen` should contain two headers:

  * `clock_constant.h`
  * `hpipe.h`


Reproducing the results of the paper
------------------------------------

The paper has 4 Figures that can be reproduced:

* Figure 3;
* Figure 4;
* Figure 5;
* Figure 6 (this figure has two subplots, one for each of the hardware platforms used for the experimental evaluation).

To generate all the figures, type the following:

* ./run_and_gen_all.sh
* ./process_data.sh

The Figures will be available in the `figures` folder.

There is one script for running tests and generating plots: `run_and_gen_all.sh`.
However, run all the tests should require about 8 hours.
If you prefer to split the running time, each Figure has associated two scripts for executing the simulations and generating data,  figure*-run.sh and figure*-plot.sh respectively.

Notes
-----

* We uploaded the thread counts for targeting platform A (48 cores) discussed within the paper.
If you want to define thread counts for your machine, please modify the `thread.conf` file as you prefer.

* Figure 3 shows one subset of a more extensive exploration of the W parameter. The script generates 6 variants for different values of W. The one corresponding to Figure 3 of the paper is: `figure3-lp_4096-ta_0.24-w_1.6.pdf`. 

