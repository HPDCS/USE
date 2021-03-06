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
* @file parse-elf.h
* @brief
* @author Davide Cingolani
* @date May 22, 2014
*/

// XXX: forse questo è più da "handle elf" ?

#pragma once
#ifndef PARSE_ELF_H_
#define PARSE_ELF_H_

//parsed_elf parsed;

extern void elf_create_map(void);
extern int elf_instruction_set(void);
extern bool is_elf(char *path);
extern void link_jump_instructions(function *func, function *code, symbol *text);

#endif /* PARSE_ELF_H_ */
