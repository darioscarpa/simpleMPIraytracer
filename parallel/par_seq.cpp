#include "par.h"

/*
Sequential implementation - makes sense if run as single process
*/

void ParSequential::init() {
	Par::init();	
}

void ParSequential::destroy(){	
	Par::destroy();	
}

// just render the whole frame locally
void ParSequential::master(SceneUpdateData *in_sud, unsigned long *out_buf) {
	*debugLog << "local render" << std::endl;	
	Scene::renderFrameBlock(0, Scene::height, out_buf); 	
}

//workers, if any, do nothing
void ParSequential::getFrameFromWorkers(unsigned long *buf) {}
void ParSequential::doWorkForAFrame() {}
