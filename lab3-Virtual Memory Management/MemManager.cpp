#include "MemManager.h";

PTE::PTE() {
	present = 0;
	write_protect = 0;
	modified = 0;
	referenced = 0;
	pageout = 0;
}

Frame::Frame() {
	pte = nullptr;
	is_free = false;
}

VMA::VMA() {

}

Process::Process() {
	for (int i = 0; i < VIRTUAL_SPACE; i++) {
		PTE *pte = new PTE();
		page_table[i] = pte;
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