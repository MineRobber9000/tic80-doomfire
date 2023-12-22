#include "tic80.h"

// adds benchmarking functions
// press A/X to exit the game, outputting an average frame time and FPS
// to the console
// #define BENCHMARK

// we use rand because otherwise the fire effect is Boring(TM)
WASM_IMPORT("rand")
int rand(void);

// our framebuffer is the size of the width*height
#define FRAMEBUFFER_SIZE (WIDTH * HEIGHT)
int8_t framebuffer[FRAMEBUFFER_SIZE];

// plots a color on the framebuffer
// this is where the Magic(TM) happens
void plot(uint16_t k, int8_t v) {
	// don't let us draw negative colors!
	if (v<0) v=0;
	// force the color into the 0-31 range (0-30 in our case since we only
	// subtract and the highest value we give is 30)
	v = v&31;
	// if we've already drawn this color here don't bother
	if (framebuffer[k]==v) return;
	// get the x and y now
	uint16_t x = k%WIDTH;
	uint16_t y = k/WIDTH;
	// draw the color
	if (v>15) { // vbank1 color!
		// swap into vbank1 and plot the pixel
		vbank(1);
		// v-15 is always at least 1
		// a color of 0 is transparent (this is how the vbank0 colors
		// are shown)
		pix(x,y,v-15);
	} else {
		// if the last color at this spot was a vbank1 color, we need
		// to clear it
		// use color 0 to clear it
		if (framebuffer[k]>15) {
			vbank(1);
			pix(x,y,0);
		}
		// regardless we need to draw the pixel in vbank0
		vbank(0);
		pix(x,y,v);
	}
	// update our framebuffer array
	framebuffer[k]=v;
}

// fire spreading logic
void spreadfire(uint16_t k) {
	// get color at current pixel
	int8_t c = framebuffer[k];
	if (c==0) {
		// if it's zero, propagate the zero directly up
		plot(k-WIDTH,0);
	} else {
		// get a random number 0-3
		int8_t r = rand()&3;
		// k-WIDTH-r-1 is k-WIDTH, give or take 1 pixel (or take 2
		// pixels, if r==3), allows for propagation to the sides
		// meanwhile if r is odd, we subtract 1 from the flame color
		// (this is why we need the no-negative-numbers thing in
		// the plot() function)
		plot(k-WIDTH-((uint16_t)r)+1,c-(r&1));
	}
}

#ifdef BENCHMARK
// keep track of frametime, fps, and framecount
double frametime;
double fps;
int64_t frames;
bool _btnp;

// rolling average without storing the previous values
// works by... math garbage IDK
double rollingavg(double avg, int64_t n, double _new) {
	double _n = (double)n;
	return avg*((_n-1)/_n) + _new/_n;
}

// this is needed in order to avoid some weird conflict between WASI and TIC
WASM_IMPORT("time")
float _tic80_time();

WASM_IMPORT("snprintf")
int snprintf(char* s, size_t n, const char* format, ...);

#define BUFSIZ 40
char statsbuffer[BUFSIZ];

void traced(const char* format, int8_t color, double arg) {
	snprintf(statsbuffer,BUFSIZ,format,arg);
	trace(statsbuffer,color);
}

void tracei(const char* format, int8_t color, int64_t arg) {
	snprintf(statsbuffer,BUFSIZ,format,arg);
	trace(statsbuffer,color);
}

void display_stats() {
	trace("---",15);
	tracei("frames: %d",15,frames);
	traced("frametime (in ms): %0.3f",15,frametime);
	traced("FPS: %0.2f",15,fps);
}
#endif

WASM_EXPORT("BOOT")
void BOOT() {
	// empty framebuffer (all 0)
	for (uint16_t i=0;i<FRAMEBUFFER_SIZE;++i) framebuffer[i]=0;
	// clear screen (both vbanks)
	cls(0);
	vbank(1);
	cls(0);
	// bottom row is 30 (brightest fire)
	for (uint16_t j=0;j<WIDTH;++j) {
		framebuffer[(HEIGHT-1)*WIDTH+j]=30;
		pix(j,135,15);
	}
#ifdef BENCHMARK
	frametime=0;
	fps=0;
	frames=0;
	_btnp=false;
#endif
}

WASM_EXPORT("TIC")
void TIC() {
#ifdef BENCHMARK
	// if we press button 4, give info
	if (btn(4)) {
		if (!_btnp) display_stats();
		_btnp=true;
	} else {
		_btnp=false;
	}
	// get starting time()
	double start = _tic80_time();
#endif
	// update the fire!
	// call spreadfire() for every pixel below the first row
	// (the first row has nowhere to propagate anyways!)
	for (int x=0;x<WIDTH;++x) {
		for (int y=1;y<HEIGHT;++y) {
			spreadfire((uint16_t)((y*WIDTH)+x));
		}
	}
#ifdef BENCHMARK
	// get difference between current time() and starting time()
	// this is the time spent rendering the frame
	double delta = _tic80_time()-start;
	// now update the rolling average for frametime and FPS
	++frames;
	frametime = rollingavg(frametime,frames,delta);
	// FPS = 1000/frametime
	// the 1000 accounts for dividing the frametime (which is milliseconds)
	// by 1000 to make them seconds
	fps = rollingavg(fps,frames,(1000/delta));
#endif
}
