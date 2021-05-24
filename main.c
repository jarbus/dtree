// TODO
// https://www.parallelrealities.co.uk/tutorials/#shooter
// https://lazyfoo.net/tutorials/SDL/32_text_input_and_clipboard_handling/index.php
// for viewport, have a -inf -> inf range of nodes located in space, then have a way to map that onto the 0, 1 viewport depending on how far you want to see. entire graph will be laid out in space, and you just take the max and min coords of the farthest nodes you want to see and map that to the screen
// use pointers as references for objects?
// marks: `s for structs
// 		  `f for functions
// 		  `m for main function
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <stdio.h>
#include <stdbool.h>

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

#define RADIUS 100
#define THICKNESS 10

#define DEBUG 1


enum Mode{Default, Edit, Travel};

typedef struct Node Node;
typedef struct Array Array;
typedef struct Graph Graph;
typedef struct Point Point;
typedef struct App App;
struct Point {
	int x;
	int y;
};

struct App {
	SDL_Renderer *renderer;
	SDL_Window *window;
	bool quit;
	Point window_size;
};

struct Array {
  Node **array;
  size_t num;  /* number of children in array */
  size_t size; /* max size of array */
};

struct Node {
	struct Node* p;
	struct Array* children;
	struct Point pos;
};

struct Graph {
	struct Node* root;
	int num_nodes;
	Node* selected;
	enum Mode mode;
};

static App app;
static Graph graph;

void initSDL();
void doKeyDown(SDL_KeyboardEvent *event);
void doKeyUp(SDL_KeyboardEvent *event);
void eventHandler(SDL_Event *event);
void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawCircle(SDL_Renderer *surface, int n_cx, int n_cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawRing(SDL_Renderer *surface, int n_cx, int n_cy, int radius, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void presentScene();
void prepareScene();
Node* makeNode();
Node* makeChild(Node* parent);
void drawNode(Node* node);

void initSDL() {
	int rendererFlags, windowFlags;

	rendererFlags = SDL_RENDERER_ACCELERATED;

	windowFlags = SDL_WINDOW_RESIZABLE;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	app.window = SDL_CreateWindow("dtree", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);

	if (!app.window) {
		printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	app.renderer = SDL_CreateRenderer(app.window, -1, rendererFlags);

	if (!app.renderer) {
		printf("Failed to create renderer: %s\n", SDL_GetError());
		exit(1);
	}
	app.quit = 0;

	int w, h;
	SDL_GetWindowSize(app.window, &w, &h);
	app.window_size.x = w;
	app.window_size.y = h;
}

void doKeyDown(SDL_KeyboardEvent *event) {
	if (event->repeat == 0) {
		/* if (event->keysym.scancode == SDL_SCANCODE_Q) { */
		/* } */
		if (event->keysym.scancode == SDL_SCANCODE_Q) {
			app.quit = 1;
		}
	}
}

void doKeyUp(SDL_KeyboardEvent *event) {
	if (event->repeat == 0) {
		if (event->keysym.scancode == SDL_SCANCODE_O) {
			makeChild(graph.selected);
		}
	}
}

void eventHandler(SDL_Event *event) {
	switch (event->type)
	{
		case SDL_KEYDOWN:
			doKeyDown(&event->key);
			break;

		case SDL_KEYUP:
			doKeyUp(&event->key);
			break;

		case SDL_WINDOWEVENT:
			if(event->window.event == SDL_WINDOWEVENT_RESIZED) {

				int w, h;
				SDL_GetWindowSize(app.window, &w, &h);
				if ( DEBUG )
					printf("window resized from %dx%d to %dx%d\n", app.window_size.x, app.window_size.y, w, h);
				app.window_size.x = w;
				app.window_size.y = h;
			}
		break;

		case SDL_QUIT:
			exit(0);
			break;

		default:
			break;
	}
}

void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_SetRenderDrawColor(rend, r,g,b,a);
	SDL_RenderDrawPoint(rend, x, y);
}

void drawCircle(SDL_Renderer *surface, int n_cx, int n_cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	// if the first pixel in the screen is represented by (0,0) (which is in sdl)
	// remember that the beginning of the circle is not in the middle of the pixel
	// but to the left-top from it:

	double error = (double)-radius;
	double x = (double)radius - 0.5;
	double y = (double)0.5;
	double cx = n_cx - 0.5;
	double cy = n_cy - 0.5;

	while (x >= y) {
		set_pixel(surface, (int)(cx + x), (int)(cy + y), r, g, b, a);
		set_pixel(surface, (int)(cx + y), (int)(cy + x), r, g, b, a);

		if (x != 0) {
			set_pixel(surface, (int)(cx - x), (int)(cy + y), r, g, b, a);
			set_pixel(surface, (int)(cx + y), (int)(cy - x), r, g, b, a);
		}

		if (y != 0) {
			set_pixel(surface, (int)(cx + x), (int)(cy - y), r, g, b, a);
			set_pixel(surface, (int)(cx - y), (int)(cy + x), r, g, b, a);
		}

		if (x != 0 && y != 0) {
			set_pixel(surface, (int)(cx - x), (int)(cy - y), r, g, b, a);
			set_pixel(surface, (int)(cx - y), (int)(cy - x), r, g, b, a);
		}

		error += y;
		++y;
		error += y;

		if (error >= 0) {
			--x;
			error -= x;
			error -= x;
		}
	}
}


void drawRing(SDL_Renderer *surface, int n_cx, int n_cy, int radius, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	if ( thickness > radius ) {
		printf("Trying to draw a circle of radius %d but thickness %d is too big\n", radius, thickness);
	}
	for (int i=0; i< thickness; i++) {
		drawCircle(surface, n_cx, n_cy, radius - i, r, g, b, a);
	}
}

void drawNode(Node* node){
	int x = (int) (node->pos.x + app.window_size.x / 2);
	int y = (int) (node->pos.y + app.window_size.y / 2);
	printf("node %p\n", node);
	printf("children %p\n", node->children);
	printf("num children %lu\n", node->children->num);
	drawRing(app.renderer, x, y, RADIUS, THICKNESS, 0xFF, 0x00, 0x00, 0x00);
	for (int i=0; i<node->children->num; i++){
		printf("child %d / %lu: %p\n", i, node->children->num, node->children->array[i]);
		drawNode(node->children->array[i]);
	}
}


void prepareScene() {
	SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
	SDL_RenderClear(app.renderer);

    /* SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255); */
	/* loc x, loc y, radius, thickness, r g b a */
	/* drawRing(app.renderer, app.window_size.x / 2, app.window_size.y / 2, 50, 8, 0xFF, 0x00, 0x00, 0x00); */
	drawNode(graph.root);
    /* SDL_RenderDrawRect(app.renderer, &rect); */
}

void presentScene() {
	SDL_RenderPresent(app.renderer);
}

Array* initArray(size_t initialSize) {
	Array *a;
	a = malloc(sizeof(Array));
  	a->array = malloc(initialSize * sizeof(Node*));
  	a->num = 0;
  	a->size = initialSize;
	return a;
}

void insertArray(Array *a, Node* element) {
  // a->num is the number of used entries, because a->array[a->num++] updates a->num only *after* the array has been accessed.
  // Therefore a->num can go up to a->size
  if (a->num == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(Node*));
  }
  printf("insertArray %lu/%lu\n", a->num, a->size);
  a->array[a->num++] = element;
}

void freeArray(Array *a) {
  free(a->array);
  a->array = NULL;
  a->num = a->size = 0;
  free(a);
}

/* creates a new node at the origin */
Node* makeNode(){
	Node* node = malloc(sizeof(Node));
	node->children = initArray(5);
	node->children->num = 0;
	node->pos.x = 0;
	node->pos.y = 0;
	return node;
}

Node* makeChild(Node* parent){
	Node* child = makeNode();
	child->p = parent;
	insertArray(parent->children, child);
	child->pos.x = parent->pos.x;
	child->pos.y = parent->pos.y + 100;

	return child;
}

void deleteNode(Node* node){
	/* Handle nodes that have already been deleted */
	if ( node == NULL ) {
		return;
	}
	/* If leaf node, delete and return */
	if ( node->children->num <= 0 ){
		free(node);
		return;
	}
    /* Else, delete each child */
	for (int i=0; i<node->children->num; i++){
		deleteNode( node->children->array[i] );
	}
	/* Then delete node */
	free(node);
	freeArray(node->children);
}

void makeGraph(){
	graph.root = makeNode();
	graph.num_nodes = 0;
	graph.selected = graph.root;
	graph.mode = Default;
}



int main(int argc, char *argv[]) {

	/* intitialize app memory */
	memset(&app, 0, sizeof(App));

	/* set up window, screen, and renderer */
	initSDL();

	makeGraph();

	/* gracefully close windows on exit of program */
	atexit(SDL_Quit);

	bool quit = 0;

	SDL_Event e;
	/* Only updates display and processes inputs on new events */
	while ( !app.quit && SDL_WaitEvent(&e) ) {

		/* Handle Resize */
		switch (e.type){
		}

		/* Handle input before rendering */
		eventHandler(&e);

		prepareScene();

		presentScene();

		SDL_Delay(0);
	}

	/* delete nodes recursively, starting from root */
	printf("deleting\n");
	deleteNode(graph.root);
	printf("deleted\n");


	return 0;
}
