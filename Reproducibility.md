Reproducibility PADS 2022
=========================

This documents describes how to reproduce the results discussed in the paper:

"Spatial/Temporal Locality-based Load-sharing in Speculative
Discrete Event Simulation on Multi-core Machine"

Published in ACM SIGSIM PADS 2022

Authors
-------

Federica Montesano
Romolo Marotta
Francesco Quaglia

Contacts
--------


Requirements for running the tests
----------------------------------

* x86_64 hardware
* Unix system with gcc toolchain

Requirements for generating figures
-----------------------------------

* Python3
* bash


Configuration
-------------

* USE does not need any manual configuration. Just type `make` and everything should go smoothly.
  Such a phase is required to generate necessary information about cache layout and clocks.
  In particular, the folder `include-gen` should contain two headers:
  * `clock_constant.h`
  * `hpipe.h`


Reproducing the results of the paper
------------------------------------

The paper has 4 Figures that can be reproduced:

* Figure 3
* Figure 4
* Figure 5
* Figure 6 (this has two subplots for two different machines)

There is one script for running tests and generating plots: `run_and_gen_all.sh`.
However
Each Figure has associated two scripts for executing the simulations and generating data,  figure*-run.sh and figure*-plot.sh respectively.
