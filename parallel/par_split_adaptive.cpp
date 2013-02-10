#include "par.h"

/*
 frame adaptive splitting 
 (task sizes based on rendering times communicated by workers)
*/

const double ParSplitAdaptive::SMOOTHING_PREV_PART = 0.95;
const double ParSplitAdaptive::SMOOTHING_CURR_PART = 1.0 - SMOOTHING_PREV_PART;

void ParSplitAdaptive::init() {
	Par::init();

	if (numprocs < 2) throw SingleProcessException();

	MPI_Type_contiguous( 2, MPI_INT, &MPI_TASK_MSG );
	MPI_Type_commit(&MPI_TASK_MSG);
	
	howManyWorkers = numprocs - 1;

	// at the beginning, frame equally split among workers
	const int initial_task_size = Scene::height / (numprocs - 1) ;

	taskMessages = new int[howManyWorkers][2];
	for (int i=0; i < howManyWorkers; i++) {
		taskMessages[i][0] = i * initial_task_size; // from row
		taskMessages[i][1] = initial_task_size; //how many rows
	}	
	
	// allocate a local processing buffer on each node (including root)
	// worst case scenario: someone renders the whole frame, so Scene::height * Scene::width
	taskbuf_size = Scene::height * Scene::width;
	taskbuf = new unsigned long[taskbuf_size];

	// handle async recvs
	req_recvs = new MPI_Request[howManyWorkers];
	req_recvs_times = new MPI_Request[howManyWorkers];

	// rendering times
	workersRenderingLag = new double[howManyWorkers];
}

void ParSplitAdaptive::destroy(){	
	MPI_Type_free(&MPI_TASK_MSG);
	Par::destroy();	
	delete[] taskMessages;	
	delete[] taskbuf;
	delete[] req_recvs;
	delete[] req_recvs_times;	
	delete[] workersRenderingLag;	
}

void ParSplitAdaptive::getFrameFromWorkers(unsigned long *buf) {
	// send tasks and post receives
	MPI_Request useless;
	for (int i=0; i < howManyWorkers; i++) {
		*debugLog << "sending part " << i << " task - msg (" << taskMessages[i][0] << "," << taskMessages[i][1] << ") to process "  << i+1 << std::endl;
		MPI_Isend(&taskMessages[i], 1, MPI_TASK_MSG, i+1, 0, MPI_COMM_WORLD, &useless);
		
		*debugLog << "posting async receives for part " << i << " buffer and rendering time" << std::endl;
		MPI_Irecv(buf + taskMessages[i][0] * Scene::width,
			      taskMessages[i][1] * Scene::width, 
				  MPI_UNSIGNED_LONG, i+1, 0, MPI_COMM_WORLD, &req_recvs[i]);	
		MPI_Irecv( workersRenderingLag + i , 1, MPI_DOUBLE, i+1, 1, MPI_COMM_WORLD, &req_recvs_times[i]);
	}

	*debugLog << "waiting for rendering times" << std::endl;		
	MPI_Waitall( howManyWorkers, req_recvs_times, MPI_STATUSES_IGNORE );

	*debugLog << "calculating new task sizes" << std::endl;		
	
	double totRenderingLag = 0.0;
	for (int i=0; i < howManyWorkers; i++) 
		totRenderingLag += workersRenderingLag[i];
	
	int lastRow = 0;
	const double avgRenderingTime = totRenderingLag / (double) howManyWorkers;

	for (int i=0; i < howManyWorkers; i++) {
		taskMessages[i][0] = lastRow;
				
		// naive, less responsive variation
		//taskMessages[i][1] = (taskRenderingTimes[i] > avgRenderingTime)?
		//					  taskMessages[i][1] = taskMessages[i][1]-1 :
		//					  taskMessages[i][1] = taskMessages[i][1]+1 ;

		// proportional smooth variation
		taskMessages[i][1] = (SMOOTHING_PREV_PART * taskMessages[i][1]) + 
							 (SMOOTHING_CURR_PART * (taskMessages[i][1]  * (avgRenderingTime / workersRenderingLag[i])));
		
		// let's avoid going out of bounds and force the last message to include all the remaining rows
		if ( ( taskMessages[i][1] + lastRow > Scene::height ) ||  (i == howManyWorkers - 1) )
			taskMessages[i][1] = Scene::height - lastRow;

		lastRow += taskMessages[i][1];
		*debugLog << "message " << i << " (" << taskMessages[i][0] << "," << taskMessages[i][1] << ")" << std::endl;
	}	
	
	// wait for rendered frame blocks
	*debugLog << "waiting for async recvs of frame blocks..." << std::endl;
	MPI_Waitall( howManyWorkers, req_recvs, MPI_STATUSES_IGNORE);	
}

void ParSplitAdaptive::doWorkForAFrame() {
	// get task
	MPI_Recv(&taskMsgData, 1, MPI_TASK_MSG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	*debugLog << "received task msg (" << taskMsgData[0] << "," << taskMsgData[1] << ")" << std::endl;

	// do task
	double rtime; 
	Scene::renderFrameBlock(taskMsgData[0], taskMsgData[1], taskbuf, &rtime);
	*debugLog << "rendered frame block starting at row " << taskMsgData[0] << " size " << taskMsgData[1] << " in time " << rtime << std::endl;
	
	// send result
	MPI_Request useless;
	*debugLog << "doing async send of rendered frame block and rendering time..." << std::endl;
	MPI_Isend(&rtime, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, &useless);		
	MPI_Isend(taskbuf, taskMsgData[1]*Scene::width, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, &useless);	
	//MPI_Send(&rtime, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);		
	//MPI_Send(taskbuf, taskMsgData[1]*Scene::width, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);		
}
