#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <string>
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
vector<Frame*> frame_table;
vector<Frame*> free_frames_list;
vector<Process*> process_list;

typedef struct Instr_Pair {
	char op;
	int num;
} instruction;
vector<instruction*> instruction_list;

vector<int> random_vect;
int random_offset = 0;

Pager *mmu = nullptr;

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

// method to get next random number
int myrandom(int mod) {
	if (random_offset == random_vect.size() - 1)
		random_offset = 0;
	return 1 + (random_vect[random_offset++] % mod);
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
	free_frames_list.erase(free_frames_list.begin());

	return f;
}

// method to assign new frame
Frame* get_frame() {
	Frame *f = allocate_frame_from_free_list();
	if (f == nullptr) {
		f = mmu->select_victim_frame();
		return f;
	}
}

// main ------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {

	string infile;
	string rfile;
	char algorithm_char = ' ';
	bool O = false, P = false, F = false, S = false;

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
				for (int j = 2; j < strlen(argv[i]); j++) {
					char option = argv[i][j];
					if (option == 'O')
						O = true;
					if (option == 'P')
						P = true;
					if (option == 'F')
						F = true;
					if (option == 'S')
						S = true;
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

	// create MMU
	switch (algorithm_char)
	{
	case 'f': mmu = new FIFO();
		break;
	case 'r':
		break;
	case 'c':
		break;
	case 'e':
		break;
	case 'a':
		break;
	case 'w':
		break;
	default:
		break;
	}

	// BEGIN SIMULATION

	Process *current_process = nullptr;

	while (!instruction_list.empty()) {
		instruction *instr = get_next_instruction();
		char op = instr->op;
		int vpage = instr->num;

		// if context change, move on to next instr
		if (op == 'c') {
			current_process = process_list.at(vpage);
			continue;
		}
		// if process exiting
		else if (op == 'e') {
			continue;
		}

		PTE *pte = current_process->page_table[vpage];

		if (!pte->present) {
			// page fault exception
			Frame *newframe = get_frame();

			// figure out what to do with old frame if it was mapped
			if (newframe->pte->pageout == true) {
				// populate frame with stored data
				if (newframe->is_free) {

				}
				else {

				}
				newframe->pte->pageout = false;
			}


		}

		// update the R/M PTE bits based on operation
	}
	

	system("pause");
}