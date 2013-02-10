#include "par.h"

void ParMasterWorkersAsync::init() {
	ParMasterWorkers::init();

	// optimizations: double buffering, different handling of tasks/async communication
	sending_buf = new unsigned long[taskbuf_size];
	req_recvs = new MPI_Request[howManyTasks];
}

void ParMasterWorkersAsync::destroy(){	
	ParMasterWorkers::destroy();
	
	delete[] sending_buf;
	delete[] req_recvs;
}

void ParMasterWorkersAsync::getFrameFromWorkers(unsigned long *buf) {
	for (int i=0; i < howManyTasks; i++)
		req_recvs[i] = MPI_REQUEST_NULL;

	int last_task_sent = 0;	
	int first_available_worker;	
	while (last_task_sent < howManyTasks) {
		/*if (last_task_sent < howManyWorkers*2) {
			first_available_worker = ( last_task_sent % howManyWorkers ) + 1;
		} else {
			MPI_Status status;
			MPI_Waitany(last_task_sent, req_recvs,  &first_available_worker, &status);
			first_available_worker = status.MPI_SOURCE;
		}*/
		if (last_task_sent < howManyWorkers) {
			first_available_worker = last_task_sent  + 1;
			*debugLog << "worker idle: " << first_available_worker << std::endl;			
		} else if (last_task_sent < howManyWorkers*2) { 
			first_available_worker = last_task_sent + 1 - howManyWorkers;
			*debugLog << "worker " << first_available_worker << " could be async sending, queue another task " << std::endl;			
		} else {
			*debugLog << "all workers should be busy in processing/sending, waiting for a receive to complete... " ;
			MPI_Status status;
			MPI_Waitany(last_task_sent, req_recvs,  &first_available_worker, &status);
			first_available_worker = status.MPI_SOURCE;
			*debugLog << "... " << first_available_worker << " completed!" << std::endl;							
		}
		*debugLog << "sending task " << last_task_sent << " - msg (" << taskMessages[last_task_sent][0] << "," << taskMessages[last_task_sent][1] << ") to process "  << first_available_worker << std::endl;
		MPI_Send(&taskMessages[last_task_sent], 1, MPI_TASK_MSG, first_available_worker, last_task_sent, MPI_COMM_WORLD);		
		
		*debugLog << "posting async receive for task " << last_task_sent << std::endl;
		MPI_Irecv(buf + taskMessages[last_task_sent][1] * Scene::width, taskbuf_size, MPI_UNSIGNED_LONG, first_available_worker, 0, MPI_COMM_WORLD, &req_recvs[last_task_sent]);	
	
		last_task_sent++;
	}

	*debugLog << "sent all tasks" << std::endl;

	// signal workers that a frame is done (so they'll wait for new world update and not new task)
	MPI_Request useless;
	for (int i = 1; i < numprocs; i++) 
		MPI_Isend(frameDoneMsgData, 1, MPI_TASK_MSG, i, 0, MPI_COMM_WORLD, &useless);		
	
	*debugLog << "sent all frame termination msg" << std::endl;

	// wait for completion of all tasks
	*debugLog << "waiting for async recvs of frame blocks..." << std::endl;
	MPI_Waitall(howManyTasks, req_recvs, MPI_STATUSES_IGNORE);
}

void ParMasterWorkersAsync::doWorkForAFrame() {
	static int framecont = 0;
	*log << " --- frame " << ++framecont << " --- " << std::endl;
	MPI_Request prevRequest = MPI_REQUEST_NULL;
	while (true) {
		MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		*debugLog << "received task msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;		
			
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
