/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file ROOT-Sim.h
* @brief This header defines all the symbols which are needed to develop a Model
*        to be simulated on top of ROOT-Sim.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date 3/16/2011
*/



#pragma once
#ifndef __ROOT_Sim_H
#define __ROOT_Sim_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>

#include "simtypes.h"



// ROOT-Sim core API
extern void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

extern void ProcessEvent(int my_id, simtime_t now, int event_type, void *data, unsigned int data_size, void *state);

extern int OnGVT(unsigned int me, void *snapshot);

#endif

