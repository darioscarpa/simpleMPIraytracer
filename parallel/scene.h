#ifndef __SCENE_H__
#define __SCENE_H__

#include "par.h"

//#include "../simplewin2d/simwin_framework.h"
#include "../simplewin2d/simplewin2d.h"
#include "../utils/timer.h"
#include "../simpleraytracer/world.h"
#include "../simpleraytracer/ray.h"
#include "../simpleraytracer/colour.h"
#include "../simpleraytracer/material.h"
#include "../simpleraytracer/rayplane.h"
#include "../simpleraytracer/raysphere.h"
#include "../simpleraytracer/object.h"
#include "../simpleraytracer/light.h"

class SceneUpdateData {
	public:
		Vec3 light1Pos;
		Vec3 sphere1Pos;
		Vec3 cameraPos;
};

class Scene {
public:
	static const int width = 400;
	static const int height = 400;

	// move the camera
	static bool moveCamera;

	// render as red the first line of each separately rendered block, show yellow area 
	static bool showSplitting;

	static void init();
	static void destroy();
	static void setRenderingBuffer(unsigned long *buf);
	
	//sequential -  render full frame
	//static void renderFrame();

	//sequential splitting frame, for testing
	//static void renderFrameSplitted();

	//render a frame block (from row y_start, for y_size rows of pixels, writing results to outbuf)
	//static void renderFrameBlock(int y_start, int y_size, unsigned long *outbuf);
	static void renderFrameBlock(int y_start, int y_size, unsigned long *outbuf, double *time = NULL);

	// stuff done by master node to render a frame
	static void renderFrameMaster();

	// update on the root node and fill SceneUpdateData structure with the new info
	static void updateWorld(const float time, SceneUpdateData *sud);

	// update the local copy of the world using data received in the SceneUpdateData struct in input
	static void updateWorldInWorker(SceneUpdateData *sud);
		
	// logging && performance testing
	static void updateFrameStats(const float time);    
	static void updateFrameStatsWorker(const float time1, const float time2);

private:
	static unsigned long *renderingBuf;
	static World* world;
	static Light* light1;
	static Object* sphere1;
	static Vec3 campos;

	// ray tracing for pixel x,y
	static unsigned long getPixelColor(int x, int y);
	
	// precalculate unitdirs
	static Vec3 unitdirs[width][height];
	static const Vec3 Scene::getUnitDirForImageCoords(int x, int y);	
};

#endif  //__SCENE_H__
