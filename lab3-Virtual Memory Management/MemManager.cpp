#include "MemManager.h"

PTE::PTE() {
	present = 0;
	write_protect = 0;
	modified = 0;
	referenced = 0;
	pageout = 0;
	filemapped = 0;
	frame = 0;
	vma_checked = 0;
}
void PTE::reset_bits() {
	present = 0;
	write_protect = 0;
	modified = 0;
	referenced = 0;
	filemapped = 0;
	pageout = 0;
	frame = 0;
	vma_checked = 0;
}

Frame::Frame() {
	pte = nullptr;
	is_free = false;
	is_mapped = false;
	process_id = -1;
	process_vpage = -1;
	bitcounter = 0;
	time_last_used = 0;
}
void Frame::free_frame() {
	is_free = true;
	is_mapped = false;
	process_id = -1;
	process_vpage = -1;
	pte = nullptr;
	bitcounter = 0;
	time_last_used = 0;
}

VMA::VMA() {
}

Process::Process() {
	for (int i = 0; i < VIRTUAL_SPACE; i++) {
		PTE *pte = new PTE();
		page_table[i] = pte;
	}
	int unmaps=0, maps=0, ins=0, outs=0, fins=0, fouts=0, zeros=0, segv=0, segprot=0;
	bool checked_access = false;
}
VMA* Process::vpage_is_in_vma(int vpage) {
	// search process' vmas to find if vpage belongs in them
	for (unsigned int i = 0; i < this->vma_list.size(); i++) {
		VMA *vma = this->vma_list.at(i);
		if (vpage >= vma->start_vpage && vpage <= vma->end_vpage) {
			return vma;
		}
	}
	return nullptr;
}
bool Process::is_vpage_filemapped(int vpage) {
	VMA *vma = this->vpage_is_in_vma(vpage);
	if (vma->filemapped)
		return true;
	else
		return false;
}
bool Process::is_vpage_write_protected(int vpage) {
	VMA *vma = this->vpage_is_in_vma(vpage);
	if (vma->write_protect) {
		return true;
	}
	else
		return false;
}
void Process::print_table() {
	printf("PT[%d]: ", this->id);
	for (int j = 0; j < VIRTUAL_SPACE; j++) {
		PTE *pte = this->page_table[j];
		if (pte->present == false) {
			if (pte->pageout == true)
				printf("# ");
			else
				printf("* ");
		}
		else {
			printf("%d:", j);
			if (pte->referenced)
				printf("R");
			else
				printf("-");
			if (pte->modified)
				printf("M");
			else
				printf("-");
			if (pte->pageout)
				printf("S");
			else
				printf("-");
			printf(" ");
		}
	}
}

Pager::Pager() {
	hand = 0;
}
void Pager::update(Frame *f) {
	vector<Frame*>::iterator it = find(queue.begin(), queue.end(), f);

	if (it != queue.end()) {  // frame found in queue, do nothing
		return;
	}
	else {  // frame not found, then add to queue
		queue.push_back(f);
	}
}

FIFO::FIFO() {
}
Frame* FIFO::select_victim_frame() {
	if (queue.empty())
		return nullptr;
	Frame *f = queue.front();
	queue.erase(queue.begin());
	return f;
}

Random::Random() {
	hand = 0;  // unused by this pager
}
Frame* Random::select_victim_frame() {
	int i = myrandom(FRAME_COUNT);
	Frame *f = frame_table.at(i);
	return f;
}

Clock::Clock() {
}
Frame* Clock::select_victim_frame() {
	if (queue.empty()) {
		printf("ERROR: Clock should not be empty");  // impossible, if it's the case, then there should have been free frames to allocate
		exit(1);
	}

	Frame *pointed_frame = nullptr;
	while (true) {
		if (hand == queue.size())  // reset hand
			hand = 0;

		pointed_frame = queue.at(hand);
		if (pointed_frame->pte->referenced == false) {  // pick this bad boy
			hand++;
			return pointed_frame;
		}
		else {
			pointed_frame->pte->referenced = false;  // set R bit to 0, move hand to next
			hand++;
		}
	}

}

NRU::NRU() {
	time = 0;
}
void NRU::reset_R_bit() {
	for (unsigned int i = 0; i < queue.size(); i++) {
		queue.at(i)->pte->referenced = false;
	}
	time = instruct_count;
}
Frame* NRU::select_victim_frame() {

	int victim_at_hand[4] = {-1,-1,-1,-1};
	Frame *Class0 = nullptr;
	Frame *Class1 = nullptr;
	Frame *Class2 = nullptr;
	Frame *Class3 = nullptr;

	unsigned int count = 0;
	unsigned int shadow_hand = hand;  // use a shadow agent to inspect frame queue. Start at where hand is pointed
	while (count < queue.size()) {
		PTE *pte = queue.at(shadow_hand)->pte;

		if (Class0 == nullptr && pte->referenced == false && pte->modified == false) {
			Class0 = queue.at(shadow_hand);
			victim_at_hand[0] = shadow_hand;
		}
		if (Class1 == nullptr && pte->referenced == false && pte->modified == true) {
			Class1 = queue.at(shadow_hand);
			victim_at_hand[1] = shadow_hand;	
		}
		if (Class2 == nullptr && pte->referenced == true && pte->modified == false) {
			Class2 = queue.at(shadow_hand);
			victim_at_hand[2] = shadow_hand;
		}
		if (Class3 == nullptr && pte->referenced == true && pte->modified == true) {
			Class3 = queue.at(shadow_hand);
			victim_at_hand[3] = shadow_hand;
		}

		count++;
		shadow_hand++;
		if (shadow_hand == queue.size())
			shadow_hand = 0;
	}

	Frame *f = nullptr;
	for (unsigned int i = 0; i < 4; i++) {
		if (victim_at_hand[i] != -1) {
			f = queue.at(victim_at_hand[i]);
			hand = ++victim_at_hand[i];  // set hand to the shadow hand where next victim is pointed at
			break;
		}
	}

	if (hand == queue.size())
		hand = 0;

	if (instruct_count - time >= 50) {  // reset bits after 50 instructions elapsed
		this->reset_R_bit();
	}
	return f;
}

Aging::Aging() {
}
Frame* Aging::select_victim_frame() {
	Frame *f = nullptr;
	// shift bitcounter right and add reference bit to leftmost bit
	for (unsigned int i = 0; i < queue.size(); i++) {
		f = queue.at(i);
		f->bitcounter = f->bitcounter >> 1;
		f->bitcounter[31] = f->pte->referenced;
		f->pte->referenced = false;
	}
	// find minimum bitcounter
	unsigned long min = ULONG_MAX;
	int min_index = 0;
	unsigned int count = 0;
	unsigned int shadow_hand = hand;
	while (count < queue.size()) {
		f = queue.at(shadow_hand);
		unsigned long to_u_long = f->bitcounter.to_ulong();
		if (to_u_long < min) {
			min = to_u_long;
			min_index = shadow_hand;
		}

		shadow_hand++;
		if (shadow_hand == queue.size())
			shadow_hand = 0;
		count++;
	}
	// find and return oldest frame
	f = queue.at(min_index);
	hand = ++min_index;
	if (hand == queue.size())
		hand = 0;

	return f;
}

Working_Set::Working_Set() {
}
Frame* Working_Set::select_victim_frame() {
	Frame *frame = nullptr;
	unsigned int count = 0;
	int min = INT_MAX;
	int min_index = -1;

	vector<Frame*>list = queue;  // for testing

	while (count < list.size()) {
		frame = list.at(hand);

		if (frame->pte->referenced == true) {  // if R = 1
			frame->time_last_used = instruct_count;
			frame->pte->referenced = false;
		}
		else {  // if R = 0
			if (instruct_count - frame->time_last_used >= 50) {  // if greater than tau, pick this victim
				hand++;
				if (hand == list.size())
					hand = 0;
				return frame;
			}
		}
		// remember oldest frame
		if (frame->time_last_used < min) {
			min = frame->time_last_used;
			min_index = hand;
		}

		hand++;
		if (hand == list.size())
			hand = 0;
		count++;
	}
	// if entire table is scanned without finding victim, evict oldest one
	frame = list.at(min_index);
	
	hand = ++min_index;
	if (hand == list.size())
		hand = 0;
	
	return frame;

}