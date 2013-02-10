#ifndef __PAR_H__
#define __PAR_H__

#include <mpi.h>
#include <sstream>

#include "scene.h"

class SceneUpdateData;

class Par  {
	public:
		struct SingleProcessException {};

		int task_size;			// in pixel rows - Scene::height / numprocs
		int taskbuf_size;		// in pixels     - task_size * Scene::width
		unsigned long *taskbuf;

		int rank;
		int numprocs;
			
		static bool loggingEnabled;
		static std::ostream *log;
		
		static bool debuggingEnabled;	
		static std::ostream *debugLog;

		enum strategy { DO_NOT_ISTANTIATE = -1,
						SEQUENTIAL,
						GATHER,
						GATHERV,					
						MASTER_WORKERS_SIMPLE,
						MASTER_WORKERS_ASYNC,
						MASTER_WORKERS_PASV,
						ADAPTIVE,
					  };

	protected:
		static const float UPDATEMSG_UPDATEWORLD;
		static const float UPDATEMSG_TERMINATE;

		MPI_Datatype MPI_UPDATE_MSG;
		float msgData[10];
		bool toldToTerminate();
		
	public: 
		void terminateWorkers();	
			
		void sceneUpdateDataFromMsg(SceneUpdateData *out_sud);
		void sceneUpdateDataToMsg(SceneUpdateData *in_sud);

		virtual void master(SceneUpdateData *in_sud, unsigned long *out_buf);
		void worker();
			
		virtual void init() = 0;
		virtual void destroy() = 0;
		virtual	void getFrameFromWorkers(unsigned long *buf) = 0;
		virtual void doWorkForAFrame() = 0;	
		
		static Par *getInstance(strategy s = DO_NOT_ISTANTIATE);

	private:
		static Par *instance;

		static void initLogging(int rank);
		static void stopLogging();
};


class ParSequential : public Par {
	virtual void master(SceneUpdateData *in_sud, unsigned long *out_buf);
	virtual void init();
	virtual void destroy();
	virtual void getFrameFromWorkers(unsigned long *buf);
	virtual void doWorkForAFrame();	
};

class ParSplitAndGather : public Par {
	virtual void init();
	virtual void destroy();
	virtual void getFrameFromWorkers(unsigned long *buf);
	virtual void doWorkForAFrame();	
};

class ParSplitAndGatherV : public ParSplitAndGather {
	int *recvcounts;
	int *displ;

	virtual void init();
	virtual void destroy();
	virtual void getFrameFromWorkers(unsigned long *buf);
	virtual void doWorkForAFrame();	
};

class ParMasterWorkers : public Par {
	protected:
		static const int DEFAULT_HOW_MANY_TASKS = 10;

		static const int TASKMSG_GIVING_TASK = 0;
		static const int TASKMSG_FRAME_DONE = 1;
		static const int TASKMSG_ASKING_TASK = 2;

		MPI_Datatype MPI_TASK_MSG;
		int frameDoneMsgData[2];
		int (*taskMessages)[2];
		int taskMsgData[2];

		static int howManyTasks;
		int howManyWorkers;
		
		MPI_Request *req_recvs;

		bool frameDone() { return (taskMsgData[0] == TASKMSG_FRAME_DONE); }

	public:
		virtual void init();
		virtual void destroy();
		virtual void doWorkForAFrame() = 0;	
		virtual	void getFrameFromWorkers(unsigned long *buf) = 0;

		static void setHowManyTasks(int taskno) { 
			if (taskno>0) howManyTasks = taskno; else howManyTasks = DEFAULT_HOW_MANY_TASKS;
		}
};

class ParMasterWorkersSimple : public ParMasterWorkers {
	virtual void init();
	virtual void destroy();
	virtual	void getFrameFromWorkers(unsigned long *buf);
	virtual void doWorkForAFrame();
};

class ParMasterWorkersAsync : public ParMasterWorkers {
	protected:
		unsigned long *sending_buf; // for double buffering		

		virtual void init();
		virtual void destroy();
		virtual	void getFrameFromWorkers(unsigned long *buf);
		virtual void doWorkForAFrame();	
};

class ParMasterWorkersPasv : public ParMasterWorkers {
	protected:
		unsigned long *sending_buf; // for double buffering
	
		virtual void init();
		virtual void destroy();
		virtual	void getFrameFromWorkers(unsigned long *buf);
		virtual void doWorkForAFrame();	
};

class ParSplitAdaptive : public Par {
	protected:
		static const double SMOOTHING_PREV_PART;
		static const double SMOOTHING_CURR_PART;

		MPI_Datatype MPI_TASK_MSG;
		
		int howManyWorkers;
		
		int (*taskMessages)[2];
		double *workersRenderingLag;

		MPI_Request *req_recvs;
		MPI_Request *req_recvs_times;

		int taskMsgData[2];

		virtual void init();
		virtual void destroy();
		virtual void getFrameFromWorkers(unsigned long *buf);
		virtual void doWorkForAFrame();	
};

#endif //__PAR_H__

