#include "par.h"
#include "nullstream.h"

#include <stdlib.h> // for declaration of __argv and __argc
#include <string>
#include <fstream>

Par *Par::instance = NULL;

bool Par::loggingEnabled = false;
bool Par::debuggingEnabled = false;

const float Par::UPDATEMSG_UPDATEWORLD = 0.0;
const float Par::UPDATEMSG_TERMINATE = 1.0;

Par *Par::getInstance(strategy s) {
	if (instance == NULL) {
		switch(s) {
			case SEQUENTIAL: instance = new ParSequential(); break;
			case GATHER  : instance = new ParSplitAndGather(); break;
			case GATHERV : instance = new ParSplitAndGatherV(); break;
			case MASTER_WORKERS_SIMPLE: instance = new ParMasterWorkersSimple(); break;
			case MASTER_WORKERS_ASYNC : instance = new ParMasterWorkersAsync(); break;
			case MASTER_WORKERS_PASV  : instance = new ParMasterWorkersPasv(); break;
			case ADAPTIVE: instance = new ParSplitAdaptive(); break;			
		}
	}
	return instance;
}

std::ostream *Par::log = new NullStream();
std::ostream *Par::debugLog = new NullStream();

void Par::initLogging(int rank) {
	if (loggingEnabled) {
		std::string logbasename = "log_task_";
		std::ostringstream oss;
		oss << logbasename << rank;
		delete log;
		log = new std::ofstream(oss.str().c_str());
	}
	if (debuggingEnabled) {
		std::string logbasename = "debuglog_task_";
		std::ostringstream oss;
		oss << logbasename << rank;
		delete debugLog;
		debugLog = new std::ofstream(oss.str().c_str());
	}
}

void Par::stopLogging() {
	if (loggingEnabled)
		((std::ofstream*)log)->close();
	if (debuggingEnabled)
		((std::ofstream*)debugLog)->close();
	delete log;
	delete debugLog;
}

void Par::init() {
	MPI_Init(&__argc, &__argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	  
	MPI_Type_contiguous( 10, MPI_FLOAT, &MPI_UPDATE_MSG );
	MPI_Type_commit(&MPI_UPDATE_MSG);

	initLogging(rank);

	*debugLog << "Process rank:" << rank << "  - Number of processes launched: " << numprocs << std::endl << " --- " << std::endl;	
}

void Par::destroy() {
	MPI_Type_free(&MPI_UPDATE_MSG);
	MPI_Finalize();
	
	stopLogging();
}

void Par::sceneUpdateDataFromMsg(SceneUpdateData *out_sud) {
	out_sud->light1Pos.x = msgData[1];
	out_sud->light1Pos.y = msgData[2];
	out_sud->light1Pos.z = msgData[3];
	out_sud->sphere1Pos.x = msgData[4];
	out_sud->sphere1Pos.y = msgData[5];
	out_sud->sphere1Pos.z = msgData[6];
	out_sud->cameraPos.x = msgData[7];
	out_sud->cameraPos.y = msgData[8];
	out_sud->cameraPos.z = msgData[9];
}

void Par::sceneUpdateDataToMsg(SceneUpdateData *in_sud) {
	msgData[0] = UPDATEMSG_UPDATEWORLD;
	msgData[1] = in_sud->light1Pos.x;
	msgData[2] = in_sud->light1Pos.y;
	msgData[3] = in_sud->light1Pos.z;
	msgData[4] = in_sud->sphere1Pos.x;
	msgData[5] = in_sud->sphere1Pos.y;
	msgData[6] = in_sud->sphere1Pos.z;
	msgData[7] = in_sud->cameraPos.x;
	msgData[8] = in_sud->cameraPos.y;
	msgData[9] = in_sud->cameraPos.z;
}

void Par::terminateWorkers() {
	*debugLog << "doing workers termination broadcast ";	
	msgData[0] = UPDATEMSG_TERMINATE;
	MPI_Bcast( msgData, 1, MPI_UPDATE_MSG, 0, MPI_COMM_WORLD );	
	*debugLog << "...done" << std::endl;		
}

bool Par::toldToTerminate() {
	return ( msgData[0] == UPDATEMSG_TERMINATE );
}

void Par::master(SceneUpdateData *in_sud, unsigned long *out_buf) {
	sceneUpdateDataToMsg(in_sud);
	
	*debugLog << "doing world update broadcast" << std::endl;	
	MPI_Bcast( msgData, 1, MPI_UPDATE_MSG, 0, MPI_COMM_WORLD );
	
	getFrameFromWorkers(out_buf);
	*debugLog << "ok, ready to display frame" << std::endl << std::endl;		
}

void Par::worker() {
	SceneUpdateData out_sud;
	while (true) {		
		MPI_Bcast( &msgData, 1, MPI_UPDATE_MSG, 0, MPI_COMM_WORLD);
		*debugLog << "received world update broadcast" << std::endl;

		if ( toldToTerminate() ) break;

		// update scene
		sceneUpdateDataFromMsg(&out_sud);
		Scene::updateWorldInWorker(&out_sud);

		// and here comes virtual
		doWorkForAFrame();

		*debugLog << "ok, work for a frame done! waiting new world update broadcast" << std::endl << std::endl;		
	}
	*debugLog << "received termination broadcast" << std::endl;
}
