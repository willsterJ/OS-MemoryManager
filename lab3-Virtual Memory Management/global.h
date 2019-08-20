
#ifndef GLOBAL_H

#include <vector>
using namespace std;
#define GLOBAL_H

extern int PROCESS_COUNT;
extern int FRAME_COUNT;
extern int VIRTUAL_SPACE;

extern vector<int> random_vect;
extern unsigned int random_offset;
extern int myrandom(int mod);
extern unsigned long instruct_count;

#endif