#include "par.h"

void ParMasterWorkersPasv::init() {
	ParMasterWorkers::init();
		
	// optimizations: double buffering, different handling of tasks/async communication
	sending_buf = new unsigned long[taskbuf_size];
	req_recvs = new MPI_Request[howManyTasks];
}

void ParMasterWorkersPasv::destroy(){	
	ParMasterWorkers::destroy();
	
	delete[] sending_buf;
	delete[] req_recvs;
}

void ParMasterWorkersPasv::getFrameFromWorkers(unsigned long *buf) {
	for (int i=0; i < howManyTasks; i++)
		req_recvs[i] = MPI_REQUEST_NULL;

	int last_task_sent = 0;
	int first_available_worker;
			
	while (last_task_sent < howManyTasks) {
		MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		*debugLog << "received task request from " << taskMsgData[1]<< std::endl;

		first_available_worker = taskMsgData[1];
		*debugLog << "sending task " << last_task_sent << " - msg (" << taskMessages[last_task_sent][0] << "," << taskMessages[last_task_sent][1] << ") to process "  << first_available_worker << std::endl;
		MPI_Send(&taskMessages[last_task_sent], 1, MPI_TASK_MSG, first_available_worker, last_task_sent, MPI_COMM_WORLD);		
		
		*debugLog << "posting async receive for task " << last_task_sent << std::endl;
		MPI_Irecv(buf + taskMessages[last_task_sent][1] * Scene::width, taskbuf_size, MPI_UNSIGNED_LONG, first_available_worker, 0, MPI_COMM_WORLD, &req_recvs[last_task_sent]);	
	
		last_task_sent++;
	}
	*debugLog << "sent all tasks" << std::endl;
	MPI_Request useless;
	for (int i = 1; i < numprocs; i++) {
		MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, i, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Isend(frameDoneMsgData, 1, MPI_TASK_MSG, i, 0, MPI_COMM_WORLD, &useless);
	}
	*debugLog << "sent all frame termination msg" << std::endl;
	
	*debugLog << "waiting for async recvs of frame blocks..." << std::endl;
	MPI_Waitall( howManyTasks, req_recvs, MPI_STATUSES_IGNORE);
}

void ParMasterWorkersPasv::doWorkForAFrame() {
	static int framecont = 0;
	*log << " --- frame " << ++framecont << " --- " << std::endl;
	MPI_Request prevRequest = MPI_REQUEST_NULL;
	while (true) {
		taskMsgData[0] = TASKMSG_ASKING_TASK;
		taskMsgData[1] = rank;
			
		*debugLog << "sending task request - msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;
		MPI_Send(&taskMsgData, 1, MPI_TASK_MSG, 0, 0, MPI_COMM_WORLD);
			
		MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		*debugLog << "received task - msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;

		if (taskMsgData[0] == TASKMSG_FRAME_DONE) break; 
		
		Scene::renderFrameBlock(taskMsgData[1], task_size, taskbuf);
		*debugLog << "rendered frame block starting at row " << taskMsgData[1] << " size " << task_size << std::endl;

		*debugLog << "waiting for prev async send, if any...";
		MPI_Wait(&prevRequest, MPI_STATUS_IGNORE); // if prev async send not finished, wait (buffer needed!)
		*debugLog << " done, buffer now available" << std::endl;

		// swap bufs
		unsigned long *tmp = sending_buf;
		sending_buf = taskbuf;
		taskbuf = tmp;
		*debugLog << "processing/sending bufs swapped" << std::endl;
				
		// send rendered frame block and allow to receive a new task in the meantime
		MPI_Isend(sending_buf, taskbuf_size, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, &prevRequest);
		*debugLog << "async send of rendered task started" << std::endl;
	}
}

