// TODO
// - hint mode
//   - qol: remove hint text that doesn't match current buffer
//   - add, copy, paste functionality
// - bugfixes
// - figure out what to do with modes
// - add "move" operation"
// NOTE:
// SDLK is software character, SCANCODE is hardware
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
typedef struct Buffer Buffer;

enum Mode{Default, Edit, FilenameEdit, Travel, Delete};
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
struct Buffer{
    char* buf;
    int len;
    int size;
};
struct Node {
    struct Node* p;
    struct Array* children;
    struct Point pos;
    float x_offset; /* offset wrt parent; "mod" in tree drawing algos */
    float rightmost; /* greatest descendant accumulated x_off wrt node*/
    float leftmost; /* smallest (negative) acc. x_off wrt node */
    struct Buffer text;
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
// array of all nodes to be hinted to
static Array* HINT_NODES;
// buffer to store current hint progress
static Buffer* CURRENT_BUFFER;
static Buffer HINT_BUFFER = {NULL, 0, 2};
static Buffer FILENAME_BUFFER = {NULL, 0, 64};
static SDL_Color EDIT_COLOR = {255, 255, 255};
static SDL_Color HINT_COLOR = {255, 0, 0};

void clearHintText();
double clip(double num, double min, double max);
unsigned int countTabs(char* string);
void deleteNode(Node* node);
void doKeyDown(SDL_KeyboardEvent *event);
void doKeyUp(SDL_KeyboardEvent *event);
void drawCircle(SDL_Renderer *surface, int n_cx, int n_cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawNode(Node* node);
void drawRing(SDL_Renderer *surface, int n_cx, int n_cy, int radius, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void endAtNewline(char* string, int textlen);
void eventHandler(SDL_Event *event);
void* freeArray(Array *a);
char* getModeName();
void handleTextInput(SDL_Event *event);
Array* initArray(size_t initialSize);
void initSDL();
bool isHintMode(enum Mode mode_param);
Node* makeNode();
Node* makeChild(Node* parent);
void calculate_offsets(Node* node);
void apply_offsets(Node* node, double depth, double offset);
void center_on_selected(Node* node, int selected_x, int selected_y);
void calculate_positions(Node* root, Node* selected);
void populateHintText(Node* node);
void presentScene();
void prepareScene();
void readfile();
void recursively_print_positions(Node* node, int level);
void* removeFromArray(Array *a, Node* node);
void removeNodeFromGraph(Node* node);
void renderMessage(char* message, Point pos, double width, double height, SDL_Color color);
void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void switch2(enum Mode to);
void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void write();
void writeChildrenStrings(FILE* file, Node* node, int level);

void readfile(){
    FILE* fp = fopen(FILENAME_BUFFER.buf, "r");
    if ( !fp )
        return;
    /* buffer to store lines */
    char* buf = calloc(MAX_TEXT_LEN, sizeof(char));
    /* load first line of file */
    char* ret = fgets(buf, MAX_TEXT_LEN, fp);
    /* keep references to all nodes that can have children added,
     * one per each level */
    Node** hierarchy = calloc(MAX_TEXT_LEN, sizeof(Node*));
    unsigned int level = 0;
    /* Load graph root manually */
    hierarchy[0] = graph.root;
    endAtNewline(ret, strlen(ret));
    strcpy(graph.root->text.buf, ret);
    graph.root->text.len = strlen(ret);
    while ( true ){
        /* loads next line */
        ret = fgets(buf, MAX_TEXT_LEN, fp);
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
        strcpy(hierarchy[level]->text.buf, ret + level);
        hierarchy[level]->text.len = strlen(ret)-1; // for some reason I need to have a -1 here
    }
    fclose(fp);
    free(buf);
    free(hierarchy);
}

 /* Recursively print children of nodes, with each child indented once from the parent */
void writeChildrenStrings(FILE* file, Node* node, int level){
    for(int i=0; i<level;i++)
        fprintf(file, "\t");
    fprintf(file, "%s\n", node->text.buf);
    for (int i=0; i<node->children->num; i++){
        writeChildrenStrings(file, node->children->array[i], level + 1);
    }
}

void write(){
    if ( FILENAME_BUFFER.buf == NULL ) return;
     FILE* output = fopen(FILENAME_BUFFER.buf, "w");
    debug_print("write open done\n");
    writeChildrenStrings(output, graph.root, 0);
    fclose(output);
}

char* getModeName(){
    switch(mode) {
        case Default: return "DEFAULT";
        case Edit: return "EDIT";
        case Travel: return "TRAVEL";
        case Delete: return "DELETE";
        default: return NULL;
    }
}

bool isHintMode(enum Mode mode_param){
    switch(mode_param){
        case Travel: return true;
        case Delete: return true;
        default: return false;
    }
}

// When a hint node is selected, this function is run
void hintFunction(Node* node){
    switch(mode){
        case Travel: graph.selected = node; return;
        case Delete: removeNodeFromGraph(node); return;
        default: break;
    }
    debug_print("did this\n");
}


double clip(double num, double min, double max){
    if ( num < min ) return min;
    if ( num > max ) return max;
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
    SDL_GetWindowSize(app.window, &app.window_size.x, &app.window_size.y);

    /* start SDL_ttf */
    if(TTF_Init()==-1){
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
    }
}


void activateHints(){
    populateHintText(graph.selected);
    CURRENT_BUFFER = &HINT_BUFFER;
}

void switch2(enum Mode to){
    if ( isHintMode(mode) ){
        HINT_NODES = freeArray (HINT_NODES);
        HINT_BUFFER.len = 0;
        if ( HINT_BUFFER.buf ) free(HINT_BUFFER.buf);
        HINT_BUFFER.buf = calloc(HINT_BUFFER.size, sizeof(char));
    }
    switch ( to ){
        case Edit:
            mode = Edit;
            CURRENT_BUFFER = &graph.selected->text;
            break;
        case Default:
            CURRENT_BUFFER = NULL;
            mode = Default;
            break;
        case FilenameEdit:
            mode = Edit;
            CURRENT_BUFFER = &FILENAME_BUFFER;
            break;
        case Travel:
            mode = Travel; break;
        case Delete:
            mode = Delete; break;
    }
    if ( isHintMode(to) )
        activateHints();
}

void doKeyUp(SDL_KeyboardEvent *event) {
    if (event->repeat != 0) {
        return;
    }
    if (event->keysym.sym == SDLK_q){
        app.quit = 1;
        return;
    }
    // up-front key-checks that apply to any mode
    switch(event->keysym.sym) {
        case SDLK_ESCAPE:
            switch2(Default);
            return;
        case SDLK_BACKSPACE:
            if ( CURRENT_BUFFER && CURRENT_BUFFER->len >= 0)
                CURRENT_BUFFER->buf[--CURRENT_BUFFER->len] = '\0';
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
            case SDLK_t: { switch2(Travel); return; }
            case SDLK_e: { switch2(Edit); return; }
            case SDLK_r: { switch2(FilenameEdit); return; }
            case SDLK_x: { switch2(Delete); return; }
        }
        break; // end of Default bindings
    case Travel:
        switch(event->keysym.sym) {
            case SDLK_t: { switch2(Default); return;}
            case SDLK_e:
                switch2(Default); // exiting travel mode requires freeing some memory
                switch2(Edit);
                return;
        }
        break; // end of Travel bindings
    case Edit: {
        return; // edit-mode specific key-binds don't really exist
    }
    case Delete: {
        switch(event->keysym.sym) {
            case SDLK_x: { switch2(Default); return; }
        }
    }
    default: break;
    }
}

void eventHandler(SDL_Event *event) {
    switch (event->type){
        case SDL_TEXTINPUT:
            handleTextInput(event);
            break;

        case SDL_KEYUP:
            doKeyUp(&event->key);
            break;

        case SDL_WINDOWEVENT:
            if(event->window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GetWindowSize(app.window, &app.window_size.x, &app.window_size.y);
            }
            break;

        case SDL_QUIT:
            exit(0);
            break;

        default:
            break;
    }
}

void handleTextInput(SDL_Event *event){
    if ( CURRENT_BUFFER && CURRENT_BUFFER->len < CURRENT_BUFFER->size ){
       debug_print("Adding text\n");
       int add_text = 1;
       // skip adding to buffer for hint modes if not a valid hint char
       if ( isHintMode(mode) ){
           add_text = 0;
           for (int i = 0; i < strlen(HINT_CHARS); ++i){
               if ( HINT_CHARS[i] == event->edit.text[0] ){
                   add_text = 1;
                   break;
               }
           }
       }

       if ( add_text )
           CURRENT_BUFFER->buf[(CURRENT_BUFFER->len)++] = event->edit.text[0];
       debug_print("edit.text[0]: %c\n", event->edit.text[0]);
       debug_print("new text, len %d: %s\n", graph.selected->text.len, graph.selected->text.buf);
   }
   if ( isHintMode(mode) ){
       // go to node specified by travel chars
       debug_print("handling travel input: %d/%d\n", HINT_BUFFER.len, HINT_BUFFER.size);
       // hardcode k to be parent
       if ( event->edit.text[0] == 'k' ){
           hintFunction(graph.selected->p);
           calculate_positions(graph.root, graph.selected);
           populateHintText(graph.selected);
       }
       // check if any nodes hint text matches current input
       for (int i = 0; i < HINT_NODES->num; ++i) {
           // continue if hint buffer != any hint text
           if ( strcmp(HINT_BUFFER.buf, HINT_NODES->array[i]->hint_text) != 0 )
               continue;
           hintFunction(HINT_NODES->array[i]);
           // reset hint text
           debug_print("freeing hint chars\n");
           if ( HINT_BUFFER.buf ) free( HINT_BUFFER.buf );
           debug_print("freed hint chars\n");
           HINT_BUFFER.buf = calloc(HINT_BUFFER.size, sizeof(char));
           HINT_BUFFER.len = 0;
           calculate_positions(graph.root, graph.selected);
           populateHintText(graph.selected);
           debug_print("changing nodes\n");
       }
       debug_print("handled hint input\n");
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

    int x = node->pos.x;
    int y = node->pos.y;
    debug_print("%d %d from %d %d for node %p\n", x, y, node->pos.x, node->pos.y, node);
    debug_print("node %p\n", node);
    debug_print("children %p\n", node->children);
    debug_print("num children %lu\n", node->children->num);
    /* draw red ring for unselected nodes, green for selected */
    if (node != graph.selected)
        drawRing(app.renderer, x, y, RADIUS, THICKNESS, 0xFF, 0x00, 0x00, 0x00);
    else
        drawRing(app.renderer, x, y, RADIUS, THICKNESS, 0x00, 0xFF, 0x00, 0x00);

    Point message_pos;
    message_pos.x = x  - (int)((TEXTBOX_WIDTH_SCALE*node->text.len) / 2);
    message_pos.y = y  - (int)(TEXTBOX_HEIGHT / 2);

    /* render node text */
    renderMessage(node->text.buf, message_pos, TEXTBOX_WIDTH_SCALE*node->text.len, TEXTBOX_HEIGHT, EDIT_COLOR);
    /* render hint text */
    if ( isHintMode(mode) ){
        if (strlen(node->hint_text) > 0){
            // position char in center of node
            message_pos.x = x  - (int)((TEXTBOX_WIDTH_SCALE) / 2) - RADIUS;
            message_pos.y = y  - (int)(TEXTBOX_HEIGHT / 2) - RADIUS;
            renderMessage(node->hint_text, message_pos, TEXTBOX_WIDTH_SCALE * 0.5, TEXTBOX_HEIGHT * 0.5, HINT_COLOR);
        }
    }

    /* draw edges between parent and child nodes */
    if (node != graph.root){
        int px = node->p->pos.x;
        int py = node->p->pos.y;
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


// recursive helper function for calculate_positions
// shifts over subtrees so that they do not overlap at any x-coordinate
void calculate_offsets(Node* node) {
    node -> x_offset  = 0;
    node -> rightmost = 0;
    node -> leftmost  = 0;
    double total_offset = 0;
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        calculate_offsets(child);
        if (i > 0) {
            // shift the current child (more than) far enough away from the
            // previous to guarantee that they subtrees will not overlap
            double offset = (node->children->array[i-1]->rightmost)-(child->leftmost)+1.;
            total_offset += offset;
            child->x_offset = total_offset;
        }
    }
    // center parent over children
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        child->x_offset -= total_offset/2.;
    }
    // calculate leftmost and rightmost for current node
    if(node->children->num >= 1) {
        Node* leftmost_child = node->children->array[0];
        node->leftmost = leftmost_child->x_offset + leftmost_child->leftmost;
        Node* rightmost_child = node->children->array[node->children->num-1];
        node->rightmost = rightmost_child->x_offset + rightmost_child->rightmost;
    }
}

// recursive helper function for calculate_positions
// accumulates offsets to assign correct [x,y] values to each node
void apply_offsets(Node* node, double depth, double offset) {
    node->pos.x = (int)(offset*2.5*RADIUS + app.window_size.x/2);
    node->pos.y = (int)(2.5*RADIUS*(depth+1.));
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        apply_offsets(child, depth+1., offset + child->x_offset);
    }
}

// shifts all node positions so that the selected node is at the center of the screen
void center_on_selected(Node* node, int selected_x, int selected_y) {
    node->pos.x += app.window_size.x/2 - selected_x;
    node->pos.y += app.window_size.y/2 - selected_y;
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        center_on_selected(child, selected_x, selected_y);
    }
}

// recomputes the coordinates of the nodes (i.e. populates pos field)
void calculate_positions(Node* root, Node* selected){
    calculate_offsets(root);
    apply_offsets(root, 0., 0.);
    center_on_selected(root, graph.selected->pos.x, graph.selected->pos.y);
}

/* Debug function, used to print locations of all nodes in indented hierarchy */
void recursively_print_positions(Node* node, int level){
    for (int i=0; i<level; i++){
        debug_print("\t");
    }
    debug_print("%d %d\n",node->pos.x, node->pos.y);

    for (int i = 0; i < node->children->num; i++) {
        recursively_print_positions(node->children->array[i], level + 1);
    }
}


/* re-computes graph and draws everything onto renderer */
void prepareScene() {
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);

    // recompute the coordinates of each node in the tree
    calculate_positions(graph.root, graph.selected);

#ifdef DEBUG
    recursively_print_positions(graph.root, 0);
#endif

    // Draw Graph
    drawNode(graph.root);

    // Draw filename
    Point filename_pos;
    filename_pos.x = (int) ((0.0) * app.window_size.x);
    filename_pos.y = (int) ((0.9) * app.window_size.y);
    renderMessage(FILENAME_BUFFER.buf, filename_pos, strlen(FILENAME_BUFFER.buf) * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, EDIT_COLOR);

    // Draw mode
    Point mode_text_pos;
    mode_text_pos.x = (int) ((0.0) * app.window_size.x);
    mode_text_pos.y = (int) ((0.0) * app.window_size.y);
    debug_print("rendering %s\n", getModeName());
    renderMessage(getModeName(), mode_text_pos, strlen(getModeName()) * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, EDIT_COLOR);

    //Draw hint buffer
    Point hint_buf_pos;
    hint_buf_pos.x = (int) ((1.0 * app.window_size.x) - (HINT_BUFFER.len * TEXTBOX_WIDTH_SCALE));
    hint_buf_pos.y = (int) ((0.9) * app.window_size.y);
    renderMessage(HINT_BUFFER.buf, hint_buf_pos, HINT_BUFFER.len * TEXTBOX_WIDTH_SCALE, TEXTBOX_HEIGHT, HINT_COLOR);
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
        debug_print("reallocing, num %ld size %ld\n", a->num, a->size);
        a->array = realloc(a->array, a->size * sizeof(void*));
        debug_print("realloced\n");
    }
    a->array[a->num++] = element;
}
void* removeFromArray(Array *a, Node* node){

    bool start_shifting = false;
    Node** nodes = a->array;
    for (int i = 0; i < a->num-1; ++i) {
        if ( nodes[i] == node )
            start_shifting = true;
        if ( start_shifting )
            nodes[i] = nodes[i+1];
    }
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
    node->text.buf = calloc(MAX_TEXT_LEN, sizeof(char));
    node->text.size = MAX_TEXT_LEN;
    node->text.len = 0;
    node->hint_text = calloc(HINT_BUFFER.size, sizeof(char));
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
    if ( node == NULL )
        return;
    /* delete each child */
    for (int i=0; i<node->children->num; i++)
        deleteNode( node->children->array[i] );

    /* Then delete node */
    debug_print("deleting node %p\n", node);
    freeArray(node->children);
    free(node->text.buf);
    free(node->hint_text);
    free(node);
    debug_print("deleted node %p\n", node);
}

void removeNodeFromGraph(Node* node){

    // remove node from array
    removeFromArray(node->p->children, node);
    removeFromArray(HINT_NODES, node);
    if ( node == graph.selected )
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
    debug_print("hint nodes %p, num %ld\n", HINT_NODES->array, HINT_NODES->num);
    // Clear all hint text in each hint node
    for (int i = 0; i < HINT_NODES->num; ++i) {
        for (int j = 0; j < HINT_BUFFER.size; j++) {
            debug_print("node %d (%p)\n", i, HINT_NODES->array[i]);
            debug_print("hint_text %p\n", HINT_NODES->array[i]->hint_text);
            if (HINT_NODES->array[i]->hint_text){
                debug_print("in if %d: %s\n", j, HINT_NODES->array[i]->hint_text);
                HINT_NODES->array[i]->hint_text[j] = '\0';
            }
            debug_print("accessed hint text\n");
        }
    }
    // Clear hint node array
    debug_print("freeing HINT_NODES\n");
    if ( HINT_NODES->array )
        HINT_NODES = freeArray ( HINT_NODES );
    debug_print("end clearHintText()\n");
}

// Add all visible nodes to HINT_NODES
void populateHintNodes(Node* node){
    if ( !node )
        return;
    debug_print("adding hint node: %dx%d\n", node->pos.x, node->pos.y);
    if ( RADIUS <= node->pos.x && node->pos.x < app.window_size.x-RADIUS &&\
            RADIUS < node->pos.y && node->pos.y < app.window_size.y-RADIUS) {
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
    char** queue = calloc(8192, sizeof(char*));
    char* prefix = calloc(HINT_BUFFER.size+1, sizeof(char));
    int front = 0, back=0, done=0;
    while ( !done ){
        debug_print("iterating while, back: %d, front: %d\n", back, front);
        for (int i = 0; i < strlen(HINT_CHARS); ++i) {
            queue[back] = calloc(HINT_BUFFER.size+1, sizeof(char));
            strcpy(queue[back], prefix);
            queue[back][strlen(prefix)] = HINT_CHARS[i];
            debug_print("allocated %p: %s\n", queue[back], queue[back]);
            debug_print("===== back-front: %d string: %s hint->num %ld\n", back-front, queue[back], HINT_NODES->num);
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

int main(int argc, char *argv[]) {
    /* set all bytes of App memory to zero */
    memset(&app, 0, sizeof(App));
    /* set up window, screen, and renderer */
    initSDL();

    makeGraph();

    FILENAME_BUFFER.buf = calloc(FILENAME_BUFFER.size, sizeof(char));
    if ( argc > 1 ){
        strcpy(FILENAME_BUFFER.buf, argv[1]);
        readfile();
    }
    else
        strcpy(FILENAME_BUFFER.buf, "unnamed.txt");
    FILENAME_BUFFER.len = strlen(FILENAME_BUFFER.buf);

    HINT_BUFFER.buf = calloc(HINT_BUFFER.size + 1, sizeof(char));
    /* gracefully close windows on exit of program */
    atexit(SDL_Quit);
    app.quit = 0;

    SDL_Event e;
    /* Only updates display and processes inputs on new events */
    while ( !app.quit && SDL_WaitEvent(&e) ) {
        if ( e.type == SDL_MOUSEMOTION) continue;

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

    if (HINT_BUFFER.buf)
        free(HINT_BUFFER.buf);
    free(FILENAME_BUFFER.buf);
    /* delete nodes recursively, starting from root */
    deleteNode(graph.root);
    debug_print("deleted nodes\n");

    SDL_DestroyRenderer( app.renderer );
    SDL_DestroyWindow( app.window );

    //Quit SDL subsystems
    SDL_Quit();
    return 0;
}
