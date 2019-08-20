#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <bitset>
#include <climits>
#include "global.h"

using namespace std;

class PTE {
public:
	unsigned int present : 1;
	unsigned int write_protect : 1;
	unsigned int modified : 1;
	unsigned int referenced : 1;
	unsigned int pageout : 1;
	unsigned int filemapped : 1;
	unsigned int frame : 7;  // we can have at most 128 frames

	unsigned int vma_checked : 1;  // keep track of if vpage is in vma

	PTE();
	void reset_bits();
};

class Frame {
public:
	int id;
	bool is_free;
	bool is_mapped;
	int process_id;
	int process_vpage;
	PTE* pte;  // each phys. frame is mapped to its current PTE

	bitset<32> bitcounter;
	int time_last_used;

	Frame();
	void free_frame();
};

class VMA { // each process' VMAs
public:
	int start_vpage;
	int end_vpage;
	int write_protect;
	int filemapped;

	VMA();
};

class Process {
public:
	int id;
	PTE *page_table[64]; // each process has 64 vpages
	vector<VMA*> vma_list;
	unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot; // stat counts

	Process();
	VMA* vpage_is_in_vma(int vpage);
	bool is_vpage_filemapped(int vpage);
	bool is_vpage_write_protected(int vpage);
	void print_table();
};

// MMU --------------------------------------------------------
class Pager {
protected:
	vector<Frame*> queue;
	unsigned int hand;
public:
	Pager();  // superclass construtor (from Derived's pov) to be called. Just initiates hand = 0
	vector<Frame*> frame_table;
	virtual Frame* select_victim_frame() = 0;
	virtual void update(Frame *f);  // can be overwritten.
};

class FIFO : public Pager {
public:
	FIFO();
	Frame* select_victim_frame();
};


class Random : public Pager {
public:
	Random();
	Frame* select_victim_frame();
};

class Clock : public Pager {
public:
	Clock();
	Frame* select_victim_frame();
};

class NRU : public Pager {  //aka Enhanced Second Chance
private:
	int time;  // time of last reset
	void reset_R_bit();  // resets R bits of pte's traversed after 50 or more instructions
public:
	NRU();
	Frame* select_victim_frame();
};

class Aging : public Pager {
public:
	Aging();
	Frame* select_victim_frame();
};

class Working_Set : public Pager {
public:
	Working_Set();
	Frame* select_victim_frame();
};