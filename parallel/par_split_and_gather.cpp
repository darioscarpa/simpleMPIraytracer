#include "par.h"

/*
 basic parallelization using broadcast/gather and static splitting of the frame in numprocs parts

 root node: broadcasts updated world info (needed for rendering), 
			and builds the frame gathering processed frame blocks
 workers: wait for broadcast of world info, then render a frame block and send it to root (by gather)

 The root node does itself a part of the job.
*/

void ParSplitAndGather::init() {
	// generic initialization 
	Par::init();
	
	// allocate a local processing buffer on each node (including root)
	task_size = Scene::height / numprocs;
	taskbuf_size = task_size * Scene::width;
	taskbuf = new unsigned long[taskbuf_size];
}

void ParSplitAndGather::destroy(){	
	Par::destroy();
	delete[] taskbuf;
}

void ParSplitAndGather::getFrameFromWorkers(unsigned long *buf) {
	// master is a worker too, render its block
	Scene::renderFrameBlock(rank * task_size, task_size, taskbuf); 
	*debugLog << "rendered frame blocks" << std::endl;

	// put the frame together directly in the displayed buffer, no useless copying around
	MPI_Gather(taskbuf, taskbuf_size, MPI_UNSIGNED_LONG, buf, taskbuf_size, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
	*debugLog << "gathered all frame blocks" << std::endl;	

	//MPI_Barrier(MPI_COMM_WORLD); //useless
}

void ParSplitAndGather::doWorkForAFrame() {
	//render updated frame block
	Scene::renderFrameBlock(rank * task_size, task_size, taskbuf); 
	*debugLog << "rendered frame block" << std::endl;

	//send it to master
	MPI_Gather(taskbuf, taskbuf_size, MPI_UNSIGNED_LONG, NULL, 0, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
	*debugLog << "sent block (by gather)" << std::endl;

	//MPI_Barrier(MPI_COMM_WORLD); //useless
}
