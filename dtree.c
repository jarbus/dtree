// TODO
// - add, copy, paste functionality for buffers
// - make hints clearly separated
// - better enter functionality for text, given a position
// - add cursor for buffers
// - better scaling
// - add a FILE-OPEN key: a node buffer will be a file name, and pressing a key on the node opens it
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

enum Mode{Travel, Edit, FilenameEdit, Delete, Cut, Paste, MakeChild};
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
    int x_offset; /* offset wrt parent; "mod" in tree drawing algos */
    int rightmost; /* greatest descendant accumulated x_off wrt node*/
    int leftmost; /* smallest (negative) acc. x_off wrt node */
    struct Buffer text;
    char* hint_text;
};
struct Graph {
    struct Node* root;
    int num_nodes;
    Node* selected;
};

/* User Customizable Variables*/
static Buffer HINT_BUFFER =      {NULL, 0, 2};              // {ptr, cur_len, max_size} for hints
static Buffer FILENAME_BUFFER =  {NULL, 0, 64};             // {ptr, cur_len, max_size} for fname
static SDL_Color EDIT_COLOR =    {220, 220, 220};           // RGB
static SDL_Color HINT_COLOR =    {220, 0, 0};               // RGB
static int SELECTED_COLOR[4] =   {0, 220, 0, 0};
static int UNSELECTED_COLOR[4] = {0, 55, 0, 0};
static int CUT_COLOR[4] =   {0, 0, 220, 0};
static int BACKGROUND_COLOR[4] = {15, 15, 15, 255};
static int TEXTBOX_WIDTH_SCALE = 25;                        // width of char
static int TEXTBOX_HEIGHT = 50;                            // Heigh of char
static int MAX_TEXT_LEN = 128;                              // Max Num of chars in a node
static int NUM_CHARS_B4_WRAP = 15;
// radius and thickness of node box
static int RADIUS = 50;
static int THICKNESS = 10;
static int FONT_SIZE = 40;
static char* FONT_NAME = "./assets/SourceCodePro-Regular.otf";   // Default Font name
static const char* HINT_CHARS = "adfghjl;\0";              // characters to use for hints

/* Globals */
static App app;                 // App object that contains rendering info
static Graph graph;             // Graph object that contains nodes
static enum Mode mode = Travel;// Global mode
static TTF_Font* FONT;          // Global Font object
static Array* HINT_NODES;       // array of all nodes to be hinted to
static Buffer* CURRENT_BUFFER;  // buffer to store current hint progress
static Node* CUT = NULL;
static bool TOGGLE_MODE=0;
static char* TOGGLE_INDICATOR = "MODE PERSIST\0";

void activateHints();
void clearHintText();
void clearBuffer(Buffer* HINT_BUFFER);
double clip(double num, double min, double max);
unsigned int countTabs(char* string);
void deleteNode(Node* node);
void doKeyUp(SDL_KeyboardEvent *event);
void doKeyDown(SDL_KeyboardEvent *event);
void drawBox(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawNode(Node* node);
void drawBorder(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void endAtNewline(char* string, int textlen);
void eventHandler(SDL_Event *event);
void* freeArray(Array *a);
char* getModeName();
int getWidth (char* message, bool wrap);
int getHeight (char* message, bool wrap);
void handleTextInput(SDL_Event *event);
Array* initArray(size_t initialSize);
void initSDL();
void insertArray(Array *a, void* element);
bool isEditMode(enum Mode mode_param);
bool isHintMode(enum Mode mode_param);
Node* makeNode();
Node* makeChild(Node* parent);
int get_depth(Node* node);
void calculate_x_offsets(Node* node);
void calculate_max_heights(Node* node, int level, int* max_heights);
void apply_offsets(Node* node, int x_offset, int level, int* y_levels);
void center_on_selected(Node* node, int selected_x, int selected_y);
void calculate_positions(Node* root, Node* selected);
void populateHintText(Node* node);
void presentScene();
void prepareScene();
void readfile();
void recursively_print_positions(Node* node, int level);
void removeFromArray(Array *a, Node* node);
void removeNodeFromGraph(Node* node);
void renderMessage(char* message, Point pos, double scale, SDL_Color color, bool wrap);
void set_pixel(SDL_Renderer *rend, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void switch2(enum Mode to);
void update_pos_children(Node* node, double leftmost_bound, double rightmost_bound, double level);
void write2File();
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
    for (int i=0; i<node->children->num; i++)
        writeChildrenStrings(file, node->children->array[i], level + 1);
}

void write2File(){
    if ( FILENAME_BUFFER.buf == NULL ) return;
    FILE* output = fopen(FILENAME_BUFFER.buf, "w");
    writeChildrenStrings(output, graph.root, 0);
    fclose(output);
}

char* getModeName(){
    switch(mode) {
        case Edit: return "EDIT";
        case Travel: return "TRAVEL";
        case Delete: return "DELETE";
        case FilenameEdit: return "FILENAME EDIT";
        case Cut: return "CUT";
        case Paste: return "PASTE";
        case MakeChild: return "MAKE CHILD";
        default: return NULL;
    }
}

bool isHintMode(enum Mode mode_param){
    switch(mode_param){
        case Travel: return true;
        case Delete: return true;
        case Cut: return true;
        case Paste: return true;
        case MakeChild: return true;
        default: return false;
    }
}
bool isEditMode(enum Mode mode_param){
    switch(mode_param){
        case FilenameEdit: return true;
        case Edit: return true;
        default: return false;
    }
}

// When a hint node is selected, this function is run
void hintFunction(Node* node){
    switch(mode){
        case Travel: graph.selected = node; break;
        case Delete: removeNodeFromGraph(node); break;
        case Cut: CUT = node; switch2(Paste); break;
        case MakeChild: makeChild(node); break;
        case Paste:
            if ( !CUT ) break;
            removeFromArray(CUT->p->children, CUT);
            insertArray(node->children, CUT);
            CUT->p = node;
            calculate_positions(graph.root, graph.selected);
            CUT = NULL;
            switch2( Cut );
            break;
        default: break;
    }
    if ( TOGGLE_MODE == false && mode != Paste )
        switch2( Travel );
}


double clip(double num, double min, double max){
    if ( num < min ) return min;
    if ( num > max ) return max;
    return num;
}
int min(int a, int b){ if (a < b ) return a; else return b; }

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
    FONT = TTF_OpenFont(FONT_NAME, FONT_SIZE);
}

void activateHints(){
    populateHintText(graph.selected);
    CURRENT_BUFFER = &HINT_BUFFER;
}

void switch2(enum Mode to){
    if ( isHintMode(mode) ){
        HINT_NODES = freeArray (HINT_NODES);
        clearBuffer(&HINT_BUFFER);
    }
    switch ( to ){
        case Edit:
            mode = Edit;
            CURRENT_BUFFER = &graph.selected->text;
            break;
        case FilenameEdit:
            mode = Edit;
            CURRENT_BUFFER = &FILENAME_BUFFER;
            break;
        case Travel:
            if (mode == Travel) CUT = NULL; // clear cut node on a double escape
            mode = Travel;
            TOGGLE_MODE = 0;
            break;
        case MakeChild: mode = MakeChild; break;
        case Delete:    mode = Delete; break;
        case Cut:       mode = Cut; break;
        case Paste:     mode = Paste; break;
    }
    if ( isHintMode(to) )
        activateHints();
}

void doKeyDown(SDL_KeyboardEvent *event) {
    if (event->repeat != 0) {
        if ( isEditMode(mode) ){
            switch(event->keysym.sym) {
                case SDLK_BACKSPACE:
                    if ( CURRENT_BUFFER && CURRENT_BUFFER->len >= 0)
                        CURRENT_BUFFER->buf[--CURRENT_BUFFER->len] = '\0';
            }
        }
        return;
    }
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
            switch2(Travel);
            return;
    }

    // mode-specific key-bindings
    switch(mode) {
    case Travel:
        switch(event->keysym.sym) {
            case SDLK_o: { switch2(MakeChild); return; }
            case SDLK_e: { switch2(Edit); return; }
            case SDLK_r: { switch2(FilenameEdit); return; }
            case SDLK_x: { switch2(Delete); return; }
            case SDLK_m: { switch2(Cut); return; }
            case SDLK_p: { switch2(Paste); return; }
            case SDLK_s: { clearBuffer(&graph.selected->text); switch2(Edit); return; }
            case SDLK_c: { TOGGLE_MODE = 1; return; }
            case SDLK_w: { write2File(); return; }
        }
        break; // end of Travel bindings
    case Edit: {
        switch(event->keysym.sym){
            case SDLK_BACKSPACE:
                if ( CURRENT_BUFFER && CURRENT_BUFFER->len >= 0)
                    CURRENT_BUFFER->buf[--CURRENT_BUFFER->len] = '\0';
                return;
            case SDLK_RETURN:
                if ( CURRENT_BUFFER && CURRENT_BUFFER->len >= 0)
                    CURRENT_BUFFER->buf[CURRENT_BUFFER->len++] = '\n';
                return;
        }
    }
    case Delete: {
        switch(event->keysym.sym)
            case SDLK_x: { switch2(Travel); return; }
    }
    default: break;
    }
}

void eventHandler(SDL_Event *event) {
    switch (event->type){
        case SDL_TEXTINPUT: { handleTextInput(event); break; }
        case SDL_KEYUP:     { doKeyUp(&event->key);   break; }
        case SDL_KEYDOWN:   { doKeyDown(&event->key); break; }
        case SDL_QUIT:      { exit(0);                break; }
        case SDL_WINDOWEVENT:
            if(event->window.event == SDL_WINDOWEVENT_RESIZED)
                SDL_GetWindowSize(app.window, &app.window_size.x, &app.window_size.y);
            break;
        default: break;
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
           clearBuffer(&HINT_BUFFER);
           debug_print("freed hint chars\n");

           debug_print("calculating positions\n");
           calculate_positions(graph.root, graph.selected);
           debug_print("populating hint text\n");
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


void drawBox(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int offset, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
    SDL_Rect rect;
    rect.x = (int) n_cx - ( len  / 2 )  - offset;
    rect.y = (int) n_cy - ( height / 2 ) - offset;
    rect.w = len + offset + offset;
    rect.h = height + offset + offset;
    SDL_SetRenderDrawColor(surface, r, g, b, a);
    SDL_RenderDrawRect(surface, &rect);
}

void drawBorder(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
    for (int i = 0; i < thickness; ++i)
        drawBox(surface, n_cx , n_cy, len, height, i, r, g, b, a);
}

/* Renders each node, then renders it's children and draws lines to the children */
void drawNode(Node* node) {
    if ( node == NULL ) return;

    int x = node->pos.x;
    int y = node->pos.y;
    debug_print("node %p\n", node);
    debug_print("children %p\n", node->children);
    debug_print("num children %lu\n", node->children->num);
    /* draw red ring for unselected nodes, green for selected */
    int width  = getWidth(node->text.buf, 1);
    int height = getHeight(node->text.buf, 1);
    if (node == CUT)
        drawBorder(app.renderer, x, y, width, height, THICKNESS, CUT_COLOR[0], CUT_COLOR[1], CUT_COLOR[2], CUT_COLOR[3]);
    else if (node == graph.selected)
        drawBorder(app.renderer, x, y, width, height, THICKNESS, SELECTED_COLOR[0], SELECTED_COLOR[1], SELECTED_COLOR[2], SELECTED_COLOR[3]);
    else
        drawBorder(app.renderer, x, y, width, height, THICKNESS, UNSELECTED_COLOR[0], UNSELECTED_COLOR[1], UNSELECTED_COLOR[2], UNSELECTED_COLOR[3]);

    Point message_pos;
    message_pos.x = x - (width / 2);
    message_pos.y = y - (height / 2);

    /* render node text */
    renderMessage(node->text.buf, message_pos, 1.0, EDIT_COLOR, 1);
    /* render hint text */
    if ( isHintMode(mode) && strlen(node->hint_text) > 0 ){
        // dont render hint text that doesn't match hint buffer
        bool render_hint = true;
        if (HINT_BUFFER.len > 0){
            for (int i = 0; i < HINT_BUFFER.len; ++i) {
                if ( HINT_BUFFER.buf[i] != node->hint_text[i] ){
                    render_hint = false;
                    break;
                }
            }
        }

        // position char in center of node
        if ( render_hint ) {
            message_pos.x = x  - (int)(width / 2) - THICKNESS;
            message_pos.y = y  - (int)(height / 2) - THICKNESS - RADIUS;
            renderMessage(node->hint_text, message_pos, 0.75, HINT_COLOR, 0);
        }
    }

    /* draw edges between parent and child nodes */
    if (node != graph.root){
        SDL_SetRenderDrawColor(app.renderer, UNSELECTED_COLOR[0], UNSELECTED_COLOR[1], UNSELECTED_COLOR[2], UNSELECTED_COLOR[3]);
        SDL_RenderDrawLine(app.renderer, x, y - (height/2), node->p->pos.x, node->p->pos.y + (getHeight(node->p->text.buf, 1) / 2));
    }


    for (int i=0; i<node->children->num; i++){
        debug_print("child %d / %lu: %p\n", i, node->children->num, node->children->array[i]);
        drawNode(node->children->array[i]);
    }
}

int getWidth (char* message, bool wrap){
    int ret;
    if ( wrap )
        ret = min(strlen(message), NUM_CHARS_B4_WRAP) * TEXTBOX_WIDTH_SCALE;
    else
        ret = strlen(message) * TEXTBOX_WIDTH_SCALE;
    debug_print("getWidth %d\n", ret);
    return ret;
}
int getHeight (char* message, bool wrap){
    if ( wrap ){
        int num_lines=1, chars_since_line_start=0, chars_since_last_space=0;
        for (int i = 0; i < strlen(message); ++i) {
            if ( chars_since_line_start == NUM_CHARS_B4_WRAP ) {
                chars_since_line_start = chars_since_last_space - 1;
                num_lines++;
            }
            if ( message[i] == ' ' )
                chars_since_last_space = 0;
            if ( message[i] == '\n' ){
                chars_since_last_space = 0;
                chars_since_line_start = 0;
            }
            chars_since_line_start++;
            chars_since_last_space++;

        }
        return TEXTBOX_HEIGHT * num_lines;
    }
    else
        return TEXTBOX_HEIGHT;
}

void renderMessage(char* message, Point pos, double scale, SDL_Color color, bool wrap){
    if (!message) return;
    // create surface from string
    SDL_Surface* surfaceMessage;
    if ( wrap )
        surfaceMessage = TTF_RenderText_Blended_Wrapped(FONT, message, color, NUM_CHARS_B4_WRAP * TEXTBOX_WIDTH_SCALE * scale);
    else
        surfaceMessage = TTF_RenderText_Solid(FONT, message, color);

    // now you can convert it into a texture
    SDL_Texture* Message = SDL_CreateTextureFromSurface(app.renderer, surfaceMessage);

    SDL_Rect Message_rect;
    Message_rect.x = pos.x;
    Message_rect.y = pos.y;
    Message_rect.w = getWidth(message, wrap) * scale;
    Message_rect.h = getHeight(message, wrap) * scale;

    SDL_SetRenderDrawColor(app.renderer, BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], BACKGROUND_COLOR[3]);
    SDL_RenderFillRect(app.renderer, &Message_rect);
    debug_print("rendering %s %d %d\n", message, Message_rect.w, Message_rect.h);
    SDL_RenderCopy(app.renderer, Message, NULL, &Message_rect);
    SDL_FreeSurface(surfaceMessage);
    SDL_DestroyTexture(Message);
}


// returns the height of the tree with given root node
int get_depth(Node* node) {
    int max_depth = 0;
    for(int i = 0; i < node->children->num; i++) {
        int child_depth = get_depth(node->children->array[i]);
        if (max_depth < child_depth) {
            max_depth = child_depth;
        }
    }
    return 1 + max_depth;
}

// recursive helper function for calculate_positions
// shifts over subtrees so that they do not overlap at any x-coordinate
void calculate_x_offsets(Node* node) {
    debug_print("calculating offsets for %p\n", node);
    int text_pixel_length = getWidth(node->text.buf, true);
    node -> x_offset  = 0;
    node -> rightmost = text_pixel_length/2;
    node -> leftmost  = -text_pixel_length/2;
    int total_offset = 0;
    debug_print("shifting %ld children for node with text %s\n", node->children->num, node->text.buf);
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        calculate_x_offsets(child);
        if (i > 0) {
            // shift the current child (more than) far enough away from the
            // previous to guarantee that they subtrees will not overlap
            int offset = (node->children->array[i-1]->rightmost)-(child->leftmost)+RADIUS;
            total_offset += offset;
            child->x_offset = total_offset;
        }
    }
    debug_print("centering parent\n");
    // center parent over children
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        child->x_offset -= total_offset/2;
    }
    // calculate leftmost and rightmost for current node
    debug_print("calculating left and rightmost children\n");
    if(node->children->num >= 1) {
        Node* leftmost_child = node->children->array[0];
        int child_leftmost = leftmost_child->x_offset + leftmost_child->leftmost;
        if(child_leftmost < node->leftmost) {
            node->leftmost = child_leftmost;
        }

        Node* rightmost_child = node->children->array[node->children->num-1];
        int child_rightmost = rightmost_child->x_offset + rightmost_child->rightmost;
        if(child_rightmost > node->rightmost) {
            node->rightmost = child_rightmost;
        }
    }
}

void calculate_max_heights(Node* node, int level, int* max_heights) {
    int node_height = getHeight(node->text.buf, true)+RADIUS;
    if (node_height > max_heights[level]) {
        max_heights[level] = node_height;
    }
    for(int i = 0; i < node->children->num; i++) {
        calculate_max_heights(node->children->array[i], level+1, max_heights);
    }
}

// recursive helper function for calculate_positions
// accumulates offsets to assign correct [x,y] values to each node
void apply_offsets(Node* node, int x_offset, int level, int* y_levels) {
    node->pos.x = x_offset + app.window_size.x/2;
    if (level > 0) {
        node->pos.y = node->p->pos.y + y_levels[level-1]/2 + y_levels[level]/2;
    }
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        apply_offsets(child, x_offset + child->x_offset, level+1, y_levels);
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

    int* y_levels = calloc(1+get_depth(root), sizeof(int));

    debug_print("calculating offsets\n");
    calculate_x_offsets(root);
    debug_print("calculating y levels\n");
    calculate_max_heights(root, 0, y_levels);
    debug_print("applying offsets\n");
    apply_offsets(root, 0, 0, y_levels);
    debug_print("centering\n");
    center_on_selected(root, graph.selected->pos.x, graph.selected->pos.y);
    debug_print("positions calculated\n");

    free(y_levels);
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
    SDL_SetRenderDrawColor(app.renderer, BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], BACKGROUND_COLOR[3]); /* Background color */
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
    filename_pos.y = (int) ((1.0) * app.window_size.y - TEXTBOX_HEIGHT);
    renderMessage(FILENAME_BUFFER.buf, filename_pos, 1.0, EDIT_COLOR, 0);

    // Draw mode
    Point mode_text_pos;
    mode_text_pos.x = (int) ((0.0) * app.window_size.x);
    mode_text_pos.y = (int) ((0.0) * app.window_size.y);
    debug_print("rendering %s\n", getModeName());
    renderMessage(getModeName(), mode_text_pos, 1.0, EDIT_COLOR, 0);

    //Draw hint buffer
    Point hint_buf_pos;
    hint_buf_pos.x = (int) ((1.0 * app.window_size.x) - (HINT_BUFFER.len * TEXTBOX_WIDTH_SCALE));
    hint_buf_pos.y = (int) ((1.0) * app.window_size.y - TEXTBOX_HEIGHT);
    renderMessage(HINT_BUFFER.buf, hint_buf_pos, 1.0, HINT_COLOR, 0);

    if ( TOGGLE_MODE ){
        Point toggle_indicator_pos;
        toggle_indicator_pos.x = (int) ((1.0 * app.window_size.x) - (strlen(TOGGLE_INDICATOR) * TEXTBOX_WIDTH_SCALE));
        toggle_indicator_pos.y = (int) ((0.0) * app.window_size.y);
        renderMessage(TOGGLE_INDICATOR, toggle_indicator_pos, 1.0, EDIT_COLOR, 0);
    }
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
void removeFromArray(Array *a, Node* node){
    bool start_shifting = false;
    Node** nodes = a->array;
    for (int i = 0; i < a->num; ++i) {
        if ( nodes[i] == node )
            start_shifting = true;
        if ( start_shifting )
            nodes[i] = nodes[i+1];
    }
    if (a->num > 0 && start_shifting){
        a->array[a->num] = NULL;
        a->num -= 1;
    }
}

void* freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->num = a->size = 0;
    free(a);
    return NULL;
}

void clearBuffer(Buffer* buffer){
    for (int i = 0; i < buffer->len; ++i)
        buffer->buf[i] = '\0';
    buffer->len = 0;
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
    calculate_positions(graph.root, graph.selected);
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
    debug_print("freeing children\n");
    freeArray(node->children);
    debug_print("freeing buffer\n");
    free(node->text.buf);
    debug_print("freeing hint_text\n");
    free(node->hint_text);
    free(node);
    debug_print("deleted node %p\n", node);
}

void removeNodeFromGraph(Node* node){

    if ( node == graph.root ) return;
    debug_print("removing node from graph\n");
    // remove node from array
    removeFromArray(node->p->children, node);
    removeFromArray(HINT_NODES, node);
    if ( node == graph.selected )
        graph.selected = node->p;
    deleteNode(node);
    debug_print("removed node from graph\n");
}

void makeGraph(){
    graph.root = makeNode();
    graph.num_nodes = 0;
    graph.selected = graph.root;
    graph.root->p = graph.root;
    switch2(Travel);
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
    for (int i = 0; i < front; ++i) {
        free(queue[i]);
    }
    for (int i = 0; i < HINT_NODES->num; i++) {
        debug_print("%p: %s\n", HINT_NODES->array[i]->hint_text, queue[front + i]);
        strcpy(HINT_NODES->array[i]->hint_text, queue[front + i]);
        free(queue[front + i]);
        debug_print("freed\n");
    }
    for (int i = front + HINT_NODES->num; i < back; ++i) {
        free(queue[i]);
    }
    free(queue);

    // parent is always k
    strcpy(node->p->hint_text,"k");
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
    if ( argc > 1 )
        strcpy(FILENAME_BUFFER.buf, argv[1]);
    else
        strcpy(FILENAME_BUFFER.buf, "unnamed.txt");

    readfile();
    FILENAME_BUFFER.len = strlen(FILENAME_BUFFER.buf);

    HINT_BUFFER.buf = calloc(HINT_BUFFER.size + 1, sizeof(char));
    /* gracefully close windows on exit of program */
    atexit(SDL_Quit);
    app.quit = 0;

    SDL_Event e;
    /* Only updates display and processes inputs on new events */
    while ( !app.quit && SDL_WaitEvent(&e) ) {
        if ( e.type == SDL_MOUSEMOTION) continue;

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

    if (HINT_BUFFER.buf) free(HINT_BUFFER.buf);
    free(FILENAME_BUFFER.buf);
    /* delete nodes recursively, starting from root */
    deleteNode(graph.root);
    debug_print("deleted nodes\n");
    SDL_DestroyRenderer( app.renderer );
    SDL_DestroyWindow( app.window );
    SDL_Quit();
    return 0;
}
