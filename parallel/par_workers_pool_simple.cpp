#include "par.h"

void ParMasterWorkersSimple::init() {
	ParMasterWorkers::init();

	// to handle async receives
	req_recvs = new MPI_Request[howManyWorkers];
	for (int i=0; i<howManyWorkers; i++) req_recvs[i] = MPI_REQUEST_NULL;
}

void ParMasterWorkersSimple::destroy(){
	ParMasterWorkers::destroy();
	
	delete[] req_recvs;
}

void ParMasterWorkersSimple::getFrameFromWorkers(unsigned long *buf) {
	int last_task_sent = 0;
	int first_available_worker;
	while (last_task_sent < howManyTasks) {
		if (last_task_sent < howManyWorkers) {	// some worker idle		 
			first_available_worker = last_task_sent + 1;
			*debugLog << "first idle worker is " << first_available_worker << std::endl;			
		} else { // ...otherwise, wait for some worker to finish a task and send it a new one
			*debugLog << "waiting for any worker to return free... " << std::endl;			
			MPI_Waitany(howManyWorkers, req_recvs,  &first_available_worker, MPI_STATUS_IGNORE);
			first_available_worker++;
			*debugLog << "..." << first_available_worker << " is now free" << std::endl;
		}

		*debugLog << "sending task " << last_task_sent << " - msg (" << taskMessages[last_task_sent][0] << "," << taskMessages[last_task_sent][1] << ") to process "  << first_available_worker << std::endl;
		MPI_Send(&taskMessages[last_task_sent], 1, MPI_TASK_MSG, first_available_worker, last_task_sent, MPI_COMM_WORLD);
		
		*debugLog << "posting async receive for task " << last_task_sent << std::endl;
		MPI_Irecv(buf + taskMessages[last_task_sent][1] * Scene::width, taskbuf_size, MPI_UNSIGNED_LONG, first_available_worker, 0, MPI_COMM_WORLD, &req_recvs[first_available_worker-1]);
		
		last_task_sent++;

		/* test: sync send/recv everything to first worker
		int first_available_worker = 1;
		MPI_Send(&taskMessages[last_task_sent], 1, MPI_TASK_MSG, first_available_worker, last_task_sent, MPI_COMM_WORLD);
		*log << "sent " << last_task_sent << std::endl;
		MPI_Recv(buf + taskMessages[last_task_sent][1] * Scene::width, taskbuf_size, MPI_UNSIGNED_LONG, first_available_worker, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		*log << "recvd " << last_task_sent << std::endl;
		last_task_sent++;
		*/		
	}
	*debugLog << "sent all tasks" << std::endl;

	// signal workers that a frame is done (so they'll wait for new world update and not new task)
	MPI_Request useless;
	for (int i = 1; i < numprocs; i++)
		MPI_Isend(frameDoneMsgData, 1, MPI_TASK_MSG, i, 0, MPI_COMM_WORLD, &useless);
		
	*debugLog << "sent all frame termination msg" << std::endl;

	// wait for completion of all tasks
	*debugLog << "waiting for async recvs of frame blocks..." << std::endl;
	MPI_Waitall(howManyWorkers, req_recvs, MPI_STATUSES_IGNORE);
}

void ParMasterWorkersSimple::doWorkForAFrame() {
	// process incoming tasks for current frame
	static int framecont = 0;
	*log << " --- frame " << ++framecont << " --- " << std::endl;
	while (true) {
		MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		*debugLog << "received task msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;

		if (taskMsgData[0] == TASKMSG_FRAME_DONE) break; 

		Scene::renderFrameBlock(taskMsgData[1], task_size, taskbuf);
		*debugLog << "rendered frame block starting at row " << taskMsgData[1] << " size " << task_size << std::endl;
			
		MPI_Send(taskbuf, taskbuf_size, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
		*debugLog << "sent buffer for task msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;
	}
}
