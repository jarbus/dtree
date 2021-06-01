// TODO
// https://www.parallelrealities.co.uk/tutorials/#shooter
// https://lazyfoo.net/tutorials/SDL/32_text_input_and_clipboard_handling/index.php
// marks: `s for structs
// 		  `f for functions
// 		  `m for main function
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <stdio.h>
#include <stdbool.h>

#define DEBUG 0

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

// radius and thickness of node ring
static int RADIUS = 50;
static int THICKNESS = 10;

static double LEFT_BOUNDARY = 0.3;
static double RIGHT_BOUNDARY = 0.7;

// Spacing between layers of tree
static double LAYER_MARGIN = 0.3;
static double SIDE_MARGIN = 0.3;

static double SCALE = 1.5;



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
struct DoublePoint {
	double x;
	double y;
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
	struct DoublePoint pos;
};

struct Graph {
	struct Node* root;
	int num_nodes;
	Node* selected;
	enum Mode mode;
};

static App app;
static Graph graph;

double clip(double num, double min, double max);
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
void compute_root_bounds_from_selected_and_update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void recursively_print_positions(Node* node, int level);

double clip(double num, double min, double max){
	if ( num < min )
		return min;
	if ( num > max )
		return max;
	return num;
}

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
		if (event->keysym.scancode == SDL_SCANCODE_H) {
			if ( graph.selected->children->num >= 1 ){
				graph.selected = graph.selected->children->array[0];
			}
		}
		if (event->keysym.scancode == SDL_SCANCODE_L) {
			if ( graph.selected->children->num >= 1 ){
				graph.selected = graph.selected->children->array[graph.selected->children->num-1];
			}
		}
		if (event->keysym.scancode == SDL_SCANCODE_K) {
			graph.selected = graph.selected->p;
		}
		if (event->keysym.scancode == SDL_SCANCODE_MINUS){
			LAYER_MARGIN /= SCALE;
			RADIUS = (int) RADIUS / SCALE;
			THICKNESS = (int) THICKNESS / SCALE;
			LEFT_BOUNDARY -= SIDE_MARGIN * SCALE;
			RIGHT_BOUNDARY -= SCALE;
		}
		if (event->keysym.scancode == SDL_SCANCODE_EQUALS){
			LAYER_MARGIN *= SCALE;
			RADIUS = (int) RADIUS * SCALE;
			THICKNESS = (int) THICKNESS * SCALE;
			LEFT_BOUNDARY += SCALE;
			RIGHT_BOUNDARY += SCALE;
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

void drawNode(Node* node) {

	if ( node == NULL )
		return;

	int x = (int) ((node->pos.x) * app.window_size.x);
	int y = (int) ((node->pos.y) * app.window_size.y);
	if ( DEBUG ){
		printf("%d %d from %f %f for node %p\n", x, y, node->pos.x, node->pos.y, node);
		printf("node %p\n", node);
		printf("children %p\n", node->children);
		printf("num children %lu\n", node->children->num);
	}
	/* draw red ring for unselected nodes, green for selected */
	if (node != graph.selected)
		drawRing(app.renderer, x, y, RADIUS, THICKNESS, 0xFF, 0x00, 0x00, 0x00);
	else
		drawRing(app.renderer, x, y, RADIUS, THICKNESS, 0x00, 0xFF, 0x00, 0x00);

	/* draw edges between parent and child nodes */
	if (node != graph.root){
		int px = (int) ((node->p->pos.x) * app.window_size.x);
		int py = (int) ((node->p->pos.y) * app.window_size.y);
		SDL_RenderDrawLine(app.renderer, x, y, px, py);
	}

	for (int i=0; i<node->children->num; i++){
		if ( DEBUG )
			printf("child %d / %lu: %p\n", i, node->children->num, node->children->array[i]);
		drawNode(node->children->array[i]);
	}
}


void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level){

	/* update current node position */
	node->pos.x = (rightmost_bound + leftmost_bound) / 2;
	node->pos.y = level;

	if ( DEBUG )
		printf("update node position to %lf %lf\n", node->pos.x, node->pos.y);

	/* dont update children if no children */
	if ( node->children->num == 0)
		return;

	/* split space for parent node up among children and update positions */
	double step = (rightmost_bound - leftmost_bound) / node->children->num;
	double iter = leftmost_bound;

	/* printf("beginning loop\n"); */
	for (int i = 0; i < node->children->num; i += 1){
		if ( DEBUG ){
			printf("loop %d / %ld with step %lf\n", i, node->children->num, step);
			printf("%d %p\n", i, node->children->array[i]);
			printf("%p->%p [%lf %lf]\n", node, node->children->array[i], iter, iter+step);
		}
		update_pos_children(node->children->array[i], iter, iter + step, level + LAYER_MARGIN);
		iter += step;
	}
}



void compute_root_bounds_from_selected_and_update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level){

	if (node == graph.root){
		update_pos_children(node, leftmost_bound, rightmost_bound, level);
		return;
	}

	double step_size = rightmost_bound - leftmost_bound;
	int child_idx;

	/* compute idx of child relative to parent */
	for (int i = 0; i < node->p->children->num; i++) {
		if (node->p->children->array[i] == node){
			child_idx = i;
			break;
		}
	}
	double parent_left_bound = leftmost_bound - (step_size * child_idx);
	double parent_right_bound = rightmost_bound + (step_size * (node->p->children->num - child_idx-1));
	compute_root_bounds_from_selected_and_update_pos_children(node->p, parent_left_bound, parent_right_bound, level - LAYER_MARGIN);
}


void recursively_print_positions(Node* node, int level){
	for (int i=0; i<level; i++){
		printf("\t");
	}
	printf("%lf %lf\n",node->pos.x, node->pos.y);

	for (int i = 0; i < node->children->num; i++) {
		recursively_print_positions(node->children->array[i], level + 1);
	}
}



void prepareScene() {
	SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
	SDL_RenderClear(app.renderer);

	/* fill up more space if at the root node, just a visual improvement */
	if ( graph.root == graph.selected )
		compute_root_bounds_from_selected_and_update_pos_children(graph.selected, 0.1, 0.9, 0.5);
	else
		compute_root_bounds_from_selected_and_update_pos_children(
			graph.selected,
				clip(SIDE_MARGIN * SCALE, 0.0, 1.0),
				clip(1.0 - (SIDE_MARGIN * SCALE), 0.0, 1.0),
				0.5);
	if ( DEBUG )
		recursively_print_positions(graph.root, 0);

	drawNode(graph.root);
}

void presentScene() {
	SDL_RenderPresent(app.renderer);
}

Array* initArray(size_t initialSize) {
	Array *a;
	a = calloc(1, sizeof(Array));
  	a->array = calloc(initialSize, sizeof(Node*));
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

	return child;
}

void deleteNode(Node* node){
	/* Handle nodes that have already been deleted */
	if ( node == NULL ) {
		return;
	}
    /* delete each child */
	for (int i=0; i<node->children->num; i++){
		deleteNode( node->children->array[i] );
	}
	/* Then delete node */
	freeArray(node->children);
	free(node);
}

void makeGraph(){
	graph.root = makeNode();
	graph.num_nodes = 0;
	graph.selected = graph.root;
	graph.mode = Default;
}

int main(int argc, char *argv[]) {

	/* set all bytes of App memory to zero */
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
	deleteNode(graph.root);


	SDL_DestroyRenderer( app.renderer );
	SDL_DestroyWindow( app.window );

    //Quit SDL subsystems
    SDL_Quit();

	printf("shutting down\n");

	return 0;
}
