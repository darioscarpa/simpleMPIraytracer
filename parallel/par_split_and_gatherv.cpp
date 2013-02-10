#include "par.h"

/*
 basic parallelization using broadcast/gatherv and static splitting of the frame in numprocs-1 parts

 root node: updates world, broadcasts info and displays frame gathered from workers
 workers: wait for broadcast of world info, then render a frame block and send it to root (by gatherv)

 The root node does NOT render any block of the frame.
*/

void ParSplitAndGatherV::init() {
	// generic initialization 
	Par::init();

	if (numprocs < 2) throw SingleProcessException();
	
	// allocate a local processing buffer on each node (including root)
	task_size = Scene::height / (numprocs-1);
	taskbuf_size = task_size * Scene::width;
	taskbuf = new unsigned long[taskbuf_size];

	// for gatherv
	recvcounts = new int[numprocs];
	displ = new int[numprocs];
	
	// no processing on root node
	recvcounts[0] = 0;
	displ[0] = 0;
	
	// work split equally among workers
	for (int i=1; i < numprocs; i++) {
		recvcounts[i] = taskbuf_size;
		displ[i] = (i-1)*taskbuf_size;
	}
}

void ParSplitAndGatherV::destroy(){	
	Par::destroy();
	delete[] taskbuf;
	delete[] recvcounts;
	delete[] displ;
}

void ParSplitAndGatherV::getFrameFromWorkers(unsigned long *buf) {
	MPI_Gatherv(taskbuf, taskbuf_size, MPI_UNSIGNED_LONG, buf, recvcounts, displ, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
	*debugLog << "gathered all frame blocks" << std::endl;
}

void ParSplitAndGatherV::doWorkForAFrame() {
	Scene::renderFrameBlock((rank-1) * task_size, task_size, taskbuf); 
	*debugLog << "rendered frame block" << std::endl;

	MPI_Gatherv(taskbuf, taskbuf_size, MPI_UNSIGNED_LONG, NULL, NULL, NULL, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
	*debugLog << "sent frame block (by gather)" << std::endl;
}
