/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/

/*=====================================================================
Additional code by Dario Scarpa (http://www.duskzone.it)
Same for me! :)
=====================================================================*/

#include "../parallel/par.h"
#include "../parallel/scene.h"
#include "../parallel/console.h"

#include "simwin_framework.h"

#include "simplewin2d.h"

#include <iostream>

bitmapWindow* graphics = NULL;
HINSTANCE hinstance = NULL;
HWND windowhandle = NULL;

void frameworkInit();
void frameworkMain();
void frameworkShutdown();

static const bool g_showConsole = true;
static const bool g_doLogging = true;

//entry point to program
int WINAPI WinMain(HINSTANCE hinstance_, 
                      HINSTANCE hprevinstance, 
                      LPSTR lpcmdline, 
                      int ncmdshow)
{
	hinstance = hinstance_;


	//    bitmapWindow(int x, int y, int width, int height, HINSTANCE currentInstance); // Constructor

	// command line params parse
	for (int i=1; i < __argc; i++) {
		if ( !strncmp(__argv[i], "tasks=", 6) ) {
			//int taskno = atoi(__argv[i] + 6 );
			ParMasterWorkers::setHowManyTasks(atoi(__argv[i] + 6 ));			
		}
		if (!strcmp(__argv[i], "enableLogging")) Par::loggingEnabled = true; 
		else if (!strcmp(__argv[i], "enableDebugging")) Par::debuggingEnabled = true; 
		else if (!strcmp(__argv[i], "showConsole")) Console::enabled = true;
		else if (!strcmp(__argv[i], "showSplitting")) Scene::showSplitting = true; 
		else if (!strcmp(__argv[i], "moveCamera")) Scene::moveCamera = true;
		else if ( !strcmp(__argv[i], "help") || !strcmp(__argv[i], "--help") || !strcmp(__argv[i], "-h") || !strcmp(__argv[i], "/?") ) {
				Console::open();
				std::cout <<
					"strategy=n (default n=1)" << std::endl <<
					" 0: sequential mode, no parallelization" << std::endl <<
					" 1: static splitting and gather (root node does rendering too)" << std::endl <<
					" 2: static splitting and gatherv (no rendering on root node)" << std::endl <<
					" 3: master/workers: basic, feeds tasks to idle workers" << std::endl <<
					" 4: master/workers: 3 with task queueing: double buffering, async communication" << std::endl <<
					" 5: master/workers: as 4, but passive master: workers request tasks when ready" << std::endl <<
					" 6: adaptive splitting: frame split to balance rendering times" << std::endl <<
					std::endl <<
					" tasks=n (default n=10)" << std::endl <<
					"  useful only in master/workers strategies" << std::endl <<
					"  defines in how many blocks (tasks) the frame is split" << std::endl <<
					std::endl <<
					"     moveCamera: moves camera, differently dislocating the workload" << std::endl <<
					"  showSplitting: red divions lines (yellow area -> rendered on odd ranked node)" << std::endl <<
					"    showConsole: show fps and other stats in a console window" << std::endl <<
					"  enableLogging: each process writes a log useful for profiling" << std::endl <<
					"enableDebugging: each process writes its actions in a verbose debug log" << std::endl <<
					"           help: I know that you know" << std::endl;

				std::cout << std::endl << "example: " << __argv[0] << " strategy=4 tasks=8 showSplitting showConsole" << std::endl;
				std::cout << " --- " << std::endl << "press enter to exit help screen" << std::endl;

				std::cin.ignore();
				std::cin.get(); 
				exit(1);				
		}
	}

	// create instance of parallelization class with the chosen strategy
	Par *par = NULL;
	for (int i=1; i < __argc; i++) {
		if (!strcmp(__argv[i], "strategy=sequential") || !strcmp(__argv[i], "strategy=0") )
			par = Par::getInstance(Par::SEQUENTIAL);		
		if (!strcmp(__argv[i], "strategy=split_and_gather") || !strcmp(__argv[i], "strategy=1") )
			par = Par::getInstance(Par::GATHER);
		else if (!strcmp(__argv[i], "strategy=split_and_gatherv") || !strcmp(__argv[i], "strategy=2") )
			par = Par::getInstance(Par::GATHERV);
		else if (!strcmp(__argv[i], "strategy=workers_pool") || !strcmp(__argv[i], "strategy=3") )
			par = Par::getInstance(Par::MASTER_WORKERS_SIMPLE);
		else if (!strcmp(__argv[i], "strategy=workers_pool_async") || !strcmp(__argv[i], "strategy=4") )
			par = Par::getInstance(Par::MASTER_WORKERS_ASYNC);		
		else if (!strcmp(__argv[i], "strategy=workers_pool_pasv") || !strcmp(__argv[i], "strategy=5") )
			par = Par::getInstance(Par::MASTER_WORKERS_PASV);		
		else if (!strcmp(__argv[i], "strategy=adaptive") || !strcmp(__argv[i], "strategy=6") )
			par = Par::getInstance(Par::ADAPTIVE);
	}
	if (par==NULL) par = Par::getInstance(Par::GATHER); //default
	
	try {
		par->init();
	} catch (Par::SingleProcessException) {
		Console::open();
		std::cout << "the selected strategy needs at least two processess, and you're running a single one!" << std::endl;
		std::cout << " --- " << std::endl << "press enter to exit" << std::endl;
		std::cin.ignore();
		std::cin.get(); 
		exit(1);				
	}

	Scene::init();	
	if (par->rank == 0) { // master thread opens window and gets events, visualizes rendering
	
		frameworkInit();
		
		//-----------------------------------------------------------------
		//enter main program loop
		//-----------------------------------------------------------------
		MSG msg;     
		while(true)  // run forever - or until we manually break out of the loop!
		{
			if (PeekMessage(&msg, NULL,0,0,PM_REMOVE)) // check if a message awaits and remove 
												 // it from the queue if there is one..
			{
				if(msg.message == WM_QUIT) 
					break; // exit loop if a quit message
	             
				TranslateMessage(&msg); //Converts the message to a format used in the
										 //event handler
	             
				DispatchMessage(&msg); // THIS function sends the message to the event
										// handler
			}
	           
		 // Here you are free to do anything not related to messages - like the inner loop
		 // of a game or something like that.
			frameworkMain();
		} // end while(TRUE)
	
		frameworkShutdown();
		par->terminateWorkers();
	} else { // worker threads
		par->worker();
	}
		
	par->destroy();
	Scene::destroy();
	
	return 0;
}




void win2dError(const std::string& errormessage)
{
	MessageBox(NULL, errormessage.c_str(), "Error", MB_OK);
	exit(666);
}


void win2dError(const std::string& errormessage, const std::string& boxtitle)
{
	MessageBox(NULL, errormessage.c_str(), boxtitle.c_str(), MB_OK);
	exit(666);
}



void frameworkInit()
{
	//int width, height;
	//getWindowDims(width, height);

	//graphics = new bitmapWindow(30, 30, width, height, hinstance);

	graphics = new bitmapWindow(30, 30, Scene::width, Scene::height, hinstance);

	windowhandle = graphics->getWindowHandle();

	Scene::setRenderingBuffer(graphics->getWindowSurfaceBuffer());

//	doInit(hinstance, windowhandle, *graphics);
	if (Console::enabled) Console::open();

}

void frameworkMain()
{
	graphics->startDrawing();
    
	//doMain(*graphics);
	//Scene::renderFrame(*graphics);
	//Scene::renderFrameMaster(*graphics);
	Scene::renderFrameMaster();
	
	graphics->finishDrawing();

}

void frameworkShutdown()
{
	if (Console::enabled) Console::close();
	//doShutdown();
	delete graphics;
}


