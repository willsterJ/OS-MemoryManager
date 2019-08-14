#include <vector>
#include "global.h"

using namespace std;

enum {FALSE, TRUE};

class PTE {
public:
	unsigned int present : 1;
	unsigned int write_protect : 1;
	unsigned int modified : 1;
	unsigned int referenced : 1;
	unsigned int pageout : 1;
	unsigned int frame : 7;  // we can have at most 128 frames

	PTE();
};

class Frame {
public:
	int id;
	bool is_free;
	int process_id;
	int process_page_table_index;

	Frame();
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

	Process();
};

// MMU --------------------------------------------------------
class Pager {
public:
	vector<Frame*> frame_table;
	virtual Frame* select_victim_frame() = 0;
};

class FIFO : public Pager {
public:
	vector<Frame*> queue;
	Frame* select_victim_frame();

	FIFO();
};