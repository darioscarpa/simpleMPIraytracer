#include "par.h"

int ParMasterWorkers::howManyTasks = ParMasterWorkers::DEFAULT_HOW_MANY_TASKS;

void ParMasterWorkers::init() {
	Par::init();

	if (numprocs < 2) throw SingleProcessException();

	MPI_Type_contiguous( 2, MPI_INT, &MPI_TASK_MSG );
	MPI_Type_commit(&MPI_TASK_MSG);

	howManyWorkers = numprocs - 1;

	// local processing buffer init
	task_size = Scene::height / howManyTasks;
	taskbuf_size = task_size * Scene::width;
	taskbuf = new unsigned long[taskbuf_size];
	
	// special message to signal a frame is done
	frameDoneMsgData[0] = TASKMSG_FRAME_DONE; 
	frameDoneMsgData[1] = 0; 

	// task messages
	taskMessages = new int[howManyTasks][2];
	for (int i=0; i < howManyTasks; i++) {
		taskMessages[i][0] = TASKMSG_GIVING_TASK; 
		taskMessages[i][1] = i * task_size; // from row
	}	
}

void ParMasterWorkers::destroy(){
	MPI_Type_free(&MPI_TASK_MSG);
	Par::destroy();
	delete[] taskbuf;
	delete[] taskMessages;
}
