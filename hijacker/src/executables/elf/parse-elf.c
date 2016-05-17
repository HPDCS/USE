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
* @file parse-elf.c
* @brief Transforms an ELF object file in the hijacker's intermediate representation
* @author Alessandro Pellegrini
* @author Davide Cingolani
* @author Simone Economo
* @date September 19, 2008
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <endian.h>
#include <string.h>
#include <limits.h>

#include <hijacker.h>
#include <prints.h>
#include <executable.h>
#include <instruction.h>
#include <utils.h>

#include <elf/elf-defs.h>
#include <elf/handle-elf.h>
#include <x86/x86.h>


#define MAX_LOOKBEHIND		10 // [SE] Used while reverse-parsing instruction to resolve jump tables

static section *relocs = 0;		/// List of all relocations sections parsed
static section *symbols = 0;		/// List of all symbols parsed
static section *code = 0;		/// List of whole code sections parsed
static function *functions = 0;		/// List of resolved functions
static char *strings = 0;		/// Array of strings
static block *blocks = 0;		/// [SE] List of recognized basic blocks

// FIXME: is this really used?
static unsigned char *strtab(unsigned int byte) {

	// This will give immediate access to the symbol table's string table,
	// and will be populated upon the first execution of this function.
	static unsigned int sym_strtab = UINT_MAX;

	register unsigned int i;

	if(sym_strtab == UINT_MAX) { // First invocation: must lookup the table!
		for(i = 0; i < ELF(secnum); i++) {
			if(sec_type(i) == SHT_STRTAB && shstrtab_idx() != i) {
				sym_strtab = i;
				break;
			}
		}
	}

	// I assume that if this function was called, at least one symbol
	// is present, so at least one name is, and therefore at least one
	// string table is present, so I don't check if the table does
	// not exist!


	// Now get displace in the section and return
	return (unsigned char *)(sec_content(sym_strtab) + byte);

}


static void elf_tls_section(int sec) {

	hnotice(2, "TLS section found %u\n", sec);

	// We do not need to perform any particular task here...

	if (sec_type(sec) & SHT_PROGBITS) {
		add_section(SECTION_TLS, sec, sec_content(sec), NULL);

		hdump(3, sec_name(sec), sec_content(sec), sec_size(sec));
	} else {
		add_section(SECTION_TLS, sec, NULL, NULL);
	}

	hsuccess();

}


static void elf_raw_section(int sec) {

	hnotice(2, "Nothing to do here...\n");

	// We do not need to perform any particular task here...
	add_section(SECTION_RAW, sec, sec_content(sec), NULL);

	if (sec_type(sec) & SHT_PROGBITS) {
		hdump(4, sec_name(sec), sec_content(sec), sec_size(sec));
	}

	hsuccess();

}






static void elf_code_section(int sec) {
	insn_info *first, *curr;

	unsigned long pos = 0, size;

	char flags = 0;

	first = curr = (insn_info *)malloc(sizeof(insn_info));
	bzero(first, sizeof(insn_info));
	size = sec_size(sec);

	// Preset some runtime parameters for instruction set decoding (when needed)
	switch(PROGRAM(insn_set)) {
	case X86_INSN:
		if(ELF(is64)) {
			flags |= DATA_64;
			flags |= ADDR_64;
		} else {
			flags |= DATA_32;
			flags |= ADDR_32;
		}
		break;
	}



	// Decode instructions and build functions map
	while(pos < size) {

		switch(PROGRAM(insn_set)) {

			case X86_INSN:
				x86_disassemble_instruction(sec_content(sec), &pos, &curr->i.x86, flags);
				hnotice(6, "%#08lx: %s (%d)\n", curr->i.x86.initial, curr->i.x86.mnemonic, curr->i.x86.opcode_size);
				hdump(5, "Disassembly", curr->i.x86.insn, 15);
				//hprint("%s, %s works on stack\n", curr->i.x86.mnemonic, curr->i.x86.flags & I_STACK ? "" : "not");

				// Make flags arch-independent
				curr->flags = curr->i.x86.flags;
				curr->new_addr = curr->orig_addr = curr->i.x86.initial;
				curr->size = curr->i.x86.insn_size;
				curr->opcode_size = curr->i.x86.opcode_size;

				curr->secname = sec_name(sec);

				//TODO: debug
				/*hprint("ISTRUZIONE:: '%s' -> opcode = %hhx%hhx, opsize = %d, insn_size = %d; breg = %x, "
						"ireg = %x; disp_offset = %lx, jump_dest = %lx, scale = %lx, span = %lx\n",
						curr->i.x86.mnemonic, curr->i.x86.opcode[1], curr->i.x86.opcode[0], curr->i.x86.opcode_size, curr->i.x86.insn_size,
						curr->i.x86.breg, curr->i.x86.ireg, curr->i.x86.disp_offset, curr->i.x86.jump_dest,
						curr->i.x86.scale, curr->i.x86.span);*/

				break;

			default:
				hinternal();
		}

		// Link the node and continue
		curr->next = (insn_info *)malloc(sizeof(insn_info));
		bzero(curr->next, sizeof(insn_info));
		curr->next->prev = curr;	// Davide
		curr = curr->next;

	}

	// TODO: we left a blank node at the end of the chain!
	curr->prev->next = NULL;
	free(curr);

	// At this time, we consider the sections just as a sequence of instructions.
	// Later, a second pass on this sequence will divide instructions in functions,
	// but we must be sure to have symbols loaded, which we cannot be at this
	// stage of processing
	// FIXME: eliminare la ridondanza sulle chiamate add_section add_sec!
	// add_section(SECTION_CODE, sec, first);
	add_section(SECTION_CODE, sec, first, &PROGRAM(sections));
	add_section(SECTION_CODE, sec, first, &code);

	hsuccess();
}




static void elf_symbol_section(int sec) {

	Elf_Sym *s;
	symbol	*sym, *first, *last;

	unsigned int 	pos = 0;
	unsigned int	size = sec_size(sec);
	unsigned int	sym_count = 0;

	int type;
	int bind;

	first = sym = (symbol *) malloc(sizeof(symbol));
	bzero(first, sizeof(symbol));

	while(pos < size) {

		s = (Elf_Sym *)(sec_content(sec) + pos);
		type = ( ELF(is64) ? ELF64_ST_TYPE(symbol_info(s, st_info)) : ELF32_ST_TYPE(symbol_info(s, st_info)) );
		bind = ( ELF(is64) ? ELF64_ST_BIND(symbol_info(s, st_info)) : ELF32_ST_BIND(symbol_info(s, st_info)) );

		// TODO: handle binding, visibility (for ld.so)

		if(type == STT_OBJECT || type == STT_COMMON || type == STT_FUNC || type == STT_NOTYPE ||
				type == STT_SECTION || type == STT_FILE || type == STT_TLS) {

			switch(type) {
			case STT_FUNC:
				sym->type = SYMBOL_FUNCTION;
				break;

			case STT_COMMON:
			case STT_OBJECT:
				sym->type = SYMBOL_VARIABLE;
				break;

			case STT_NOTYPE:
				sym->type = SYMBOL_UNDEF;
				break;

			case STT_SECTION:
				sym->type = SYMBOL_SECTION;
				break;

			case STT_FILE:
				sym->type = SYMBOL_FILE;
				break;

			case STT_TLS:
				sym->type = SYMBOL_TLS;
				break;

			default:
				hinternal();
			}

			sym->name = strtab(symbol_info(s, st_name));
			sym->size = symbol_info(s, st_size);
			sym->extra_flags = symbol_info(s, st_info);
			sym->secnum = symbol_info(s, st_shndx);
			sym->index = sym_count;

			// XXX: "initial" was intended here as the initial value, but st_value refers to the position of the instruction.
			// This was breaking the generation of references in case of local calls.
			// I don't know if it is safe to remove the "initial" field anyhow
			sym->position = symbol_info(s, st_value);
			// sym->initial = symbol_info(s, st_value);
			sym->bind = bind;

			hnotice(2, "[%d] %s: '%s' [%d] in section %d :: %lld\n",
				sym->index, (sym->type == SYMBOL_FUNCTION ? "Function" :
					(sym->type == SYMBOL_UNDEF ? "Undefined" :
						sym->type == SYMBOL_SECTION ? "Section" :
							sym->type == SYMBOL_FILE ? "File" :
								"Variable")), sym->name, sym->size, sym->secnum, sym->position);

			// insert symbol
			sym->next = (symbol *) malloc(sizeof(symbol));
			bzero(sym->next, sizeof(symbol));
			last = sym;
			sym = sym->next;

		}

		sym_count++;
		pos += (ELF(is64) ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym));
	}

	// free the last empty symbol
	last->next = NULL;
	free(sym);

	// TODO: is needed, now, to add symbol chain to the program structure (ie. executable_info)?


	// At this stage symbol section will contain a list of symbols.
	// This section will be appended to the linked list of all section
	// maintained by the program descriptor.
	add_section(SECTION_SYMBOLS, sec, first, NULL);
	add_section(SECTION_SYMBOLS, sec, first, &symbols);

	hsuccess();
}





static void elf_rel_section(int sec) {
	Elf_Rel *r;
	reloc *first, *prev, *rel;

	size_t pos, size;
	unsigned long long info;

	r = NULL;
	first = prev = rel = NULL;
	pos = 0;
	size = sec_size(sec);

	while(pos < size) {
		r = (Elf_Rel *) (sec_content(sec) + pos);

		rel = (reloc *) calloc(sizeof(reloc), 1);

		if (prev == NULL) {
			first = rel;
		} else {
			prev->next = rel;
		}

		info = reloc_info(r, r_info);

		rel->type = ELF(is64) ? ELF64_R_TYPE(info) : ELF32_R_TYPE(info);
		rel->offset = reloc_info(r, r_offset);
		rel->s_index = ELF(is64) ? ELF64_R_SYM(info) : ELF32_R_SYM(info);

		// TODO: Retrieve the addend, which is embedded into the instruction

		// link symbol to relocation entry, however they are not available yet
		// so it is needed to be done in a future pass

		hnotice(2, "Relocation type %d refers symbol %d at offset <%#08llx>\n",
			rel->type, rel->s_index, rel->offset);

		prev = rel;
		pos += (ELF(is64) ? sizeof(Elf64_Rel) : sizeof(Elf32_Rel));
	}

	// Adds the section to the program map
	add_section(SECTION_RELOC, sec, first, NULL);
	add_section(SECTION_RELOC, sec, first, &relocs);

	hsuccess();
}



static void elf_rela_section(int sec) {
	Elf_Rela *r;
	reloc *first, *prev, *rel;

	size_t pos, size;
	unsigned long long info;

	r = NULL;
	first = prev = rel = NULL;
	pos = 0;
	size = sec_size(sec);

	while(pos < size) {
		r = (Elf_Rela *) (sec_content(sec) + pos);

		rel = (reloc *) calloc(sizeof(reloc), 1);

		if (prev == NULL) {
			first = rel;
		} else {
			prev->next = rel;
		}

		info = reloc_info(r, r_info);

		rel->type = ELF(is64) ? ELF64_R_TYPE(info) : ELF32_R_TYPE(info);
		rel->offset = reloc_info(r, r_offset);
		rel->s_index = ELF(is64) ? ELF64_R_SYM(info) : ELF32_R_SYM(info);
		rel->addend = reloc_info(r, r_addend);

		// link symbol to relocation entry, however they are not available yet
		// so it is needed to be done in a future pass

		hnotice(2, "Relocation type %d refers symbol %d at offset <%#08llx>\n",
			rel->type, rel->s_index, rel->offset);

		prev = rel;
		pos += (ELF(is64) ? sizeof(Elf64_Rela) : sizeof(Elf32_Rela));
	}

	// Adds the section to the program map
	add_section(SECTION_RELOC, sec, first, NULL);
	add_section(SECTION_RELOC, sec, first, &relocs);

	hsuccess();
}


static void elf_string_section(int sec) {
	unsigned int pos = 0;
	unsigned int size = sec_size(sec);

	unsigned char *name;
	char *stringtab = (char *)malloc(sizeof(char) * size);

	while(pos < size){

		name = (sec_content(sec) + pos);
		strcpy(stringtab + pos, (char *)name);
		hnotice(2, "%#08x: '%s'\n", pos, stringtab + pos);

		pos += (strlen((const char *)name) + 1);
	}

	// adds the section to the program
	add_section(SECTION_NAMES, sec, stringtab, NULL);	//TODO: is this needed?
	strings = stringtab;

	hsuccess();
}


/**
 * Creates a function descriptor by resolving symbol pointer to code section.
 * In this second pass the parser resolves instruction addresses into function objects dividing them
 * into the right instruction chain belonging to the symbol requested.
 *
 * @param sym Pointer to a 'symbol' descriptor, which represent the current symbol to be resolved into function.
 *
 * @param func Pointer to the 'function' descriptor to be filled. Must be previoulsy allocated.
 *
 */
static void split_function(symbol *sym, function *func) {
	section *sec;
	insn_info *instr, *first;

	// check if the type is really a function
	if(sym->type != SYMBOL_FUNCTION){
		hinternal();
	}

	bzero(func, sizeof(function));

	// surf the section list until the one whose index is 'secnum' is found
	sec = code;
	while(sec){
		if(sec->index == sym->secnum)
			break;
		sec = sec->next;
	}
	if(!sec)
		hinternal();

	// check if the section pointed to is actually a code text section
	if(sec->type != SECTION_CODE){
		hinternal();
	}

	// reaches the instruction pointed to by the symbol value
	first = instr = sec->payload;
	while(instr != NULL){
		if(instr->orig_addr == sym->position)
			break;
		instr = instr->next;
	}
	if(!instr)
		hinternal();

	first = instr;
	func->name = sym->name;
	func->insn = instr;
	func->orig_addr = sym->position;
	func->new_addr = func->orig_addr;

	// now, it is necessary to find the end of the function.
	// it's not possible to do it by finding the RET instruction, cause it can be used
	// in the middle of a function for optimization purpose, so we use the 'first' instruction
	// of the current function to brake the chain in reverse, but will be done in a future pass.
}


static reloc *find_reloc(section *sec, unsigned long offset) {
	section *relsec;
	reloc *rel;

	// Find the relocation section relative to 'sec'
	relsec = relocs;
	while(relsec) {
		if (!strncmp(".rela", sec_name(relsec->index), strlen(".rela"))
				&& !strcmp(sec_name(relsec->index) + strlen(".rela"), sec_name(sec->index))) {
		// if (!strncmp(".rela", relsec->name, strlen(".rela"))
		//     && !strcmp(relsec->name + strlen(".rela"), sec->name)) {
			break;
		}

		relsec = relsec->next;
	}

	if (!relsec) {
		hinternal();
	}

	rel = relsec->payload;

	// Start iterating over relocation entries until we find
	// the one that refers to address 'offset'
	while(rel) {
		if (rel->offset == offset) {
			break;
		}

		rel = rel->next;
	}

	return rel;
}


static void resolve_jump_table(function *func, insn_info *instr,
	unsigned int secnum, unsigned long addr, unsigned long long size, symbol *text) {

	insn_info *target;
	section *sec;
	symbol *rel;
	function *foo;

	linked_list relocs;
	ll_node *relnode;

	unsigned char *toname;

	unsigned int i;

	// sec = find_section(secnum);

	// if (!sec) {
	// 	hinternal();
	// }

	relocs.first = relocs.last = NULL;
	toname = (text == NULL) ? (unsigned char*)".text" : text->name;
	rel = NULL;

	find_relocations(symbols->payload, sec_name(secnum), toname, &relocs);

	// printf("%s %p\n", sec_name(secnum), addr);

	for (relnode = relocs.first; relnode; relnode = relnode->next) {
		rel = relnode->elem;

		// printf("%p %p\n", addr, rel->relocation.offset);

		if (rel->relocation.offset == addr) {
			break;
		}
	}

	if (rel == NULL) {
		return;
	}

	// rel = find_reloc(sec, addr);

	// if (!rel) {
	// 	// [SE] TODO: clone_rodata_relocation in handle-elf.c non riflette
	// 	// l'inserimento di nuove rilocazioni dentro la rispettiva struct section
	// 	// quindi al momento dell'instrumentazione (cioè a parsing già terminato)
	// 	// si incappa in questo branch
	// 	return;
	// }

	instr->jumptable.size = size;
	instr->jumptable.entry = malloc(sizeof(insn_info *) * size);

	// Keep parsing relocation entries until we reach the
	// boundary of the jump table
	i = 0;
	while(i < size && relnode) {
		rel = relnode->elem;

		if (IS_JUMPIND(instr)) {
			target = find_insn(func, rel->relocation.addend, true);

			set_jumptable_entry(instr, target, i);
		}

		else if (IS_CALLIND(instr)) {
			foo = rel->func;

			if (rel->relocation.addend) {
				// It doesn't make much sense to compute the address of a function
				// from the address of another function and a displacement, but we
				// handle that possibility anyway...
				// [SE] TODO: Questo branch è abbastanza inutile
				target = find_insn(NULL, foo->insn->orig_addr + rel->relocation.addend, true);

				if (!target) {
					hinternal();
				}
			} else {
				target = foo->insn;
			}

			// hnotice(4, "Call instruction at <%#08llx> linked to address <%#08llx> ('%s' + <%#08llx>)\n",
			// 	instr->orig_addr, instr->jumptable.insn[i]->orig_addr, foo->name, rel->addend);

			set_jumptable_entry(instr, target, i);
		}

		else {
			hinternal();
		}

		i = i + 1;
		relnode = relnode->next;
	}
}

// [SE] TODO: Funzione abbastanza rozzetta, da rivedere
static void *infer_jump_table(function *func, insn_info *instr, symbol *text) {
	insn_info *backinstr;
	symbol *sym;

	unsigned int secnum;
	unsigned long start;
	unsigned long long size;

	unsigned int i;

	backinstr = instr;
	start = size = secnum = 0;
	i = 0;


	// Code for indirect jumps (very very unreliable!)
	if (IS_JUMPIND(instr)) {
		bool start_found;
		bool size_found;

		start_found = size_found = false;

		// We keep searching for the start address and the size of the jump table
		backinstr = instr;
		while(backinstr && i < MAX_LOOKBEHIND && (!start_found || !size_found)) {

			if (!start_found && IS_MEMRD(backinstr)) {
				sym = backinstr->reference;

				// We make the reasonable assumption that case statement addresses are in .rodata
				if (sym && !strcmp(sym->name, ".rodata")) {
					secnum = sym->secnum;
					start = sym->relocation.addend;

					start_found = true;
				}
			}
			else if (!size_found && IS_CTRL(backinstr)) {

				switch(PROGRAM(insn_set)) {
				case X86_INSN:
					// [SE] TODO: Bisognerebbe beccare non semplicemente una CMP, ma quella
					// che ha come destinazione il registro utilizzato nella MOV precedente,
					// cioè quello impiegato come valore di indice per la jump table
					size = backinstr->i.x86.immed;
					break;

				default:
					size = 0;
				}

				size_found = true;

			}

			backinstr = backinstr->prev;
			i = i + 1;
		}

		// It doesn't make sense to have zero-sized jump tables, therefore
		// we assume that this check is safe enough...
		if (size > 0) {
			hnotice(6, "JT starting at %s + <%#08llx> and sized %u\n", sec_name(sym->secnum), start, size);

			// The immediate value of the previous CMP instruction is inclusive of
			// the last case statement, hence it must be increased by one if we
			// wish to use it as the size of the jump table
			size = size + 1;
			resolve_jump_table(func, instr, secnum, start, size, text);
		}

	}


	// Code for indirect calls (slightly more reliable)
	else if (IS_CALLIND(instr)) {
		function *foo;

		foo = NULL;
		backinstr = instr->prev;
		while(backinstr && i < MAX_LOOKBEHIND) {

			if (IS_MEMRD(backinstr) || IS_MEMWR(backinstr)) {
				sym = backinstr->reference;

				if (sym != NULL) {
					// Single function pointer
					if (sym->type == SYMBOL_FUNCTION) {
						foo = sym->func;
					}
					// Array of function pointers
					else if (sym->type == SYMBOL_VARIABLE) {
						secnum = sym->secnum;
						start = sym->position;
						size = sym->size / sizeof(char *);
					}
					else {
						backinstr = backinstr->prev;
						i = i + 1;
						continue;
					}

					break;
				}
			}

			backinstr = backinstr->prev;
			i = i + 1;
		}

		// Zero-sized call tables mean a single function pointer
		if (foo && size == 0) {
			hnotice(6, "Function pointer to %s\n", foo->name);

			set_jumpto_reference(instr, foo->insn);

		} else if (size) {
			hnotice(6, "Array named %s starting at %s + <%#08llx> and sized %u\n",
				sym->name, sec_name(sym->secnum), start, size);

			resolve_jump_table(NULL, instr, secnum, start, size, text);
		}

	}


	// Cannot handle any other kind of instruction
	else {
		hinternal();
	}
}


/**
 * Links jumps instructions to destination ones.
 * Provided a valid function's descriptors, it will look up for all the jump instructions
 * and will link them to their relative destination ones.
 *
 * @param func The pointer to a valid function's descriptors
 */

void link_jump_instructions(function *func, function *code_version, symbol *text) {
	insn_info *instr;	// Current instruction
	insn_info *dest;	// Destination one
	function *callee;	// Callee function
	symbol *sym;		// Callee function's symbol
	unsigned long long jmp_addr;	// Jump address

	hnotice(2, "Link jump and call instructions of function '%s'\n", func->name);

	// For each instruction, look for jump/call ones
	instr = func->insn;
	while(instr != NULL) {


		if(IS_JUMP(instr)) {
			// If the jump instruction has a reference, this means that a relocation has to be applied;
			// therefore looking for the target instruction is actually incorrect since it can't be found.
			if(instr->reference != NULL) {

				// Simply skip the instruction; the linker will be in charge to correctly handle it
				instr = instr->next;
				continue;
			}

			else if (IS_JUMPIND(instr)) {
				// [SE] If the instruction is an indirect jump, try to resolve its
				// associated jump table (currently only for switch-case statements)
				// NOTE: This is a very naive and loose algorithm that may fail
				// in several cases, and is not kitten-proof! Beware!

				infer_jump_table(func, instr, text);
			}

			else {
				// Provided a jump instruction, look for the destination address
				switch(PROGRAM(insn_set)) {
				case X86_INSN:
					jmp_addr = instr->orig_addr + instr->i.x86.insn_size + instr->i.x86.jump_dest;
					break;

				default:
					jmp_addr = -1;
				}

				dest = func->insn;
				while(dest) {
					if(dest->orig_addr == jmp_addr)
						break;

					dest = dest->next;
				}

				if(!dest) {
					hinternal();
				}

				// At this point 'dest' will point to the destination instruction relative to the jump 'instr'
				set_jumpto_reference(instr, dest);
			}

		}


		else if(IS_CALL(instr)) {

			if (IS_CALLIND(instr)) {
				// [SE] Handle indirect calls (tricky, uses the same naive algorithm
				// as for switch-case statements)
				infer_jump_table(func, instr, text);
			}

			else {
				// a CALL could be seen as a JUMP and could help in handling the embedded offset to local functions

				switch(PROGRAM(insn_set)) {
					case X86_INSN:
						jmp_addr = instr->i.x86.jump_dest;
						break;

					default:
						jmp_addr = 0;
				}

				if(jmp_addr != 0) {
					// If the CALL has a non-null embedded offset, it is a call to a local function and
					// the format is the same as a jump. The offset is interpreted, the called function
					// retrieved and the instruction is translated into a zero'd CALL with an associated
					// relocation entry.

					// XXX: credo che fosse scorretto nel caso delle funzioni locali, infatti non trovava la funzione
					jmp_addr += instr->orig_addr + instr->size;

					hnotice(4, "Call to a local function at <%#08llx> detected\n", jmp_addr);

					// look for the relative function called
					callee = code_version;
					while(callee) {

						if(callee->orig_addr == jmp_addr)
							break;

						callee = callee->next;
					}

					// mhhh, something goes wrong i guess...
					if(!callee) {
						hinternal();
					}

					// CALL instruction embedded offset must be reinitialized to zero
					switch(PROGRAM(insn_set)) {
						case X86_INSN:
							memset(instr->i.x86.insn + 1, 0, (instr->size - instr->opcode_size));
							break;
					}

				}

				else {
					// [SE] If the CALL instruction has no embedded offset, it is already associated with a relocation.
					// We must check whether is it a CALL to a local function or not, and act accordingly.

					jmp_addr = instr->reference->position;

					// It means the function is defined elsewhere (i.e. in a different file object)
					// meaning that the linker will be in charge to correctly handle it
					if(instr->reference->size == 0) {
						instr = instr->next;
						continue;
					}

					callee = code_version;
					while(callee) {

						if(callee->orig_addr == jmp_addr)
							break;

						callee = callee->next;
					}
				}

				if (callee) {
					// CALL to local function detected, augment the intermediate representation
					// with the appropriate linking between instructions.
					hnotice(4, "Callee function '%s' at <%#08llx> found\n", callee->name, callee->orig_addr);

					// At this point 'func' will point to the destination function relative to the call;
					// the only thing we have to do is to add the reference to the relative function's symbol
					// so that, in the future emit step, the code will automatically retrieve the correct final
					// address of the relocation. In such a way we threat local function calls as relocation enties.
					sym = callee->symbol;

					if (instr->reference == NULL || PROGRAM(version) > 0) {
						// The instruction object will be bound to the proper symbol
						instruction_rela_node(sym, instr, RELOCATE_RELATIVE_32);
					}

					set_jumpto_reference(instr, callee->insn);
				}

			}

		}


		instr = instr->next;
	}

}


/**
 * Second phase parser which resolves symbols.
 * Resolves symbols by retrieving its type and calling the relative function which handle them correctly.
 * At the end of the phase, instructions within the code section are translated into function objects
 * and returned into the global variable 'functions', whereas variables' values are stored into a data array.
 */
static void resolve_symbols(void) {
	// This is the second pass needed to divide the instruction chain into function objects.
	// Symbols will resolved and linked to the relative function descriptor object.

	symbol *sym;			// Current symbol to be resolved
	function *head, *curr, *prev, *func;	// Function pointers

	sym = symbols->payload;

	head = malloc(sizeof(function));
	bzero(head, sizeof(function));

	hnotice(1, "Resolving symbols...\n");

	// For each symbol registered, resolve it
	while(sym) {

		switch(sym->type){
		case SYMBOL_FUNCTION:
			func = malloc(sizeof(function));
			if(func == NULL) {
				herror(true, "Out of memory!\n");
			}

			split_function(sym, func);
			func->symbol = sym;
			sym->func = func;
			sym->sec = find_section(sym->secnum);

			hnotice(2, "Function '%s' (%d bytes long) :: <%#08llx>\n", sym->name, sym->size, func->orig_addr);

			curr = prev = head;
			while(curr) {
				if(func->orig_addr <= curr->orig_addr) {
				//	func->next = prev->next;
				//	prev->next = func;
					break;
				}

				prev = curr;
				curr = curr->next;
			}
			func->next = prev->next;
			prev->next = func;

			break;

		case SYMBOL_VARIABLE:
			hnotice(2, "Variable '%s' (%d bytes long) :: %lld (%s)\n",
				sym->name, sym->size, sym->position, sym->secnum == SHN_COMMON ? "COM" : sec_name(sym->secnum));

			if (sym->secnum != SHN_COMMON) {
				// [SE] The symbol's initial value is copied into a private buffer,
				// then later flushed to the new ELF file during the emit step
				sym->initial = calloc(sym->size, 1);
				sym->sec = find_section(sym->secnum);

				if (sec_type(sym->secnum) & SHT_PROGBITS) {
					memcpy(sym->initial, sec_content(sym->secnum) + sym->position, sym->size);
				}
				// [/SE]

				hdump(5, sym->name, sym->initial, sym->size);
			}
			break;

		case SYMBOL_TLS:
			hnotice(2, "TLS symbol pointing to section %d (%s)\n", sym->secnum, sec_name(sym->secnum));

			sym->initial = calloc(sym->size, 1);
			sym->sec = find_section(sym->secnum);

			if (sec_type(sym->secnum) & SHT_PROGBITS) {
				memcpy(sym->initial, sec_content(sym->secnum) + sym->position, sym->size);
			}

			hdump(5, sym->name, sym->initial, sym->size);
			break;

		case SYMBOL_UNDEF:
			hnotice(2, "Undefined symbol '%s' (%d bytes long)\n", sym->name, sym->size);
			break;

		case SYMBOL_SECTION:
			hnotice(2, "Section symbol pointing to section %d (%s)\n", sym->secnum, sec_name(sym->secnum));
			sym->name = (unsigned char *)sec_name(sym->secnum);
			sym->size = sec_size(sym->secnum);
			sym->sec = find_section(sym->secnum);
			break;

		case SYMBOL_FILE:
			hnotice(2, "Filename's symbol\n");
			break;

		default:
			hnotice(2, "Unknown type for symbol '%s', skipped\n", sym->name);
		}

		sym = sym->next;
	}

	// save function list
	functions = head->next;
	free(head);

	// Link JUMP instructions and break the instruction chain
	func = functions;
	while(func) {
		// breaks the instructions chain
		if(func->insn->prev) {
			func->insn->prev->next = NULL;
		}
		func->insn->prev = NULL;

		func = func->next;
	}

	hsuccess();
}


/**
 * Third phase which resolves relocation.
 * Resolves each relocation entry stored in previous phase, by looking for each symbol name and binding them
 * to the relative reference. In particular, in functions each instruction descriptor handles a 'reference'
 * void * pointer which can represent either a variable or a call instruction to a specific address.
 * In case of a reference to an 'undefined' symbol, which probably means an external library function, a
 * temporary NULL pointer is set.
 *
 * If the symbol or the code address referenced to by the relocation entry was not found a warning is issued,
 * but the parsing goes on.
 *
 * Note: Requires that symbols have already been resolved!
 */
static void resolve_relocation(void) {
	reloc *rel;
	function *func;
	insn_info *instr;
	symbol *sym, *sym_2;
	section *sec;
	int target, flags;
	unsigned long long offset;

	hnotice(1, "Resolving relocation entries...\n");

	// Part of this work is already done in the previous step performed by 'link_jump_instructions' function

	// Get the list of parsed relocation sections
	sec = relocs;

	// For each relocation section
	while(sec) {

		hnotice(2, "Parsing relocation section '%s'\n\n", sec_name(sec->index));

		// Retrieve relocation's metadata
		target = sec_field(sec->index, sh_info);
		flags = sec_field(target, sh_flags);


		// Retrieve the payload and cycles on each relocation entry
		rel = sec->payload;
		while(rel) {

			if (rel->s_index == 0) {
				// We should never have relocations toward STN_UNDEF
				hinternal();
			}

			// We look for the symbol pointed to by the relocation's 'info' field
			hnotice(2, "Parsing relocation at '%s' + <%#08llx> + %d to [%d]\n",
				sec_name(sec->index), rel->offset, rel->addend, rel->s_index);

			sym = symbols->payload;
			while(sym) {
				if(sym->index == rel->s_index && rel->s_index){
					hnotice(3, "Symbol found: '%s' [%u] [%s]\n", sym->name, rel->s_index,
							sym->type == SYMBOL_FUNCTION ? "function" :
									sym->type == SYMBOL_VARIABLE ? "variable" :
											sym->type == SYMBOL_SECTION ? "section" : "undefined");
					break;
				}
				sym = sym->next;
			}

			if(!sym){
				hnotice(3, "Symbol [%u] not found!\n\n", rel->s_index);
				hinternal();
				// rel = rel->next;
				// continue;
			}

			rel->symbol = sym;

			if(flags & SHF_EXECINSTR) {
				// the relocation applies to an instruction
				// hnotice(2, "Looking up for address <%#08llx>\n", rel->offset);

				// Search in the function list the one containing the right instruction.
				// This is simply done by looking for the function whose starting offset
				// is the closest address to one relocation refers to.
				func = functions;
				instr = NULL;
				offset = (unsigned long long)rel->offset;
				while(func){
					if(offset > func->orig_addr && offset < (func->orig_addr + func->symbol->size)){
						break;
					}

					func = func->next;
				}

				if(func) {
					hnotice(3, "Relocation is found to be in function '%s' at <%#08llx>\n", func->name, func->orig_addr);
					instr = func->insn;
				} else {
					hinternal();
				}

				// At this point 'instr' (should) contains the first instruction of the
				// correct function to which apply the relocation.
				// Now we have to look for the right instruction pointed to by the offset.
				// in order to do that is sufficient to look up for the instructions whose
				// address is the closest to the one relocation refers to.
				// Note that '>' is because the relocation actually does not refer to
				// the instruction's address itself, but it is shifted by the opcode size.
				while(instr->next) {
					if(instr->next->orig_addr > (unsigned long long)rel->offset){
						break;
					}

					instr = instr->next;
				}

				// if instr is NULL, uuh...something is going wrong!
				if(instr != NULL) {
					hnotice(3, "Instruction pointed to by relocation: <%#08llx> '%s'\n",
						instr->orig_addr, instr->i.x86.mnemonic);


					// TODO: now there is the create_rela_node functions, use it!
					// Check for relocation duplicates
					sym_2 = symbol_check_shared(sym);
					sym_2->relocation.addend = rel->addend;
					sym_2->relocation.offset = rel->offset;
					sym_2->relocation.type = rel->type;
					sym_2->relocation.secname = (unsigned char *)sec_name(target);

					// The instruction object will be bound to the proper symbol.
					// This reference is read by the specific machine code emitter
					// that is in charge to proper handle the relocation.
					sym_2->relocation.ref_insn = instr;
					instr->reference = sym_2;

					hnotice(2, "Added symbol reference to '%s' + <%#08llx> + %d\n\n",
						sym_2->relocation.secname, rel->offset, rel->addend);

				} else {
					herror(true, "Relocation cannot be applied, reference not found\n\n");
				}
			} else {
				// If the section's flags are not EXEC_INSTR, then this means that
				// the relocation does not apply to an instruction but to another symbol;
				// e.g. a SECTION symbol, in case of generic references (.data, .bss, .rodata)

				// If we are here, the relocation is SECTION->SECTION, otherwise
				// an instruction would be found in the previous branch.

				// hnotice(3, "Looking up for address <%#08llx>\n", rel->addend);

				// Search in the function list the one containing the right instruction.
				// This is simply done by looking for the function whose starting offset
				// is the closest address to one relocation refers to.
				func = functions;
				instr = NULL;
				while(func->next){
					if(func->next->orig_addr > (unsigned long long)rel->addend){
						break;
					}

					func = func->next;
				}

				if(func) {
					hnotice(3, "Relocation is found to be in function '%s' at <%#08llx>\n", func->name, func->orig_addr);
					instr = func->insn;
				}

				// At this point 'instr' (should) contains the first instruction of the
				// correct function to which apply the relocation.
				// Now we have to look for the right instruction pointed to by the offset.
				// in order to do that is sufficient to look up for the instructions whose
				// address is the closest to the one relocation refers to.
				// Note that '>' is because the relocation actually does not refer to
				// the instruction's address itself, but it is shifted by the opcode size.
				while(instr) {
					if(instr->orig_addr == (unsigned long long)rel->addend){
						break;
					}

					instr = instr->next;
				}

				// TODO: now there is the create_rela_node function, so use it!
				// Check for relocation duplicates
				sym_2 = symbol_check_shared(sym);
				sym_2->relocation.addend = rel->addend;
				sym_2->relocation.offset = rel->offset;
				sym_2->relocation.type = rel->type;
				sym_2->relocation.secname = (unsigned char *)sec_name(target);

				// if instr is NULL, uuh...something is going wrong!
				if(instr != NULL) {
					hnotice(3, "Instruction pointed to by relocation: <%#08llx> '%s'\n", instr->orig_addr, instr->i.x86.mnemonic);

					sym_2->relocation.ref_insn = instr;
					instr->pointedby = sym_2;
				}

				hnotice(2, "Added symbol reference to '%s' + <%#08llx> + %d\n\n",
					sym_2->relocation.secname, rel->offset, rel->addend);
			}

			rel = rel->next;
		}

		sec = sec->next;
	}

	hsuccess();
}


static void resolve_jumps(void) {
	function *func;

	// links the jump instructions
	func = functions;
	while(func) {
		link_jump_instructions(func, functions, NULL);

		func = func->next;
	}
}


// [SE]
static void resolve_blocks(void) {
	hnotice(1, "Resolving blocks...\n");

	blocks = block_graph_create(functions);

	// We spit out some boring textual representation of both the balanced tree
	// and the final flow graph
	if (config.verbose > 5) {
		block_tree_dump("treedump.txt", "w+");
		block_graph_dump(functions, "graphdump.txt", "w+");
	}

	hsuccess();
}


void elf_create_map(void) {
	unsigned int size;
	unsigned int sec;

	// Reserve space and load ELF in memory
	fseek(ELF(pointer), 0L, SEEK_END);
	size = ftell(ELF(pointer));
	rewind(ELF(pointer));
	ELF(data) = malloc(size * sizeof(unsigned char));
	if(fread(ELF(data), 1, size, ELF(pointer)) != size) {
		herror(true, "Unable to correctly load the ELF file\n");
	}
	rewind(ELF(pointer));

	// Keep track of the header
	ELF(hdr) = (Elf_Hdr *)ELF(data);

	// Where is the section header?
	if(ELF(is64))
		ELF(sec_hdr) = (Section_Hdr *)(ELF(data) + ELF(hdr)->header64.e_shoff);
	else
		ELF(sec_hdr) = (Section_Hdr *)(ELF(data) + ELF(hdr)->header32.e_shoff);

	// How many sections are in the ELF?
	if(ELF(is64))
		ELF(secnum) = ELF(hdr)->header64.e_shnum;
	else
		ELF(secnum) = ELF(hdr)->header32.e_shnum;

	hnotice(1, "Found %u sections...\n", ELF(secnum));

	// Scan ELF Sections and convert/parse them (if any to be)
	for(sec = 0; sec < ELF(secnum); sec++) {

		hnotice(1, "Parsing section %d: '%s' (%d bytes long, offset %#08lx)\n",
				sec, sec_name(sec), sec_size(sec), sec_field(sec, sh_offset));

		switch(sec_type(sec)) {

		case SHT_PROGBITS:
			if(sec_test_flag(sec, SHF_EXECINSTR)) {
				elf_code_section(sec);
			} else {
				// Qui è sicuramente una sezione data
				if(sec_test_flag(sec, SHF_TLS)) {
					elf_tls_section(sec);
				} else {
					elf_raw_section(sec);
				}
			}
			break;

		case SHT_SYMTAB:
			elf_symbol_section(sec);
			break;

		case SHT_NOBITS:
			if(sec_test_flag(sec, SHF_TLS)) {
				elf_tls_section(sec);
			} else {
				elf_raw_section(sec);
			}
			break;

		case SHT_REL:
			elf_rel_section(sec);
			break;

		case SHT_RELA:
			if(str_prefix(sec_name(sec), ".rela.text"))
				elf_rela_section(sec);
			else if(!strcmp(sec_name(sec), ".rela.data"))
				elf_rela_section(sec);
			else if(!strcmp(sec_name(sec), ".rela.rodata"))
				elf_rela_section(sec);
			else if(!strcmp(sec_name(sec), ".rela.bss"))
				elf_rela_section(sec);
			break;

		case SHT_STRTAB:
			elf_string_section(sec);
			break;

		case SHT_HASH:
		case SHT_DYNAMIC:
		case SHT_DYNSYM:
			elf_raw_section(sec);
			break;
		}
	}

	// Ultimates the binary representation
	resolve_symbols();
	resolve_relocation();
	resolve_jumps();
	resolve_blocks();

	// Updates the binary representation's pointers
	PROGRAM(code) = functions;
	PROGRAM(symbols) = symbols->payload;
	PROGRAM(v_code)[0] = functions;
	PROGRAM(rawdata) = 0;
	PROGRAM(versions)++;
	PROGRAM(blocks)[0] = blocks;

	hnotice(1, "ELF parsing terminated\n\n");
	hsuccess();
}








int elf_instruction_set(void) {
	Elf32_Ehdr hdr; // Headers are same sized. Assuming its 32 bits...
	int insn_set = UNRECOG_INSN;

	hnotice(1, "Determining instruction set... ");

	// Load ELF Header
	if(fread(&hdr, 1, sizeof(Elf32_Ehdr), ELF(pointer)) != sizeof(Elf32_Ehdr)) {
		herror(true, "An error occurred while reading program header\n");
	}

	// Switch on proper field
	switch(hdr.e_machine) {

		case EM_386:
		case EM_X86_64:
			insn_set = X86_INSN;
			break;
		}


	if(insn_set == UNRECOG_INSN) {
		hfail();
	} else {
		hsuccess();
	}

	rewind(ELF(pointer));

	return insn_set;
}




bool is_elf(char *path) {
	Elf32_Ehdr hdr; // Headers are same sized. Assuming its 32 bits...

	hnotice(1, "Checking whether '%s' is an ELF executable...", path);

	// Try to oper the file
	ELF(pointer) = fopen(path, "r+");
	if(ELF(pointer) == NULL) {
		herror(true, "Unable to open '%s' for reading\n", path);
	}


	// Load ELF Header
	if(fread(&hdr, 1, sizeof(Elf32_Ehdr), ELF(pointer)) != sizeof(Elf32_Ehdr)) {
		herror(true, "An error occurred while reading program header\n");
	}

	// Is it a valid ELF?!
	if(hdr.e_ident[EI_MAG0] != ELFMAG0 ||
			hdr.e_ident[EI_MAG1] != ELFMAG1 ||
			hdr.e_ident[EI_MAG2] != ELFMAG2 ||
			hdr.e_ident[EI_MAG3] != ELFMAG3) {
		fclose(ELF(pointer));
		hfail();
		return false;
	}

	// We cannot deal with executables, only with relocatable objects
	if(hdr.e_type != ET_REL) {
		herror(true, "Can analyze only relocatable ELF objects\n");
	}

	// Is the current ELF 32- or 64-bits?
	switch(hdr.e_ident[EI_CLASS]) {
	case ELFCLASS32:
		ELF(is64) = false;
		break;
	case ELFCLASS64:
		ELF(is64) = true;
		break;
	default:
		herror(true, "Invalid ELF class\n");
	}

	// Reset the file descriptor
	rewind(ELF(pointer));

	hsuccess();
	return true;
}

