/**
*                       Copyright (C) 2008-2015 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of the Hijacker static binary instrumentation tool.
*
* Hijacker is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* Hijacker is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* hijacker; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file create.c
* @brief Multiplexer for the generation of output object files
* @author Alessandro Pellegrini
*/
#include <stdlib.h>

#include <hijacker.h>
#include <prints.h>
#include <executable.h>

#include <elf/emit-elf.h>

void output_object_file(char *pathname) {
	hprint("Generating the new object file...\n");

	// Switch on file type
	switch(PROGRAM(type)) {

		case EXECUTABLE_ELF:

			elf_generate_file(pathname);
			break;

		default:
			hinternal();
	}

	hsuccess();
}
