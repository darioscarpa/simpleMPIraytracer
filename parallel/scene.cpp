#include "scene.h"
#include "console.h"

#include <float.h>

Vec3 Scene::campos(-3,0,1);
bool Scene::moveCamera = false;

World* Scene::world = NULL;
Light* Scene::light1 = NULL; 
Object* Scene::sphere1 = NULL;
unsigned long *Scene::renderingBuf = NULL;

bool Scene::showSplitting = false;

Vec3 Scene::unitdirs[Scene::width][Scene::height];

void Scene::init() {
	world = new World();

	{
		Material planemat(Colour(1,1,1), 0.5, 20, 0.6); 
		RayPlane* planegeom = new RayPlane(Plane(Vec3(0,0,1), 0));
		Object* groundplane = new Object(planemat, planegeom);
		world->insertObject(groundplane);
	}

	{
		Material mat(Colour(1,1,1), 0.5, 20, 0); 
		RayPlane* geom = new RayPlane(Plane(Vec3(0,1,0), -6));
		Object* theplane = new Object(mat, geom);
		world->insertObject(theplane);
	}

	{
		Material mat(Colour(0,1,0), 0.4, 20, 0.7); 
		RaySphere* geom = new RaySphere(Vec3(4,0,2), 0.7);
		sphere1 = new Object(mat, geom);
		world->insertObject(sphere1);
	}

	{
		Material mat(Colour(1,0,0), 0.4, 10, 0.5); 
		RaySphere* geom = new RaySphere(Vec3(4,0,2), 0.7);
		Object* thesphere = new Object(mat, geom);
		world->insertObject(thesphere);
	}

	light1 = new Light(Vec3(15,0,5), Colour(1, 1, 1) * 100);
	world->insertLight(light1);

	
	// precalculate unitdirs
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
			unitdirs[x][y] = getUnitDirForImageCoords(x, y);	
}

void Scene::setRenderingBuffer(unsigned long *buf) {
	renderingBuf = buf;
}

void Scene::destroy() {
	delete world;
	world = NULL;
}

//the below is for a camera pointing in the x direction, with y off to the left and z up.
const Vec3 Scene::getUnitDirForImageCoords(int x, int y)
{
	const float xfrac = (float)x / (float)width;
	const float yfrac = (float)y / (float)height;

	return normalise(Vec3(1.0f, -(xfrac - 0.5f), -(yfrac - 0.5f)));
}

static const float MAX_UBYTE_VAL = 255.0f;
unsigned long Scene::getPixelColor(int x, int y) {
//		const Ray ray(campos, getUnitDirForImageCoords(x, y));
		const Ray ray(campos, unitdirs[x][y]);
		Colour colour;
		world->getColourForRay(ray, colour, 0);
		if (showSplitting && Par::getInstance()->rank % 2) { colour.g += 0.10f; colour.r += 0.10f; }
		colour.positiveClipComponents();
		return RGB(colour.b*MAX_UBYTE_VAL, colour.g*MAX_UBYTE_VAL, colour.r*MAX_UBYTE_VAL);
}

Timer xtimer;

void Scene::renderFrameBlock(int y_start, int y_size, unsigned long *outbuf, double *time) {
	const double time1 = xtimer.getSecondsElapsed();	
	if(showSplitting && y_start!=0) {		
		for(int x=0; x<width; ++x) {
			getPixelColor(x, y_start); // useless, but we don't want to false stats (lowering rendering time)
									   // by not rendering the pixels "under" the division lines! :D
			(*outbuf++) = RGB(0,0,1.0*MAX_UBYTE_VAL);
		}
		y_start++;
		y_size--;
	}
	for(int y=y_start; y<y_size+y_start; ++y)
		for(int x=0; x<width; ++x) 
			(*outbuf++) = getPixelColor(x, y); // color * y + x;//getPixelColor(x,y);		
	const double time2 = xtimer.getSecondsElapsed();
	Scene::updateFrameStatsWorker(time1, time2);
	if (time!=NULL) 
		*time = time2-time1;
}

void Scene::updateWorld(const float time, SceneUpdateData *sud) {
	if(light1)
		light1->pos = Vec3(0 + cos(time) * 5, sin(time) * 5, 5);
	if(sphere1)
		static_cast<RaySphere&>(sphere1->getGeometry()).centerpos = 
		Vec3(3 + cos(time * 0.2) * 2, sin(time * 0.2) * 2, 1);

	if (Scene::moveCamera)
		campos.x = campos.x + (cos(time) * 0.1);

	sud->light1Pos = light1->pos;
	sud->sphere1Pos = static_cast<RaySphere&>(sphere1->getGeometry()).centerpos;
	sud->cameraPos = campos;
}

void Scene::updateWorldInWorker(SceneUpdateData *sud) {
	light1->pos = sud->light1Pos;
	static_cast<RaySphere&>(sphere1->getGeometry()).centerpos = sud->sphere1Pos;
	campos = sud->cameraPos;
}

void Scene::renderFrameMaster() {
	const float time = xtimer.getSecondsElapsed();
	SceneUpdateData sud;
	updateWorld(time, &sud);
	
	Par::getInstance()->master(&sud, renderingBuf);

	updateFrameStats(time);
}

void Scene::updateFrameStatsWorker(const float time1, const float time2) {
	static int callcont = 0;
	static float sum = 0;
	
	callcont++;

	const float diff = time2 - time1;
	sum += diff;
	
	*Par::log  << callcont << " " << time2-time1 <<  " " << sum/callcont  << std::endl;
}

void Scene::updateFrameStats(const float time) {
	static int framecont = 0;
	static double starttime = xtimer.getSecondsElapsed();
	static double currtime; 

	static float maxRenderingTime = 0;
	static float minRenderingTime = FLT_MAX;
	static float avgRenderingTime;

	static float maxFps = 0;
	static float minFps = FLT_MAX;
	static float avgFps;
	static float fps = 0.0f;
	static double rtime;

	currtime = xtimer.getSecondsElapsed();
	framecont++;

	updateFrameStatsWorker(time, currtime);

	rtime = currtime - time;
	fps = fps*0.9 + 0.1*(1.0/rtime);	
	
	if (fps > maxFps) maxFps = fps;
	if (fps < minFps) minFps = fps;

	if (rtime > maxRenderingTime) maxRenderingTime = rtime;
	if (rtime < minRenderingTime) minRenderingTime = rtime;
	
	avgRenderingTime = (currtime-starttime)/(double)framecont;
	avgFps = framecont/(currtime-starttime);
	
	static double lastUpdate = 0;	
	static double updatefrequency = 0.2;
	if (currtime-lastUpdate >= updatefrequency) {
		if (Console::enabled) {
			for (int n = 0; n < 10; n++) {			// don't tell anyone I did this
			  printf( "\n\n\n\n\n\n\n\n\n\n" );
			} 
			printf("   fps %3.2f\n", fps);
			printf("\n");
			//printf("maxfps %3.2f\n", maxFps);
			//printf("minfps %3.2f\n", minFps);
			printf("avgfps %3.2f\n", avgFps);
			printf("\n");
			printf(" maxRT %1.5f\n", maxRenderingTime);
			printf(" minRT %1.5f\n", minRenderingTime);
			printf(" avgRT %1.5f\n", avgRenderingTime);			
		}
		lastUpdate = currtime;
	}
}

/*
void Scene::renderFrame() {
	const float time = xtimer.getSecondsElapsed();
	updateWorld(time);

	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
			*(renderingBuf + (x + (y * Scene::width))) = getPixelColor(x,y);
	
	//renderFrameBlock(0, Scene::height, renderingBuf);
	
	//for(int x=0; x<width; ++x)
	//	for(int y=0; y<height; ++y)
	//		graphics.drawPixel(x, y, imgbuf[x][y]);
	
}

void Scene::renderFrameSplitted() {	
	const float time = xtimer.getSecondsElapsed();
	updateWorld(time);
	
	int y_size = 100;
	for (int y = 0; y < Scene::height; y += y_size) {
		renderFrameBlock(y, y_size, renderingBuf + y * Scene::width);		
	}
}*/
