#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "MemManager.h"
#include "global.h"

using namespace std;

// global vars -------------------------------------------------------------------------------
int PROCESS_COUNT = 0;
int FRAME_COUNT = 0;
int VIRTUAL_SPACE = 64;
bool O = false, P = false, F = false, S = false, x=false;
unsigned long instruct_count = 0;

vector<Frame*> frame_table;
vector<Frame*> free_frames_list;
vector<Process*> process_list;

typedef struct Instr_Pair {
	char op;
	int num;
} instruction;
vector<instruction*> instruction_list;

vector<int> random_vect;
unsigned int random_offset = 0;

Pager *pager = nullptr;

// functions ---------------------------------------------------------------------------------

// method to skip through input file lines starting with '#'
void find_next_line(ifstream &file, string &line_buffer) {
	if (file.eof()) {
		printf("WARNING: EOF reached!\n");
		return;
	}

	getline(file, line_buffer);

	while (!file.eof()) {
		if (line_buffer.at(0) == '#') {
			getline(file, line_buffer);
			continue;
		}
		else {
			return;  // stop searching, we've found next line!
		}
	}
}

// method to read the input file and populate process list and instruction list
void read_input_file(string filename) {
	ifstream file;
	file.open(filename.c_str());

	string line_buffer;

	find_next_line(file, line_buffer);

	PROCESS_COUNT = atoi(line_buffer.c_str());

	// read each process' address space (i.e. VMAs)
	int vma_count = 0;
	for (int i = 0; i < PROCESS_COUNT; i++) {
		find_next_line(file, line_buffer);
		vma_count = atoi(line_buffer.c_str());

		Process *p = new Process();
		process_list.push_back(p);
		p->id = i;

		for (int j = 0; j < vma_count; j++) {
			VMA *vma = new VMA();
			find_next_line(file, line_buffer);
			sscanf(line_buffer.c_str(), "%d %d %d %d", &vma->start_vpage, &vma->end_vpage, &vma->write_protect, &vma->filemapped);

			p->vma_list.push_back(vma);
		}
	}

	// read instruction list
	while (!file.eof()) {
		char c = ' ';
		int n = 0;
		find_next_line(file, line_buffer);
		sscanf(line_buffer.c_str(), "%c %d", &c, &n);

		if (c==' ') { // break if op is empty 
			break;
		}

		instruction *instr = new instruction;
		instr->op = c;
		instr->num = n;
		instruction_list.push_back(instr);
	}

	file.close();
}

// method to read random file. Indexing follows	[0, size)
void read_rand_file(string filename) {
	ifstream file;
	string line = "";
	file.open(filename.c_str());
	if (file.is_open()) {
		while (getline(file, line)) {
			random_vect.push_back(atoi(line.c_str()));	// store random numbers in vector
		}
	}
	else printf("unable to reade rfile");
	file.close();
}

// method to get next random number. [a,b)
int myrandom(int mod) {
	if (random_offset == random_vect.size())
		random_offset = 0;
	return (random_vect[++random_offset] % mod);
}

// method to get next instruction
instruction *get_next_instruction() {
	if (instruction_list.empty()) {
		printf("PROBLEM: instruction list is empty!\n");
		return nullptr;
	}
	instruction *in = instruction_list.front();
	instruction_list.erase(instruction_list.begin());

	return in;
}


// method to get a frame from free list
Frame* allocate_frame_from_free_list() {
	if (free_frames_list.empty()) {
		return nullptr;
	}

	Frame *f = free_frames_list.front();
	f->is_free = false;
	free_frames_list.erase(free_frames_list.begin());

	return f;
}

// method to assign new frame
Frame* get_frame() {
	Frame *f = allocate_frame_from_free_list();
	// if no frames in free list, choose a victim frame
	if (f == nullptr) {
		f = pager->select_victim_frame();  // call pager algorithm

		PTE *prev_pte = f->pte;

		prev_pte->present = false;  // set previous pte's present bit to false

		// for pstats
		Process *pstat = process_list.at(f->process_id);

		if (O) { printf(" UNMAP %d:%d\n", f->process_id, f->process_vpage); }
		pstat->unmaps++;

		// if frame had been modified...
		if (prev_pte->modified == true) {
			if (prev_pte->filemapped == true) {  // filemapped
				if (O) { printf(" FOUT\n"); }
				pstat->fouts++;
			}
			else {  // to swap device
				if (O) { printf(" OUT\n"); }
				pstat->outs++;
				// set pageout to true? Also account for FIN?
				prev_pte->pageout = true;
			}
			prev_pte->referenced = false;
			prev_pte->modified = false; // reset modified bit here?
		}

	}

	return f;
}

// main ------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {

	string infile;
	string rfile;
	char algorithm_char = ' ';

	// read the inputs
	int i = 1;
	while (i < argc - 1) {
		if (argv[i][0] == '-') {
			switch (argv[i][1])
			{
			case 'a': // algorithm choice
			{
				algorithm_char = argv[i][2];
				break;
			}
			case 'o': // options
			{
				for (unsigned int j = 2; j < strlen(argv[i]); j++) {
					char option = argv[i][j];
					if (option == 'O')
						O = true;
					if (option == 'P')
						P = true;
					if (option == 'F')
						F = true;
					if (option == 'S')
						S = true;
					if (option == 'x')
						x = true;
				}
				break;
			}
			case 'f': // frame size
			{
				string s = argv[i];
				string sub = s.substr(2);
				FRAME_COUNT = atoi(sub.c_str());
				break;
			}
			default:
				break;
			}
		}
		else{  // files
			infile = argv[i];
			i++;
			rfile = argv[i];
		}

		i++;
	}

	read_input_file(infile);
	read_rand_file(rfile);

	// create frame table
	for (int i = 0; i < FRAME_COUNT; i++) {
		Frame *f = new Frame();
		f->id = i;
		f->is_free = true;
		frame_table.push_back(f);
		free_frames_list.push_back(f);
	}

	// create Pager
	switch (algorithm_char)
	{
	case 'f': pager = new FIFO();
		break;
	case 'r': pager = new Random();
		break;
	case 'c': pager = new Clock();
		break;
	case 'e': pager = new NRU();
		break;
	case 'a': pager = new Aging();
		break;
	case 'w': pager = new Working_Set();
		break;
	default:
		break;
	}
	
	pager->frame_table = frame_table;

	// BEGIN SIMULATION

	Process *current_process = nullptr;
	unsigned long ctx_switches = 0;
	unsigned long process_exits = 0;
	int accesses = 0;

	while (!instruction_list.empty()) {
		instruction *instr = get_next_instruction();
		char op = instr->op;
		int vpage = instr->num;

		if (O) { printf("%lu: ==> %c %d\n", instruct_count, op, vpage); }
		instruct_count++;

		// if context change, move on to next instr
		if (op == 'c') {
			current_process = process_list.at(vpage);
			ctx_switches++;
			continue;
		}
		// if process exiting
		else if (op == 'e') {
			process_exits++;
			printf("EXIT current process %d\n", vpage);

			for (int i = 0; i < VIRTUAL_SPACE; i++) {
				PTE *pte = current_process->page_table[i];
				// remove each valid pte of process from frame table in the order 0...63
				if (pte->present == true) {
					if (O) { printf(" UNMAP %d:%d\n", current_process->id, i); }
					current_process->unmaps++;

					Frame *freeing_frame = frame_table.at(pte->frame);
					freeing_frame->free_frame();
					free_frames_list.push_back(freeing_frame);
				}
				// FOUT modified entries
				if (pte->modified == true) {
					if (pte->filemapped == true) {
						if (O) { printf(" FOUT\n"); }
						current_process->fouts++;
					}
				}

				pte->reset_bits(); // reset all bit to 0
			}

			current_process = nullptr;
			continue;
		}

		accesses++;

		PTE *pte = current_process->page_table[vpage];

		// check if vpage is accessible in process' VMA
		if (pte->vma_checked == false) {
			if (current_process->vpage_is_in_vma(vpage)) {
				// set the pte's filemapped bit
				if (current_process->is_vpage_filemapped(vpage)) {
					pte->filemapped = true;
				}
				// set the pte's write-protected bit
				if (current_process->is_vpage_write_protected(vpage)) {
					pte->write_protect = true;
				}
				pte->vma_checked = true;
			}
			else {
				printf(" SEGV\n");
				current_process->segv++;
				continue;
			}
		}

		Frame *frame = nullptr;
		// if pte is not valid, seek new frame
		if (!pte->present) {
			frame = get_frame();

			// check if vpage is file mapped
			if (pte->filemapped == true) {
				// check to see if pte was filemapped
				if (O) { printf(" FIN\n"); }
				current_process->fins++;
			}

			// if current vpage was paged out, IN from swap device
			else if (pte->pageout == true) {
				if (O) { printf(" IN\n"); }
				current_process->ins++;
				//pte->pageout = false;  // update bit?
			}
			else {
				if (O) { printf(" ZERO\n"); }
				current_process->zeros++;
			}

			frame->pte = pte;  // frame now points to new pte
			frame->bitcounter = 0;  // reset bit counter for new allocated frame
			frame->time_last_used = instruct_count;

			if (O) { printf(" MAP %d\n", frame->id); }

			current_process->maps++;

			pte->present = true;
			pte->frame = frame->id;  // set the pte's frame reference index

		}
		else {  // if pte is valid, access frame through process' page table
			frame = frame_table.at(current_process->page_table[vpage]->frame);
		}

		// update the R/M PTE bits based on instruction
		if (op == 'r') {
			pte->referenced = true;
		}
		else if (op == 'w') {
			pte->referenced = true;
			if (pte->write_protect == true) {
				printf(" SEGPROT\n");
				current_process->segprot++;
			}
			else {
				pte->modified = true;
			}
		}

		// update frame table with pager algorithm
		frame->is_mapped = true; // for option print
		frame->process_id = current_process->id;
		frame->process_vpage = vpage;

		pager->update(frame);

		if (x) {  // for debugging
			current_process->print_table();
			printf("\n");
		}

	} // end-while

	// print P (pagetable )option
	if (P) {
		for (unsigned int i = 0; i < process_list.size(); i++) {
			Process *p = process_list.at(i);
			p->print_table();

			printf("\n");
		}
	}

	// print F (frame table) option
	if (F) {
		printf("FT: ");
		for (unsigned int i = 0; i < frame_table.size(); i++) {
			Frame *f = frame_table.at(i);
			if (f->is_mapped)
				printf("%d:%d ", f->process_id, f->process_vpage);
			else
				printf("* ");
		}
	}
	printf("\n");

	// print S (summary) option
	unsigned long long cost = 0;
	if (S) {
		for (unsigned int i = 0; i < process_list.size(); i++) {
			Process *p = process_list.at(i);
			printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
				p->id, p->unmaps, p->maps, p->ins, p->outs, p->fins, p->fouts, p->zeros, p->segv, p->segprot);

			cost += 400 * p->maps; cost += 400 * p->unmaps;
			cost += 3000 * p->ins; cost += 3000 * p->outs;
			cost += 2500 * p->fins; cost += 2500 * p->fouts;
			cost += 150 * p->zeros;
			cost += 240 * p->segv; cost += 300 * p->segprot;
		}
		cost += 121 * ctx_switches;
		cost += 175 * process_exits;
		cost += accesses;

		// print total cost
		printf("TOTALCOST %lu %lu %lu %llu\n", instruct_count, ctx_switches, process_exits, cost);
	}

	

	system("pause");
}