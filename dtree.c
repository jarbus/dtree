// TODO
// - generalize buffer editing, apply to hint buffer
//   - add way to edit file name in app
// - generalize hint modes
// - travel mode
//   - qol: highlight matching characters as you fill them in
// - better drawing algs - durkin
// - add, copy, paste functionality
// - delete mode
// NOTE:
// SDLK is software character, SCANCODE is hardware
//
// https://www.parallelrealities.co.uk/tutorials/#shooter
// https://lazyfoo.net/tutorials/SDL/32_text_input_and_clipboard_handling/index.php
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_ttf.h>
#include "SDL_events.h"
#include "SDL_scancode.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Originally
// https://stackoverflow.com/questions/1644868/define-macro-for-debug-printing-in-c
// EDIT: https://stackoverflow.com/questions/1941307/debug-print-macro-in-c
#ifdef DEBUG
#define debug_print(...) fprintf(stderr, __VA_ARGS__);
#else
#define debug_print(...) /* no instruction */
#endif

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

typedef struct Node Node;
typedef struct Array Array;
typedef struct Graph Graph;
typedef struct Point Point;
typedef struct DoublePoint DoublePoint;
typedef struct App App;

enum Mode{Default, Edit, Travel, Delete};

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
    char* hint_text;
};

struct Graph {
    struct Node* root;
    int num_nodes;
    Node* selected;
};

static App app;
static Graph graph;
static enum Mode mode = Default;

static const char* HINT_CHARS = "asdfghjl;\0";
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
static int MAX_TEXT_LEN = 128;
// current idx of travel char
// array of all nodes to be traveled to
static Array* HINT_NODES;
// buffer to store current travel hint progress
static char* HINT_CHAR_BUF;
static int HINT_CHAR_LEN = 0, MAX_NUM_HINT_CHARS = 2;
static char* CURRENT_BUFFER;
static int CURRENT_BUFFER_SIZE, *CURRENT_BUFFER_LEN;
static char* FILENAME;
static int FILENAME_SIZE=64, FILENAME_LEN;
static unsigned int BUF_SIZE = 128;
static SDL_Color EDIT_COLOR = {255, 255, 255};
static SDL_Color HINT_COLOR = {255, 0, 0};


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
Array* initArray(size_t initialSize);
void* freeArray(Array *a);
void* popArray(Array *a);
void clearHintText();
void populateHintText(Node* node);
char* getModeName();
void switch2Default();
void switch2Edit();
void switch2Travel();

char* getModeName(){
    switch(mode) {
        case Default: return "DEFAULT";
        case Edit: return "EDIT";
        case Travel: return "TRAVEL";
        case Delete: return "DELETE";
        default: return NULL;
    }
}

bool isHintMode(){
    switch(mode){
        case Travel: return true;
        default: return false;
    }
}

// When a hint node is selected, this function is run
void hintFunction(Node* node){
    switch(mode){
        case Travel: graph.selected = node;
        case Delete: deleteNode(node);
        default: break;
    }
}


double clip(double num, double min, double max){
    if ( num < min )
        return min;
    if ( num > max )
        return max;
    return num;
}

void initSDL() {

    HINT_NODES = initArray(10);
    int rendererFlags, windowFlags;
    rendererFlags = SDL_RENDERER_ACCELERATED;
    windowFlags = SDL_WINDOW_RESIZABLE;

    debug_print("initing video\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    debug_print("inited video\n");

    app.window = SDL_CreateWindow("dtree", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);


    if (!app.window) {
        printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
        exit(1);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    debug_print("creating renderer\n");
    app.renderer = SDL_CreateRenderer(app.window, -1, rendererFlags);
    debug_print("created renderer\n");

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

    debug_print("TTF_FontHeight          : %d\n",TTF_FontHeight(FONT));
    debug_print("TTF_FontAscent          : %d\n",TTF_FontAscent(FONT));
    debug_print("TTF_FontDescent         : %d\n",TTF_FontDescent(FONT));
    debug_print("TTF_FontLineSkip        : %d\n",TTF_FontLineSkip(FONT));
    debug_print("TTF_FontFaceIsFixedWidth: %d\n",TTF_FontFaceIsFixedWidth(FONT));

    char *str=TTF_FontFaceFamilyName(FONT);
    if(!str)
        str="(null)";
    debug_print("TTF_FontFaceFamilyName  : \"%s\"\n",str);

    if(FONT == NULL) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
        // handle error
    }
}

void switch2Default(){
    debug_print("switching to default\n");
    if ( isHintMode() ){
        HINT_NODES = freeArray (HINT_NODES);
        HINT_CHAR_LEN = 0;
        if ( HINT_CHAR_BUF ) free(HINT_CHAR_BUF);
        HINT_CHAR_BUF = calloc(MAX_NUM_HINT_CHARS, sizeof(char));
    }
    CURRENT_BUFFER = NULL;
    CURRENT_BUFFER_SIZE = -1;
    mode = Default;
    debug_print("switched to default %p\n", HINT_NODES);
}

void switch2Edit(){
    mode = Edit;
    CURRENT_BUFFER = graph.selected->text;
    CURRENT_BUFFER_SIZE = MAX_TEXT_LEN;
    CURRENT_BUFFER_LEN = &graph.selected->text_len;
}

void switch2FilenameEdit(){
    mode = Edit;
    CURRENT_BUFFER = FILENAME;
    CURRENT_BUFFER_SIZE = FILENAME_SIZE;
    CURRENT_BUFFER_LEN = &FILENAME_LEN;
}

void activateHints(){
    populateHintText(graph.selected);
    CURRENT_BUFFER = HINT_CHAR_BUF;
    CURRENT_BUFFER_LEN = & HINT_CHAR_LEN;
    CURRENT_BUFFER_SIZE = MAX_NUM_HINT_CHARS;
}

void switch2Travel(){
    mode = Travel;
    activateHints();
}

void switch2Delete(){
    mode = Delete;
    activateHints();
}

void doKeyDown(SDL_KeyboardEvent *event) {
    if (event->repeat != 0) {
        return;
    }
    if (event->keysym.sym == SDLK_q) {
        printf("quitting...\n");
        app.quit = 1;
    }
}

void doKeyUp(SDL_KeyboardEvent *event) {
    if (event->repeat != 0) {
        return;
    }
    // up-front key-checks that apply to any mode
    switch(event->keysym.sym) {
        case SDLK_ESCAPE:
            switch2Default();
            return;
        case SDLK_BACKSPACE:
            if ( CURRENT_BUFFER && CURRENT_BUFFER_LEN >= 0)
                CURRENT_BUFFER[--*CURRENT_BUFFER_LEN] = '\0';
            return;
        case SDLK_MINUS:
            LAYER_MARGIN /= SCALE;
            RADIUS = (int) RADIUS / SCALE;
            THICKNESS = (int) THICKNESS / SCALE;
            LEFT_BOUNDARY -= SIDE_MARGIN * SCALE;
            RIGHT_BOUNDARY -= SCALE;
            return;
        case SDLK_EQUALS:
            LAYER_MARGIN *= SCALE;
            RADIUS = (int) RADIUS * SCALE;
            THICKNESS = (int) THICKNESS * SCALE;
            LEFT_BOUNDARY += SCALE;
            RIGHT_BOUNDARY += SCALE;
            return;
    }

    // mode-specific key-bindings
    switch(mode) {
    case Default:
    switch(event->keysym.sym) {
        case SDLK_o: { makeChild(graph.selected); return; }
        case SDLK_d: { removeNodeFromGraph(graph.selected); return; }
        case SDLK_t: { switch2Travel(); return; }
        case SDLK_e: { switch2Edit(); return; }
        case SDLK_r: { switch2FilenameEdit(); return; }
        case SDLK_x: { switch2Delete(); return; }
    }
    break; // end of Default bindings
    case Travel:
    switch(event->keysym.sym) {
        case SDLK_t: { switch2Default(); return;}
        case SDLK_e:
            switch2Default(); // exiting travel mode requires freeing some memory
            switch2Edit();
            return;
    }
    break; // end of Travel bindings
    case Edit: {
        return; // edit-mode specific key-binds don't really exist
    }
    case Delete: {
        switch(event->keysym.sym) {
            case SDLK_x: { switch2Default(); return; }
        }
    }
    }
}

void eventHandler(SDL_Event *event) {
    switch (event->type){
        case SDL_TEXTINPUT:
            if ( CURRENT_BUFFER && *CURRENT_BUFFER_LEN < CURRENT_BUFFER_SIZE ){
                debug_print("Adding text\n");
                int add_text = 1;
                // skip adding to buffer for hint modes if not a valid hint char
                if ( isHintMode() ){
                    add_text = 0;
                    for (int i = 0; i < strlen(HINT_CHARS); ++i){
                        if ( HINT_CHARS[i] == event->edit.text[0] ){
                            add_text = 1;
                            break;
                        }
                    }
                }

                if ( add_text )
                    CURRENT_BUFFER[(*CURRENT_BUFFER_LEN)++] = event->edit.text[0];
                debug_print("edit.text[0]: %c\n", event->edit.text[0]);
                debug_print("first: %c\n", graph.selected->text[0]);
                debug_print("new text, len %d: %s\n", graph.selected->text_len, graph.selected->text);
            }
            if ( isHintMode() ){
                // go to node specified by travel chars
                debug_print("handling travel input: %d/%d\n", HINT_CHAR_LEN, MAX_NUM_HINT_CHARS);
                // hardcode k to be parent
                if ( event->edit.text[0] == 'k' ){
                    hintFunction(graph.selected->p);
                    prepareScene();
                    populateHintText(graph.selected);
                }
                // check if any nodes travel text matches current input
                for (int i = 0; i < HINT_NODES->num; ++i) {
                    // continue if travel buffer != any travel text
                    if ( strcmp(HINT_CHAR_BUF, HINT_NODES->array[i]->hint_text) != 0 )
                        continue;
                    hintFunction(HINT_NODES->array[i]);
                    // reset travel text
                    debug_print("freeing travel chars\n");
                    if ( HINT_CHAR_BUF ) free(HINT_CHAR_BUF);
                    debug_print("freed travel chars\n");
                    HINT_CHAR_BUF = calloc(MAX_NUM_HINT_CHARS, sizeof(char));
                    HINT_CHAR_LEN = 0;
                    prepareScene();
                    populateHintText(graph.selected);
                    debug_print("changing nodes\n");
                }
                debug_print("handled travel input\n");
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
                debug_print("window resized from %dx%d to %dx%d\n", app.window_size.x, app.window_size.y, w, h);
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
    if ( node == NULL ) return;

    int x = (int) ((node->pos.x) * app.window_size.x);
    int y = (int) ((node->pos.y) * app.window_size.y);
    debug_print("%d %d from %f %f for node %p\n", x, y, node->pos.x, node->pos.y, node);
    debug_print("node %p\n", node);
    debug_print("children %p\n", node->children);
    debug_print("num children %lu\n", node->children->num);
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
    if ( isHintMode() ){
        if (strlen(node->hint_text) > 0){
            // position char in center of node
            message_pos.x = (int) ((node->pos.x) * app.window_size.x)  - (int)((TEXTBOX_WIDTH_SCALE) / 2) - RADIUS;
            message_pos.y = (int) ((node->pos.y) * app.window_size.y)  - (int)(TEXTBOX_HEIGHT / 2) - RADIUS;
            renderMessage(node->hint_text, message_pos, TEXTBOX_WIDTH_SCALE * 0.5, TEXTBOX_HEIGHT * 0.5, HINT_COLOR);
        }
    }

    /* draw edges between parent and child nodes */
    if (node != graph.root){
        int px = (int) ((node->p->pos.x) * app.window_size.x);
        int py = (int) ((node->p->pos.y) * app.window_size.y);
        SDL_RenderDrawLine(app.renderer, x, y, px, py);
    }

    for (int i=0; i<node->children->num; i++){
        debug_print("child %d / %lu: %p\n", i, node->children->num, node->children->array[i]);
        drawNode(node->children->array[i]);
    }
}

// testing SDL_TTF font rendering
void renderMessage(char* message, Point pos, double width, double height, SDL_Color color){
    if (!message)
        return;

    debug_print("rendering %s\n", message);

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

    debug_print("update node position to %lf %lf\n", node->pos.x, node->pos.y);

    /* if ( node == graph.selected ) */
    /*    printf("selected: %lf %lf\n", leftmost_bound, rightmost_bound); */

    /* dont update children if no children */
    if ( node->children->num == 0)
        return;

    /* split space for parent node up among children and update positions */
    double step = (rightmost_bound - leftmost_bound) / node->children->num;
    double iter = leftmost_bound;

    for (int i = 0; i < node->children->num; i += 1){
        debug_print("loop %d / %ld with step %lf\n", i, node->children->num, step);
        debug_print("%d %p\n", i, node->children->array[i]);
        debug_print("%p->%p [%lf %lf]\n", node, node->children->array[i], iter, iter+step);
        update_pos_children(node->children->array[i], iter, iter + step, level + LAYER_MARGIN);
        iter += step;
    }
}



void compute_root_bounds_from_selected_and_update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level){

    debug_print("crb start\n");
    /* If we reached the root, compute all child bounds relative to the root bounds */
    if (node == graph.root){
        update_pos_children(node, leftmost_bound, rightmost_bound, level);
        return;
    }
    debug_print("root not reached\n");

    /* we step through each sibling of the current node,
    giving each child the same size as the current node */
    double step_size = rightmost_bound - leftmost_bound;
    int child_idx = 0;

    debug_print("computing node children\n");
    /* compute idx of child relative to parent */
    for (int i = 0; i < node->p->children->num; i++) {
        if (node->p->children->array[i] == node){
            child_idx = i;
            break;
        }
    }
    debug_print("computed node children\n");
    /* compute the left and right boundaries of the parent node relative
    to the location of the child */
    double parent_left_bound = leftmost_bound - (step_size * child_idx);
    double parent_right_bound = rightmost_bound + (step_size * (node->p->children->num - child_idx-1));
    compute_root_bounds_from_selected_and_update_pos_children(node->p, parent_left_bound, parent_right_bound, level - LAYER_MARGIN);
    debug_print("crb end\n");
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

#ifdef DEBUG
    recursively_print_positions(graph.root, 0);
#endif

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
    debug_print("rendering %s\n", getModeName());
    renderMessage(getModeName(), mode_text_pos, strlen(getModeName()) * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, EDIT_COLOR);

    //Draw travel buffer
    Point travel_buf_pos;
    travel_buf_pos.x = (int) ((1.0 * app.window_size.x) - (HINT_CHAR_LEN * TEXTBOX_WIDTH_SCALE));
    travel_buf_pos.y = (int) ((0.9) * app.window_size.y);
    renderMessage(HINT_CHAR_BUF, travel_buf_pos, HINT_CHAR_LEN * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, HINT_COLOR);
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

void* freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->num = a->size = 0;
    free(a);
    return NULL;
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
    node->hint_text = calloc(MAX_NUM_HINT_CHARS, sizeof(char));
    return node;
}

Node* makeChild(Node* parent){
    Node* child = makeNode();
    child->p = parent;
    insertArray(parent->children, child);

    return child;
}

void deleteNode(Node* node){
    debug_print("DELETEING\n");
    /* Handle nodes that have already been deleted */
    if ( node == NULL ) {
        return;
    }
    /* delete each child */
    for (int i=0; i<node->children->num; i++){
        deleteNode( node->children->array[i] );
    }

    /* Then delete node */
    debug_print("deleting node %p\n", node);
    freeArray(node->children);
    free(node->text);
    debug_print("deleting travel text %p\n", node);
    free(node->hint_text);
    debug_print("deleted travel text %p\n", node);
    free(node);
    debug_print("deleted node %p\n", node);
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
    mode = Default;
}


void clearHintText() {
    debug_print("start clearHintText()\n");
    // return if already freed
    if (HINT_NODES == NULL || HINT_NODES->array == NULL){
        return;
    }
    debug_print("travel nodes %p, num %ld\n", HINT_NODES->array, HINT_NODES->num);
    // Clear all travel text in each travel node
    for (int i = 0; i < HINT_NODES->num; ++i) {
        for (int j = 0; j < MAX_NUM_HINT_CHARS; j++) {
            debug_print("node %d (%p)\n", i, HINT_NODES->array[i]);
            debug_print("hint_text %p\n", HINT_NODES->array[i]->hint_text);
            if (HINT_NODES->array[i]->hint_text){
                debug_print("in if %d: %s\n", j, HINT_NODES->array[i]->hint_text);
                HINT_NODES->array[i]->hint_text[j] = '\0';
            }
            debug_print("accessed travel text\n");
        }
    }
    // Clear travel node array
    debug_print("freeing HINT_NODES\n");
    if ( HINT_NODES->array )
        HINT_NODES = freeArray ( HINT_NODES );
    debug_print("end clearHintText()\n");
}

// Add all visible nodes to HINT_NODES
void populateHintNodes(Node* node){
    if ( !node )
        return;
    debug_print("adding travel node: %fx%f\n", node->pos.x, node->pos.y);
    if ( 0.0 < node->pos.x && node->pos.x < 1.0 && 0.0 < node->pos.y && node->pos.y < 1.0 ){
        insertArray(HINT_NODES, node);
        debug_print("added\n");
    }
    for (int i = 0; i < node->children->num; ++i) {
        debug_print("going to next child\n");
        populateHintNodes(node->children->array[i]);
    }

}
void populateHintText(Node* node){
    // Current method: add parent and children
    // New method: add every node that is visible
    debug_print("populate start\n");
    clearHintText();

    HINT_NODES = initArray(10);
    populateHintNodes(graph.root);
    debug_print("cleared\n");
    debug_print("%p: %s\n",node->p->hint_text, node->p->hint_text);
    // TODO PICKUP FROM HERE
    char** queue = calloc(8192, sizeof(char*));
    char* prefix = calloc(MAX_NUM_HINT_CHARS+1, sizeof(char));
    int front = 0, back=0, done=0;
    while ( !done ){
        debug_print("iterating while, back: %d, front: %d\n", back, front);
        for (int i = 0; i < strlen(HINT_CHARS); ++i) {
            queue[back] = calloc(MAX_NUM_HINT_CHARS+1, sizeof(char));
            strcpy(queue[back], prefix);
            queue[back][strlen(prefix)] = HINT_CHARS[i];
            debug_print("allocated %p: %s\n", queue[back], queue[back]);
            debug_print("===== back-front: %d string: %s travel_nodes->num %ld\n", back-front, queue[back], HINT_NODES->num);
            if ( back - front == HINT_NODES->num){
                done = 1;
                break;
            }
            back++;
        }
        prefix = queue[front++];
    }
    debug_print("loop 1\n");
    for (int i = 0; i < front; ++i) {
        free(queue[i]);
    }
    debug_print("loop 2, %ld HINT_NODES, front: %d back: %d\n", HINT_NODES->num, front, back);
    for (int i = 0; i < HINT_NODES->num; i++) {

        debug_print("%p: %s\n", HINT_NODES->array[i]->hint_text, queue[front + i]);
        strcpy(HINT_NODES->array[i]->hint_text, queue[front + i]);
        free(queue[front + i]);
        debug_print("freed\n");
    }
    debug_print("loop 3\n");
    for (int i = front + HINT_NODES->num; i < back; ++i) {
        free(queue[i]);

    }
    free(queue);

    // parent is always k
    strcpy(node->p->hint_text,"k\0");
    debug_print("==== populate end\n");
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
    if ( FILENAME == NULL ) return;
     FILE* output = fopen(FILENAME, "w");
    debug_print("write open done\n");
    writeChildrenStrings(output, graph.root, 0);
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
    if ( !fp )
        return;
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
        /* copy current line to child node, offset by number of tabs */
        strcpy(hierarchy[level]->text, ret + level);
        hierarchy[level]->text_len = strlen(ret)-1; // for some reason I need to have a -1 here
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

    FILENAME = calloc(64, sizeof(char));
    if ( argc > 1 ){
        strcpy(FILENAME, argv[1]);
        readfile();
    }
    else
        strcpy(FILENAME, "unnamed.txt");
    FILENAME_LEN = strlen(FILENAME);

    HINT_CHAR_BUF = calloc(MAX_NUM_HINT_CHARS + 1, sizeof(char));
    /* gracefully close windows on exit of program */
    atexit(SDL_Quit);
    app.quit = 0;

    SDL_Event e;
    /* Only updates display and processes inputs on new events */
    while ( !app.quit && SDL_WaitEvent(&e) ) {

        if ( HINT_NODES )
            debug_print("%ld/%ld\n", HINT_NODES ->num, HINT_NODES->size);

        /* Handle input before rendering */
        eventHandler(&e);
        debug_print("event handler done\n");

        debug_print("prepare scene start\n");
        prepareScene();
        debug_print("prepare scene end\n");

        debug_print("present scene start\n");
        presentScene();
        debug_print("present scene end\n");

        SDL_Delay(0);
        debug_print("end loop\n");
    }
    debug_print("saving...\n");
    write();
    debug_print("saved\n");

    if (HINT_CHAR_BUF)
        free(HINT_CHAR_BUF);
    free(FILENAME);
    /* delete nodes recursively, starting from root */
    deleteNode(graph.root);
    debug_print("deleted nodes\n");

    SDL_DestroyRenderer( app.renderer );
    SDL_DestroyWindow( app.window );

    //Quit SDL subsystems
    SDL_Quit();
    return 0;
}
