// TODO
// - travel mode
// 	- better population of text
// 	- issue: handle invalid characters properly
// 	- qol: highlight matching characters as you fill them in
// - make parent visible
// - add way to edit file name in app
// - add, copy, paste functionality
// NOTE:
// SDLK is software character, SCANCODE is hardware
//
// https://www.parallelrealities.co.uk/tutorials/#shooter
// https://lazyfoo.net/tutorials/SDL/32_text_input_and_clipboard_handling/index.php
// marks: `s for structs
//		  `f for functions
//		  `m for main function
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_ttf.h>
#include "SDL_events.h"
#include "SDL_scancode.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define DEBUG 0

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

typedef struct Node Node;
typedef struct Array Array;
typedef struct Graph Graph;
typedef struct Point Point;
typedef struct DoublePoint DoublePoint;
typedef struct App App;

enum Mode{Default, Edit, Travel};

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

/* dynamic array to save me some headache, code is stolen from stack overflow */
struct Array {
  Node **array;
  size_t num;  /* number of children in array */
  size_t size; /* max size of array */
};

struct Node {
	struct Node* p;
	struct Array* children;
	struct DoublePoint pos;
	char* text;
	int text_len;
	char* travel_text;
};

struct Graph {
	struct Node* root;
	int num_nodes;
	Node* selected;
	enum Mode mode;
};

static App app;
static Graph graph;

static const char* TRAVEL_CHARS = "asdfghjl;";
// radius and thickness of node ring
static int RADIUS = 50;
static int THICKNESS = 10;
// Boundaries for tree
static double LEFT_BOUNDARY = 0.2;
static double RIGHT_BOUNDARY = 0.8;
// Spacing between layers of tree
static double LAYER_MARGIN = 0.3;
// Spacing between edge of screen and node
static double SIDE_MARGIN = 0.2;
static double SCALE = 1.5;
// Default Font
static TTF_Font* FONT;
static int FONT_SIZE = 40;
// width and heigh of text boxes
static int TEXTBOX_WIDTH_SCALE = 40;
static int TEXTBOX_HEIGHT = 100;
static int MAX_TEXT_LEN = 32;
// number of characters for travel hint text
static int MAX_NUM_TRAVEL_CHARS = 2;
// current idx of travel char
static int TRAVEL_CHAR_I = 0;
// array of all nodes to be traveled to
static Node** TRAVEL_NODES;
// length of TRAVEL_NODES
static int NUM_TRAVEL_NODES;
// buffer to store current travel hint progress
static char* TRAVEL_CHAR_BUF;
static char* FILENAME;
static unsigned int BUF_SIZE = 128;
static SDL_Color EDIT_COLOR = {255, 255, 255};
static SDL_Color TRAVEL_COLOR = {255, 0, 0};


double clip(double num, double min, double max);
void initSDL();
void doKeyDown(SDL_KeyboardEvent *event);
void doKeyUp(SDL_KeyboardEvent *event);
void eventHandler(SDL_Event *event);
void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawCircle(SDL_Renderer *surface, int n_cx, int n_cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawRing(SDL_Renderer *surface, int n_cx, int n_cy, int radius, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void renderMessage(char* message, Point pos, double width, double height, SDL_Color color);
void presentScene();
void prepareScene();
Node* makeNode();
Node* makeChild(Node* parent);
void drawNode(Node* node);
void compute_root_bounds_from_selected_and_update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void recursively_print_positions(Node* node, int level);
void write();
void writeChildrenStrings(FILE* file, Node* node, int level);
void readfile();
void deleteNode(Node* node);
void removeNodeFromGraph(Node* node);
void clearTravelText();
void populateTravelText(Node* node);
char* enum2Name(enum Mode mode);

char* enum2Name(enum Mode mode){
	if ( mode == Default )
		return "DEFAULT";
	if ( mode == Edit )
		return "EDIT";
	if ( mode == Travel )
		return "TRAVEL";
	return NULL;
}


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

	printf("initing video\n");
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	printf("inited video\n");

	app.window = SDL_CreateWindow("dtree", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);


	if (!app.window) {
		printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	printf("creating renderer\n");
	app.renderer = SDL_CreateRenderer(app.window, -1, rendererFlags);
	printf("created renderer\n");

	if (!app.renderer) {
		printf("Failed to create renderer: %s\n", SDL_GetError());
		exit(1);
	}
	app.quit = 0;

	int w, h;
	SDL_GetWindowSize(app.window, &w, &h);
	app.window_size.x = w;
	app.window_size.y = h;

	/* start SDL_ttf */
	if(TTF_Init()==-1)
	{
		printf("TTF_Init: %s\n", TTF_GetError());
		return;
	}
	atexit(TTF_Quit); /* remember to quit SDL_ttf */

	//this opens a font style and sets a size
	FONT = TTF_OpenFont("./assets/FiraSans-Regular.ttf", FONT_SIZE);

	printf("TTF_FontHeight          : %d\n",TTF_FontHeight(FONT));
	printf("TTF_FontAscent          : %d\n",TTF_FontAscent(FONT));
	printf("TTF_FontDescent         : %d\n",TTF_FontDescent(FONT));
	printf("TTF_FontLineSkip        : %d\n",TTF_FontLineSkip(FONT));
	printf("TTF_FontFaceIsFixedWidth: %d\n",TTF_FontFaceIsFixedWidth(FONT));

	char *str=TTF_FontFaceFamilyName(FONT);
	if(!str)
		str="(null)";
	printf("TTF_FontFaceFamilyName  : \"%s\"\n",str);

	if(FONT == NULL) {
		printf("TTF_OpenFont: %s\n", TTF_GetError());
		// handle error
	}
}

void switch2Default(){
	printf("switching to default\n");
	if ( graph.mode == Travel ){
		free (TRAVEL_NODES);
		TRAVEL_NODES = NULL;
		NUM_TRAVEL_NODES = 0;
		TRAVEL_CHAR_I = 0;
		if ( TRAVEL_CHAR_BUF ) free(TRAVEL_CHAR_BUF);
		TRAVEL_CHAR_BUF = calloc(MAX_NUM_TRAVEL_CHARS, sizeof(char));
	}
	graph.mode = Default;
	printf("switched to default %p\n", TRAVEL_NODES);
}

void doKeyDown(SDL_KeyboardEvent *event) {
	if (event->repeat == 0) {
		if (event->keysym.sym == SDLK_q) {
			printf("quitting...\n");
			app.quit = 1;
		}
	}
}

void doKeyUp(SDL_KeyboardEvent *event) {
	if (event->repeat == 0) {
		if ( graph.mode == Default ){
			if (event->keysym.sym == SDLK_o) {
				makeChild(graph.selected);
			}
			if (event->keysym.sym == SDLK_d) {
				removeNodeFromGraph(graph.selected);
			}
			if (event->keysym.sym == SDLK_h) {
				if ( graph.selected->children->num >= 1 ){
					graph.selected = graph.selected->children->array[0];
				}
			}
			if (event->keysym.sym == SDLK_l) {
				if ( graph.selected->children->num >= 1 ){
					graph.selected = graph.selected->children->array[graph.selected->children->num-1];
				}
			}
			if (event->keysym.sym == SDLK_k) {
				graph.selected = graph.selected->p;
			}
		}
		if (event->keysym.sym == SDLK_ESCAPE){
			switch2Default();
		}

		if (event->keysym.sym == SDLK_t) {
			if ( graph.mode == Edit ){}
			else if ( graph.mode != Travel ){
				graph.mode = Travel;
				populateTravelText(graph.selected);
			}
			else{
				switch2Default();
			}
		}

		if (event->keysym.sym == SDLK_BACKSPACE){
			if ( graph.selected->text_len > 0 ){
				graph.selected->text_len--;
				graph.selected->text[graph.selected->text_len] = '\0';
			}
		}
		if (event->keysym.sym == SDLK_e){
			if ( graph.mode != Edit ){
				graph.mode = Edit;
			}
		}
		if (event->keysym.sym == SDLK_MINUS){
			LAYER_MARGIN /= SCALE;
			RADIUS = (int) RADIUS / SCALE;
			THICKNESS = (int) THICKNESS / SCALE;
			LEFT_BOUNDARY -= SIDE_MARGIN * SCALE;
			RIGHT_BOUNDARY -= SCALE;
		}
		if (event->keysym.sym == SDLK_EQUALS){
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
		case SDL_TEXTINPUT:
			if (graph.mode == Edit){
				if ( graph.selected->text_len < MAX_TEXT_LEN )
					graph.selected->text[graph.selected->text_len++] = event->edit.text[0];
			}
			if (graph.mode == Travel){
				// go to node specified by travel chars
				printf("handling travel input: %d/%d\n", TRAVEL_CHAR_I, MAX_NUM_TRAVEL_CHARS);
				if ( TRAVEL_CHAR_I < MAX_NUM_TRAVEL_CHARS ) {
					TRAVEL_CHAR_BUF[TRAVEL_CHAR_I++] = event->edit.text[0];
					// check if any nodes travel text matches current input
					for (int i = 0; i < NUM_TRAVEL_NODES; ++i) {
						if ( strcmp(TRAVEL_CHAR_BUF, TRAVEL_NODES[i]->travel_text) == 0 ){
							graph.selected = TRAVEL_NODES[i];
							// reset travel text
							printf("freeing travel chars\n");
							if ( TRAVEL_CHAR_BUF ) free(TRAVEL_CHAR_BUF);
							printf("freed travel chars\n");
							TRAVEL_CHAR_BUF = calloc(MAX_NUM_TRAVEL_CHARS, sizeof(char));
							TRAVEL_CHAR_I = 0;
							populateTravelText(graph.selected);
							printf("changing nodes\n");
						}

					}
				}
				printf("handled travel input\n");


			}
			break;

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

/* Sets value of a single pixel on the screen */
void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_SetRenderDrawColor(rend, r,g,b,a);
	SDL_RenderDrawPoint(rend, x, y);
}

/* I stole this code, it works, do not touch
   n_cx and n_cy are the centers of the circle*/
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

/* Renders each node, then renders it's children and draws lines to the children */
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

	Point message_pos;
	message_pos.x = (int) ((node->pos.x) * app.window_size.x)  - (int)((TEXTBOX_WIDTH_SCALE*node->text_len) / 2);
	message_pos.y = (int) ((node->pos.y) * app.window_size.y)  - (int)(TEXTBOX_HEIGHT / 2);

	/* render node text */
	renderMessage(node->text, message_pos, TEXTBOX_WIDTH_SCALE*node->text_len, TEXTBOX_HEIGHT, EDIT_COLOR);
	/* render travel text */
	if ( graph.mode == Travel ){
		if (strlen(node->travel_text) > 0){
			// position char in center of node
			message_pos.x = (int) ((node->pos.x) * app.window_size.x)  - (int)((TEXTBOX_WIDTH_SCALE) / 2) - RADIUS;
			message_pos.y = (int) ((node->pos.y) * app.window_size.y)  - (int)(TEXTBOX_HEIGHT / 2) - RADIUS;
			renderMessage(node->travel_text, message_pos, TEXTBOX_WIDTH_SCALE * 0.5, TEXTBOX_HEIGHT * 0.5, TRAVEL_COLOR);
		}
	}

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

// testing SDL_TTF font rendering
void renderMessage(char* message, Point pos, double width, double height, SDL_Color color){
	if (!message)
		return;

	if ( DEBUG )
		printf("rendering %s\n", message);

	// create surface from string
	SDL_Surface* surfaceMessage =
		TTF_RenderText_Solid(FONT, message, color);
	//      ^ wrapped len

	// now you can convert it into a texture
	SDL_Texture* Message = SDL_CreateTextureFromSurface(app.renderer, surfaceMessage);

	SDL_Rect Message_rect; //create a rect
	Message_rect.x = pos.x;  //controls the rect's x coordinate
	Message_rect.y = pos.y; // controls the rect's y coordinte
	Message_rect.w = width; // controls the width of the rect
	Message_rect.h = height; // controls the height of the rect

	// Copy message to the renderer
	SDL_RenderCopy(app.renderer, Message, NULL, &Message_rect);

	// Don't forget to free your surface and texture
	SDL_FreeSurface(surfaceMessage);
	SDL_DestroyTexture(Message);
}


/* Given the bounds for a parent node, updates the positions of all children nodes */
void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level){

	/* update current node position */
	node->pos.x = (rightmost_bound + leftmost_bound) / 2;
	node->pos.y = level;

	if ( DEBUG )
		printf("update node position to %lf %lf\n", node->pos.x, node->pos.y);

	/* if ( node == graph.selected ) */
	/*	printf("selected: %lf %lf\n", leftmost_bound, rightmost_bound); */

	/* dont update children if no children */
	if ( node->children->num == 0)
		return;

	/* split space for parent node up among children and update positions */
	double step = (rightmost_bound - leftmost_bound) / node->children->num;
	double iter = leftmost_bound;

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

	printf("crb start\n");
	/* If we reached the root, compute all child bounds relative to the root bounds */
	if (node == graph.root){
		update_pos_children(node, leftmost_bound, rightmost_bound, level);
		return;
	}
	printf("root not reached\n");

	/* we step through each sibling of the current node,
	giving each child the same size as the current node */
	double step_size = rightmost_bound - leftmost_bound;
	int child_idx = 0;

	printf("computing node children\n");
	/* compute idx of child relative to parent */
	for (int i = 0; i < node->p->children->num; i++) {
		if (node->p->children->array[i] == node){
			child_idx = i;
			break;
		}
	}
	printf("computed node children\n");
	/* compute the left and right boundaries of the parent node relative
	to the location of the child */
	double parent_left_bound = leftmost_bound - (step_size * child_idx);
	double parent_right_bound = rightmost_bound + (step_size * (node->p->children->num - child_idx-1));
	compute_root_bounds_from_selected_and_update_pos_children(node->p, parent_left_bound, parent_right_bound, level - LAYER_MARGIN);
	printf("crb end\n");
}

/* Debug function, used to print locations of all nodes in indented hierarchy */
void recursively_print_positions(Node* node, int level){
	for (int i=0; i<level; i++){
		printf("\t");
	}
	printf("%lf %lf\n",node->pos.x, node->pos.y);

	for (int i = 0; i < node->children->num; i++) {
		recursively_print_positions(node->children->array[i], level + 1);
	}
}


/* re-computes graph and draws everything onto renderer */
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

	// Draw Graph
	drawNode(graph.root);

	// Draw filename
	Point filename_pos;
	filename_pos.x = (int) ((0.0) * app.window_size.x);
	filename_pos.y = (int) ((0.9) * app.window_size.y);
	renderMessage(FILENAME, filename_pos, strlen(FILENAME) * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, EDIT_COLOR);

	// Draw mode
	Point mode_text_pos;
	mode_text_pos.x = (int) ((0.0) * app.window_size.x);
	mode_text_pos.y = (int) ((0.0) * app.window_size.y);
	printf("rendering %s\n", enum2Name(graph.mode));
	renderMessage(enum2Name(graph.mode), mode_text_pos, strlen(enum2Name(graph.mode)) * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, EDIT_COLOR);
}

/* actually renders the screen */
void presentScene() {
	SDL_RenderPresent(app.renderer);
}

Array* initArray(size_t initialSize) {
	Array *a;
	a = calloc(1, sizeof(Array));
	a->array = calloc(initialSize, sizeof(void*));
	a->num = 0;
	a->size = initialSize;
	return a;
}

void insertArray(Array *a, void* element) {
	// a->num is the number of used entries, because a->array[a->num++] updates a->num only *after* the array has been accessed.
	// Therefore a->num can go up to a->size
	if (a->num == a->size) {
		a->size *= 2;
		a->array = realloc(a->array, a->size * sizeof(void*));
	}
	a->array[a->num++] = element;
}
void* popArray(Array *a){
	if (a->num > 0){
		void* ret = a->array[a->num];
		a->array[a->num] = NULL;
		a->num -= 1;
		return ret;
	}
	return NULL;
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
	node->text = calloc(MAX_TEXT_LEN, sizeof(char));
	node->text_len = 0;
	node->travel_text = calloc(MAX_NUM_TRAVEL_CHARS, sizeof(char));
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
	free(node->text);
	free(node->travel_text);
	free(node);
}

void removeNodeFromGraph(Node* node){

	bool start_shifting = false;
	Node** siblings = node->p->children->array;
	/* shift all siblings right of node over by one in the parent */
	for (int i = 0; i < node->p->children->num-1; ++i) {
		if ( siblings[i] == node )
			start_shifting = true;
		if ( start_shifting )
			siblings[i] = siblings[i+1];
	}
	popArray(node->p->children);
	graph.selected = node->p;
	deleteNode(node);

}

void makeGraph(){
	graph.root = makeNode();
	graph.num_nodes = 0;
	graph.selected = graph.root;
	graph.root->p = graph.root;
	graph.mode = Default;
}


void clearTravelText() {
	printf("start clearTravelText()\n");
	// return if already freed
	if ( TRAVEL_NODES == NULL || NUM_TRAVEL_NODES == 0 )
		return;
	printf("travel nodes %p, num %d\n", TRAVEL_NODES, NUM_TRAVEL_NODES);
	// Clear all travel text in each travel node
	for (int i = 0; i < NUM_TRAVEL_NODES; ++i) {
		for (int j = 0; j < MAX_NUM_TRAVEL_CHARS; j++) {
			printf("node %d (%p)\n", i, TRAVEL_NODES[i]);
			printf("travel_text %p\n", TRAVEL_NODES[i]->travel_text);
			if (TRAVEL_NODES[i]->travel_text)
				TRAVEL_NODES[i]->travel_text[j] = '\0';
			printf("accessed travel text\n");
		}
	}
	// Clear travel node array
	printf("freeing TRAVEL_NODES\n");
	if ( TRAVEL_NODES )
		free ( TRAVEL_NODES );
	TRAVEL_NODES = NULL;
	NUM_TRAVEL_NODES = 0;
	printf("end clearTravelText()\n");
}

void populateTravelText(Node* node){
	// Current method: add parent and children
	// New method: add every node that is visible
	printf("populate start\n");
	clearTravelText();
	node->p->travel_text[0] = 'k';
	// cancel if too many children
	if ( node->children->num > strlen(TRAVEL_CHARS) ){
		TRAVEL_NODES = NULL;
		return;
	}
	NUM_TRAVEL_NODES = node->children->num + 1;
	TRAVEL_NODES = (Node**) calloc(NUM_TRAVEL_NODES, sizeof(Node**));
	// Add parent
	TRAVEL_NODES[0] = node->p;
	// Add children
	for (int i = 0; i < node->children->num; ++i) {
		node->children->array[i]->travel_text[0] = TRAVEL_CHARS[i];
		printf("travel txt: %p\n", node->children->array[i]->travel_text);
		TRAVEL_NODES[i+1] = node->children->array[i];
	}
	printf("populate end\n");
}

/* Recursively print children of nodes, with each child indented once from the parent */
void writeChildrenStrings(FILE* file, Node* node, int level){
	for(int i=0; i<level;i++)
		fprintf(file, "\t");
	fprintf(file, "%s\n", node->text);
	for (int i=0; i<node->children->num; i++){
		writeChildrenStrings(file, node->children->array[i], level + 1);
	}
}


void write(){
	printf("write start\n");
 	FILE* output = fopen(FILENAME, "w");
	printf("write open done\n");
	writeChildrenStrings(output, graph.root, 0);
	printf("write children done\n");
	fclose(output);
}

unsigned int countTabs(char* string){
	unsigned int num_tabs = 0;
	for (int i=0; i<strlen(string); i++){
		if (string[i] == '\t')
			num_tabs++;
		else
			break;
	}
	return num_tabs;
}

void endAtNewline(char* string, int textlen){
	for (int i = 0; i < textlen; ++i) {
		if (string[i] == '\n') {
			string[i] = '\0';
			break;
		}
	}
}

void readfile(){
 	FILE* fp = fopen(FILENAME, "r");
	/* buffer to store lines */
	char* buf = calloc(BUF_SIZE, sizeof(char));
	/* load first line of file */
	char* ret = fgets(buf, BUF_SIZE, fp);
	/* keep references to all nodes that can have children added,
	 * one per each level */
	Node** hierarchy = calloc(BUF_SIZE, sizeof(Node*));
	unsigned int level = 0;
	/* Load graph root manually */
	hierarchy[0] = graph.root;
	endAtNewline(ret, strlen(ret));
	strcpy(graph.root->text, ret);
	graph.root->text_len = strlen(ret);
	while ( true ){
		/* loads next line */
		ret = fgets(buf, BUF_SIZE, fp);
		/* reached EOF */
		if ( ret != buf )
			break;
		/* remove any trailing newlines */
		endAtNewline(ret, strlen(ret));
		/* determine level in tree by number of tabs */
		level = countTabs(ret);
		/* create new child node */
		hierarchy[level] = makeChild(hierarchy[level-1]);
		/* copy current line to child node */
		strcpy(hierarchy[level]->text, ret + level);
		hierarchy[level]->text_len = strlen(ret);
	}
	fclose(fp);
	free(buf);
	free(hierarchy);
}

int main(int argc, char *argv[]) {
	/* set all bytes of App memory to zero */
	memset(&app, 0, sizeof(App));

	/* set up window, screen, and renderer */
	initSDL();

	makeGraph();

	if ( argc > 1 ){
		FILENAME = argv[1];
		readfile();
	}
	else{
		FILENAME = "test.txt";
	}

	TRAVEL_CHAR_BUF = calloc(MAX_NUM_TRAVEL_CHARS + 1, sizeof(char));
	/* gracefully close windows on exit of program */
	atexit(SDL_Quit);

	app.quit = 0;

	SDL_Event e;
	/* Only updates display and processes inputs on new events */
	while ( !app.quit && SDL_WaitEvent(&e) ) {

		/* Handle Resize */
		switch (e.type){
		}

		/* Handle input before rendering */
		eventHandler(&e);
		printf("event handler done\n");

		printf("prepare scene start\n");
		prepareScene();
		printf("prepare scene end\n");

		printf("present scene start\n");
		presentScene();
		printf("present scene end\n");

		SDL_Delay(0);
	}
	printf("saving...\n");
	write();
	printf("saved\n");

	if (TRAVEL_CHAR_BUF)
		free(TRAVEL_CHAR_BUF);
	/* delete nodes recursively, starting from root */
	deleteNode(graph.root);


	SDL_DestroyRenderer( app.renderer );
	SDL_DestroyWindow( app.window );

	//Quit SDL subsystems
	SDL_Quit();
	return 0;
}
