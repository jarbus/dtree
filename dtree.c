#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_ttf.h>
#include "SDL_events.h"
#include "SDL_scancode.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
// https://stackoverflow.com/questions/1644868/define-macro-for-log-printing-in-c
// EDIT: https://stackoverflow.com/questions/1941307/log-print-macro-in-c
#ifdef DEBUG
#define logPrint(...) fprintf(stderr, __VA_ARGS__);
#else
#define logPrint(...) /* no instruction */
#endif

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

/* User Customizable Variables*/
static const SDL_Color EDIT_COLOR =    {220, 220, 220};           // RGB
static const SDL_Color HINT_COLOR =    {220, 0, 0};               // RGB
static const int SELECTED_COLOR[4] =   {0, 220, 0, 0};
static const int UNSELECTED_COLOR[4] = {0, 55, 0, 0};
static const int CUT_COLOR[4] =        {0, 0, 220, 0};
static const int EDGE_COLOR[4] =       {220, 220, 220, 0};
static const int BACKGROUND_COLOR[4] = {15, 15, 15, 255};
static const int TEXTBOX_WIDTH_SCALE = 25;                        // width of char
static const int TEXTBOX_HEIGHT = 50;                             // height of char
static const int FILENAME_BUFFER_MAX_SIZE = 64;
static const int HINT_BUFFER_MAX_SIZE = 3;
static const int MAX_TEXT_LEN = 128;                              // Max Num of chars in a node
static const int NUM_CHARS_B4_WRAP = 20;
// radius and thickness of node box
static const int RADIUS = 50;
static const int THICKNESS = 5;
static const int FONT_SIZE = 40;
static const char* FONT_NAME = "./assets/SourceCodePro-Regular.otf";   // Default Font name
static const char* HINT_CHARS = "adfghjkl;\0";              // characters to use for hints


// ENUMS AND DATA STRUCTURES
typedef struct Point Point;
typedef struct App App;
typedef struct Buffer Buffer;
typedef struct Array Array;
typedef struct Node Node;
typedef struct Graph Graph;

enum Mode{Travel, Edit, FilenameEdit, Delete, Cut, Paste, MakeChild};
char* getModeName(enum Mode mode_param){
    switch(mode_param) {
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
        case Travel: case Delete: case Cut: case Paste: case MakeChild: return true;
        default: return false;
    }
}
bool isEditMode(enum Mode mode_param){
    switch(mode_param){
        case FilenameEdit: case Edit: return true;
        default: return false;
    }
}

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

struct Buffer {
    char* buf;
    int len;
    int size;
};

void clearBuffer(Buffer* buffer){
    for (int i = 0; i < buffer->len; ++i)
        buffer->buf[i] = '\0';
    buffer->len = 0;
}

/* dynamic array to save me some headache, code is stolen from stack overflow */
struct Array {
    Node **array;
    size_t num;  /* number of children in array */
    size_t size; /* max size of array */
};
Array* initArray(size_t initial_size) {
    Array *a;
    a = calloc(1, sizeof(Array));
    a->array = calloc(initial_size, sizeof(void*));
    a->num = 0;
    a->size = initial_size;
    return a;
}
void insertArray(Array *a, void* element) {
    // a->num is the number of used entries, because a->array[a->num++] updates a->num only *after* the array has been accessed.
    // Therefore a->num can go up to a->size
    if (a->num == a->size) {
        a->size *= 2;
        logPrint("Reallocing, num %ld size %ld...\n", a->num, a->size);
        a->array = realloc(a->array, a->size * sizeof(void*));
        logPrint("Realloced.\n");
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

struct Node {
    Node* p;
    Array* children;
    Point pos;
    int x_offset; /* offset wrt parent; "mod" in tree drawing algos */
    int rightmost; /* greatest descendant accumulated x_off wrt node*/
    int leftmost; /* smallest (negative) acc. x_off wrt node */
    Buffer text;
    char* hint_text;
};
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
    node->hint_text = calloc(HINT_BUFFER_MAX_SIZE, sizeof(char));
    return node;
}
Node* makeChild(Node* parent){
    Node* child = makeNode();
    child->p = parent;
    insertArray(parent->children, child);
    return child;
}
// frees all memory for the given node, as well as all its descendants
void deleteNode(Node* node){
    logPrint("DELETEING %p\n", node);
    /* Handle nodes that have already been deleted */
    if ( node == NULL )
        return;
    /* delete each child */
    for (int i=0; i<node->children->num; i++)
        deleteNode( node->children->array[i] );

    /* Then delete node */
    logPrint("Freeing children\n");
    freeArray(node->children);
    logPrint("Freeing buffer\n");
    free(node->text.buf);
    logPrint("Freeing hint_text\n");
    free(node->hint_text);
    free(node);
    logPrint("Deleted node %p\n", node);
}
// removes each node in a subtree from a given Array
void removeSubtreeFromArray(Array* array, Node* node) {
    if(node == NULL)
        return;
    removeFromArray(array, node);
    for(int i = 0; i < node->children->num; i++)
        removeSubtreeFromArray(array, node->children->array[i]);
}
bool isInSubtree(Node* node, Node* root) {
    if (node==NULL || root==NULL)
        return false;
    if (node == root)
        return true;
    for (int i = 0; i < root->children->num; i++)
        if (isInSubtree(node, root->children->array[i]))
            return true;
    return false;
}

struct Graph {
    struct Node* root;
    Node* selected;
};
void makeGraph(Graph* graph){
    graph->root = makeNode();
    graph->selected = graph->root;
    graph->root->p = graph->root;
}


/* Global variables */
static App APP;                 // App object that contains rendering info
static Graph GRAPH;             // Graph object that contains nodes
static enum Mode MODE;          // Global mode
// {ptr, cur_len, max_size} for hints
static Buffer HINT_BUFFER = {NULL, 0, HINT_BUFFER_MAX_SIZE};
// {ptr, cur_len, max_size} for fname
static Buffer FILENAME_BUFFER = {NULL, 0, FILENAME_BUFFER_MAX_SIZE};
static TTF_Font* FONT;          // Global Font object
static Array* HINT_NODES;       // array of all nodes to be hinted to
static Buffer* CURRENT_BUFFER;  // buffer to store current hint progress
static Node* CUT = NULL;
static bool TOGGLE_MODE = false;
static char* TOGGLE_INDICATOR = "MODE PERSIST\0";
static Node* LEFT_NEIGHBOR = NULL; // left and right neighbors of the selected node
static Node* RIGHT_NEIGHBOR = NULL;

// GENERAL UTIL FUNCTIONS
void removeNodeFromGraph(Node* node);
int min(int a, int b);
int max(int a, int b);
int getWidth (char* message, bool wrap);
int getHeight (char* message, bool wrap);
// READ/WRITE
void replaceChar(char* arr, char find, char replace);
unsigned int countTabs(char* string);
void endAtNewline(char* string, int text_len);
void readFile();
void writeChildrenStrings(FILE* file, Node* node, int level);
void writeFile();
// HINT MANAGEMENT
Node* calculateLeftNeighbor(Node* cur);
Node* calculateRightNeighbor(Node* cur);
void calculateNeighbors();
void clearHintText();
void populateHintNodes(Node* node);
void populateHintText(Node* node);
// EVENT HANDLING
void activateHints();
void switchMode(enum Mode to);
void hintFunction(Node* node);
void handleTextInput(SDL_Event *event);
void doKeyDown(SDL_KeyboardEvent *event);
void doKeyUp(SDL_KeyboardEvent *event);
void eventHandler(SDL_Event *event);
// POSITION CALCULATION ALGORITHM
int getDepth(Node* node);
void calculateOffsets(Node* node);
void calculateMaxHeights(Node* node, int level, int* max_heights);
void applyOffsets(Node* node, int x_offset, int level, int* y_levels);
void centerOnSelected(Node* node, int selected_x, int selected_y);
void calculatePositions(Node* root, Node* selected);
void recursivelyPrintPositions(Node* node, int level);
// RENDERING
void renderMessage(char* message, Point pos, double scale, SDL_Color color, bool wrap);
void drawBox(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int offset, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawBorder(SDL_Renderer *surface, int n_cx, int n_cy, int len, int height, int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void drawNode(Node* node);
void prepareScene();
void presentScene();
// INIT
void initSDL();

// GENERAL UTIL FUNCTIONS

// Utility function for removing a node & subtree from graph
void removeNodeFromGraph(Node* node){
    if ( node == GRAPH.root ) return;
    logPrint("Removing node from graph...\n");
    // remove node from parent's children
    removeFromArray(node->p->children, node);
    // remove hint memory for this node & subtree
    removeSubtreeFromArray(HINT_NODES, node);
    // if the currently selected node would be deleted, bubble up to the parent
    if ( isInSubtree(GRAPH.selected, node) )
        GRAPH.selected = node->p;
    // free memory for this node and its subtree
    deleteNode(node);
    logPrint("Removed node from graph.\n");
}

int min(int a, int b){ if (a < b ) return a; else return b; }
int max(int a, int b){ if (a > b ) return a; else return b; }

// return graphical width of text (with or without text wrapping)
int getWidth (char* message, bool wrap){
    int ret;
    if ( wrap ){
        for (int i = 0; i < strlen(message); ++i) {
            if ( i > 0 && message[i-1] == '\n' ){
                return NUM_CHARS_B4_WRAP * TEXTBOX_WIDTH_SCALE;
            }
        }
        ret = min( strlen(message), NUM_CHARS_B4_WRAP) * TEXTBOX_WIDTH_SCALE;
    }
    else
        ret = strlen(message) * TEXTBOX_WIDTH_SCALE;
    return ret;
}
// return graphical height of text (with or without text wrapping)
int getHeight (char* message, bool wrap){
    if ( wrap ){
        int num_lines=1, chars_since_line_start=0, chars_since_last_space=0;
        for (int i = 0; i < strlen(message); ++i) {
            if ( chars_since_line_start >= NUM_CHARS_B4_WRAP && message[i] != ' ' && message[i] != '\n' ) {
                chars_since_line_start = chars_since_last_space -1;
                num_lines++;
            }
            if ( message[i] == ' ' )
                chars_since_last_space = 0;
            if ( message[i] == '\n' && i < strlen(message) - 1){
                chars_since_last_space = 0;
                chars_since_line_start = -1;
                num_lines++;
            }
            chars_since_line_start++;
            chars_since_last_space++;
        }
        return TEXTBOX_HEIGHT * num_lines;
    }
    else
        return TEXTBOX_HEIGHT;
}


// READ/WRITE

// We replace user-entered \n with | for the file format
void replaceChar(char* arr, char find, char replace){
    for (int i = 0; i < strlen(arr); ++i)
        if ( arr[i] == find )
            arr[i] = replace;
    return;
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

void endAtNewline(char* string, int text_len){
    for (int i = 0; i < text_len; ++i) {
        if (string[i] == '\n') {
            string[i] = '\0';
            break;
        }
    }
}

void readFile(){
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
    hierarchy[0] = GRAPH.root;
    endAtNewline(ret, strlen(ret));
    replaceChar(ret, '|', '\n');
    strcpy(GRAPH.root->text.buf, ret);
    GRAPH.root->text.len = strlen(ret);
    while ( true ){
        /* loads next line */
        ret = fgets(buf, MAX_TEXT_LEN, fp);
        /* reached EOF */
        if ( ret != buf )
            break;
        /* remove any trailing newlines */
        endAtNewline(ret, strlen(ret));
        replaceChar(ret, '|', '\n');
        /* determine level in tree by number of tabs */
        level = countTabs(ret);
        /* create new child node */
        hierarchy[level] = makeChild(hierarchy[level-1]);
        /* copy current line to child node, offset by number of tabs */
        strcpy(hierarchy[level]->text.buf, ret + level);
        hierarchy[level]->text.len = strlen(ret + level); // for some reason I need to have a -1 here
    }
    fclose(fp);
    free(buf);
    free(hierarchy);
}

 /* Recursively print children of nodes, with each child indented once from the parent */
void writeChildrenStrings(FILE* file, Node* node, int level){
    for(int i=0; i<level;i++)
        fprintf(file, "\t");
    replaceChar(node->text.buf, '\n', '|');
    fprintf(file, "%s\n", node->text.buf);
    replaceChar(node->text.buf, '|', '\n');
    for (int i=0; i<node->children->num; i++)
        writeChildrenStrings(file, node->children->array[i], level + 1);
}

void writeFile(){
    if ( FILENAME_BUFFER.buf == NULL ) return;
    FILE* output = fopen(FILENAME_BUFFER.buf, "w");
    writeChildrenStrings(output, GRAPH.root, 0);
    fclose(output);
}


// HINT MANAGEMENT

// helper function for calculateNeighbors
// the right neighbor of a node is the immediate node to the right of the current node
// this is either a right sibling, or the leftmost child of the parent node's right sibling
Node* calculateRightNeighbor(Node* cur) {
    if(cur == GRAPH.root || cur == NULL)
        return NULL;
    Node* parent = cur->p; 
    // check for right sibling
    for(int i = 0; i < parent->children->num; i++)
        if (parent->children->array[i] == cur && i < parent->children->num-1)
            return parent->children->array[i+1];
    // take leftmost child of parent's right neighbor
    Node* rightParent = calculateRightNeighbor(parent);
    if (rightParent == NULL || rightParent->children->num == 0)
        return NULL;
    return rightParent->children->array[0];
}
// symmetric algorithm for left neighbor
Node* calculateLeftNeighbor(Node* cur) {
    if(cur == GRAPH.root || cur == NULL)
        return NULL;
    Node* parent = cur->p; 
    // check for left sibling
    for(int i = 0; i < parent->children->num; i++)
        if (parent->children->array[i] == cur && i != 0)
            return parent->children->array[i-1];
    // take rightmost child of parent's left neighbor
    Node* leftParent = calculateLeftNeighbor(parent);
    if (leftParent == NULL || leftParent->children->num == 0)
        return NULL;
    return leftParent->children->array[leftParent->children->num-1];
}

void calculateNeighbors() {
    LEFT_NEIGHBOR = calculateLeftNeighbor(GRAPH.selected);
    RIGHT_NEIGHBOR = calculateRightNeighbor(GRAPH.selected);
}

void clearHintText() {
    logPrint("clearHintText()\n");
    // return if already freed
    if (HINT_NODES == NULL || HINT_NODES->array == NULL){
        return;
    }
    logPrint("Hint nodes %p, num %ld\n", HINT_NODES->array, HINT_NODES->num);
    // Clear all hint text in each hint node
    for (int i = 0; i < HINT_NODES->num; ++i) {
        for (int j = 0; j < HINT_BUFFER.size; j++) {
            if (HINT_NODES->array[i]->hint_text){
                HINT_NODES->array[i]->hint_text[j] = '\0';
            }
        }
    }
    // Clear hint node array
    logPrint("Freeing HINT_NODES\n");
    HINT_NODES = freeArray ( HINT_NODES );
    logPrint("End clearHintText()\n");
}

// Add all visible nodes to HINT_NODES
void populateHintNodes(Node* node){
    if ( !node )
        return;
    logPrint("Adding hint node: %dx%d\n", node->pos.x, node->pos.y);
    if ( -(2*getWidth(node->text.buf, 1)) <= node->pos.x && node->pos.x < APP.window_size.x+(2*getWidth(node->text.buf, 1)) &&\
            RADIUS < node->pos.y && node->pos.y < APP.window_size.y+(2*getHeight(node->text.buf, 1))) {
        insertArray(HINT_NODES, node);
    }
    for (int i = 0; i < node->children->num; ++i) {
        logPrint("Going to next child\n");
        populateHintNodes(node->children->array[i]);
    }
}

void populateHintText(Node* node){

    logPrint("Populate start\n");
    calculateNeighbors(GRAPH.root, GRAPH.selected);
    clearHintText();
    HINT_NODES = initArray(10);
    populateHintNodes(GRAPH.root);
    logPrint("Cleared\n");
    logPrint("%p: %s\n",node->p->hint_text, node->p->hint_text);
    char** queue = calloc(8192, sizeof(char*));
    char* prefix = calloc(HINT_BUFFER.size+1, sizeof(char));
    int front = 0, back=0, done=0;
    while ( !done ){
        logPrint("Iterating while loop, back: %d, front: %d\n", back, front);
        for (int i = 0; i < strlen(HINT_CHARS); ++i) {
            // blacklist hard-coded hints
            if (HINT_CHARS[i] == 'k' || HINT_CHARS[i] == 'h' || HINT_CHARS[i] == 'l')
                continue;

            queue[back] = calloc(HINT_BUFFER.size+1, sizeof(char));
            strcpy(queue[back], prefix);
            queue[back][strlen(prefix)] = HINT_CHARS[i];
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
        strcpy(HINT_NODES->array[i]->hint_text, queue[front + i]);
        free(queue[front + i]);
    }
    for (int i = front + HINT_NODES->num; i < back; ++i) {
        free(queue[i]);
    }
    free(queue);

    // parent is 'k', left neighbor 'h', right neighbor 'l'
    strcpy(node->p->hint_text,"k");
    if (LEFT_NEIGHBOR)
        strcpy(LEFT_NEIGHBOR->hint_text, "h");
    if (RIGHT_NEIGHBOR)
        strcpy(RIGHT_NEIGHBOR->hint_text, "l");
    logPrint("Populate end.\n");
}


// EVENT HANDLING

void activateHints(){
    populateHintText(GRAPH.selected);
    CURRENT_BUFFER = &HINT_BUFFER;
}

void switchMode(enum Mode to){
    if ( isHintMode(MODE) ){
        HINT_NODES = freeArray (HINT_NODES);
        clearBuffer(&HINT_BUFFER);
    }
    switch ( to ){
        case Edit: CURRENT_BUFFER = &GRAPH.selected->text; break;
        case FilenameEdit: CURRENT_BUFFER = &FILENAME_BUFFER; to = Edit; break;
        case Travel: TOGGLE_MODE = false; break;
        default: break;
    }
    MODE = to;
    if ( isHintMode(to) )
        activateHints();
}

// When a hint node is selected, this function is run
void hintFunction(Node* node){
    switch(MODE){
        case Travel: GRAPH.selected = node; break;
        case Delete: removeNodeFromGraph(node); break;
        case Cut: CUT = node; switchMode(Paste); break;
        case MakeChild: makeChild(node); activateHints(); break;
        case Paste:
            if ( !CUT ) break;
            removeFromArray(CUT->p->children, CUT);
            insertArray(node->children, CUT);
            CUT->p = node;
            CUT = NULL;
            switchMode( Cut );
            break;
        default:
            break;
    }
    if ( TOGGLE_MODE == false && MODE != Paste )
        switchMode( Travel );
}

void handleTextInput(SDL_Event *event){
    if ( CURRENT_BUFFER && CURRENT_BUFFER->len < CURRENT_BUFFER->size ){
        logPrint("Adding text to current buffer...\n");
        int add_text = 1;
        // skip adding to buffer for hint modes if not a valid hint char
        if ( isHintMode(MODE) ){
            add_text = 0;
            for (int i = 0; i < strlen(HINT_CHARS); ++i){
                if ( HINT_CHARS[i] == event->edit.text[0] ){
                    add_text = 1;
                    break;
                }
            }
        }

        if ( add_text && CURRENT_BUFFER->len < CURRENT_BUFFER->size)
            CURRENT_BUFFER->buf[(CURRENT_BUFFER->len)++] = event->edit.text[0];
        logPrint("Detected character: %c\n", event->edit.text[0]);
        logPrint("New CURRENT_BUFFER: len %d: %s\n", CURRENT_BUFFER->len, CURRENT_BUFFER->buf);
    }
    if ( isHintMode(MODE) ){
        // go to node specified by travel chars
        logPrint("Handling travel input: %d/%d chars\n", HINT_BUFFER.len, HINT_BUFFER.size);
        // check if any nodes hint text matches current input
        for (int i = 0; i < HINT_NODES->num; ++i) {
            // continue if hint buffer != any hint text
            if ( strcmp(HINT_BUFFER.buf, HINT_NODES->array[i]->hint_text) != 0 )
                continue;
            hintFunction(HINT_NODES->array[i]);
            // reset hint text

            logPrint("Freeing hint buffer\n");
            clearBuffer(&HINT_BUFFER);
            logPrint("Freed hint buffer\n");

            logPrint("Re-calculating positions of nodes...\n");
            calculatePositions(GRAPH.root, GRAPH.selected);
            logPrint("Populating hint text...\n");
            populateHintText(GRAPH.selected);
            logPrint("Populated.\n");
        }
        logPrint("Handled hint input.\n");
    }
}

void doKeyDown(SDL_KeyboardEvent *event) {
    if ( isEditMode(MODE) ){
        switch(event->keysym.sym) {
            case SDLK_BACKSPACE:
                if ( CURRENT_BUFFER && CURRENT_BUFFER->len > 0)
                    CURRENT_BUFFER->buf[--CURRENT_BUFFER->len] = '\0';
        }
    }
    return;
}

void doKeyUp(SDL_KeyboardEvent *event) {
    if (event->repeat != 0) {
        return;
    }
    // up-front key-checks that apply to any mode
    switch(event->keysym.sym) {
        case SDLK_ESCAPE:
            if (MODE == Travel) CUT = NULL; // clear cut node on a double escape
            switchMode(Travel);
            return;
    }

    // mode-specific key-bindings
    switch(MODE) {
        case Travel:
            switch(event->keysym.sym) {
                case SDLK_o: switchMode(MakeChild); return;
                case SDLK_e: switchMode(Edit); return;
                case SDLK_r: switchMode(FilenameEdit); return;
                case SDLK_x: switchMode(Delete); return;
                case SDLK_m: switchMode(Cut); return;
                case SDLK_p: switchMode(Paste); return;
                case SDLK_s: clearBuffer(&GRAPH.selected->text); switchMode(Edit); return;
                case SDLK_c: TOGGLE_MODE = true; return;
                case SDLK_w: writeFile(); return;
                case SDLK_q: APP.quit = true; return;
            }
            break; // end of Travel bindings
        case Edit:
            switch(event->keysym.sym) {
                case SDLK_RETURN:
                    if ( CURRENT_BUFFER && CURRENT_BUFFER->len >= 0)
                        CURRENT_BUFFER->buf[CURRENT_BUFFER->len++] = '\n';
                    return;
            }
            break; // end of Edit bindings
        case Delete:
            switch(event->keysym.sym) {
                case SDLK_x: { switchMode(Travel); return; }
            }
            break; // end of Delete bindings
        default:
            break;
    }
}

void eventHandler(SDL_Event *event) {
    switch (event->type){
        case SDL_TEXTINPUT: handleTextInput(event); break;
        case SDL_KEYDOWN: doKeyDown(&event->key); break;
        case SDL_KEYUP: doKeyUp(&event->key); break;
        case SDL_QUIT: exit(0); break;
        case SDL_WINDOWEVENT:
            if(event->window.event == SDL_WINDOWEVENT_RESIZED)
                SDL_GetWindowSize(APP.window, &APP.window_size.x, &APP.window_size.y);
            break;
        default:
            break;
    }
}


// POSITION CALCULATION ALGORITHM

// returns the height of the tree with given root node
int getDepth(Node* node) {
    int max_depth = 0;
    for(int i = 0; i < node->children->num; i++) {
        int child_depth = getDepth(node->children->array[i]);
        if (max_depth < child_depth) {
            max_depth = child_depth;
        }
    }
    return 1 + max_depth;
}

// recursive helper function for calculatePositions
// shifts over subtrees so that they do not overlap at any x-coordinate
void calculateOffsets(Node* node) {

    logPrint("Calculating offsets for %p...\n", node);
    int text_pixel_length = getWidth(node->text.buf, true);
    node -> x_offset  = 0;
    node -> rightmost = text_pixel_length/2;
    node -> leftmost  = -text_pixel_length/2;
    int total_offset = 0;
    logPrint("Shifting %ld children for node with text %s\n", node->children->num, node->text.buf);
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        calculateOffsets(child);
        if (i > 0) {
            // shift the current child (more than) far enough away from the
            // previous to guarantee that they subtrees will not overlap
            int offset = (node->children->array[i-1]->rightmost)-(child->leftmost)+RADIUS;
            total_offset += offset;
            child->x_offset = total_offset;
        }
    }
    logPrint("Centering parent\n");
    // center parent over children
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        child->x_offset -= total_offset/2;
    }
    // calculate leftmost and rightmost for current node
    logPrint("Calculating left and rightmost children\n");
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

void calculateMaxHeights(Node* node, int level, int* max_heights) {
    int node_height = getHeight(node->text.buf, true)+RADIUS;
    if (node_height > max_heights[level]) {
        max_heights[level] = node_height;
    }
    for(int i = 0; i < node->children->num; i++) {
        calculateMaxHeights(node->children->array[i], level+1, max_heights);
    }
}

// recursive helper function for calculatePositions
// accumulates offsets to assign correct [x,y] values to each node
void applyOffsets(Node* node, int x_offset, int level, int* y_levels) {
    node->pos.x = x_offset + APP.window_size.x/2;
    if (level > 0) {
        node->pos.y = node->p->pos.y + y_levels[level-1]/2 + y_levels[level]/2;
    }
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        applyOffsets(child, x_offset + child->x_offset, level+1, y_levels);
    }
}

// shifts all node positions so that the selected node is at the center of the screen
void centerOnSelected(Node* node, int selected_x, int selected_y) {
    node->pos.x += APP.window_size.x/2 - selected_x;
    node->pos.y += APP.window_size.y/2 - selected_y;
    for (int i = 0; i < node->children->num; i++) {
        Node* child = node->children->array[i];
        centerOnSelected(child, selected_x, selected_y);
    }
}

// recomputes the coordinates of the nodes (i.e. populates pos field)
void calculatePositions(Node* root, Node* selected){
    int* y_levels = calloc(1+getDepth(root), sizeof(int));

    logPrint("Calculating offsets...\n");
    calculateOffsets(root);
    logPrint("Calculating y levels...\n");
    calculateMaxHeights(root, 0, y_levels);
    logPrint("Applying offsets...\n");
    applyOffsets(root, 0, 0, y_levels);
    logPrint("Centering...\n");
    centerOnSelected(root, GRAPH.selected->pos.x, GRAPH.selected->pos.y);
    logPrint("Positions calculated.\n");

    free(y_levels);
}

/* Debug function, used to print locations of all nodes in indented hierarchy */
// dead code
void recursivelyPrintPositions(Node* node, int level){
    for (int i=0; i<level; i++){
        logPrint("\t");
    }
    logPrint("%d %d\n",node->pos.x, node->pos.y);

    for (int i = 0; i < node->children->num; i++) {
        recursivelyPrintPositions(node->children->array[i], level + 1);
    }
}


// RENDERING

void renderMessage(char* message, Point pos, double scale, SDL_Color color, bool wrap){
    if (!message) return;
    // create surface from string
    SDL_Surface* surface_message;
    if ( wrap )
        surface_message = TTF_RenderText_Blended_Wrapped(FONT, message, color, NUM_CHARS_B4_WRAP * TEXTBOX_WIDTH_SCALE * scale);
    else
        surface_message = TTF_RenderText_Solid(FONT, message, color);

    // now you can convert it into a texture
    SDL_Texture* texture_message = SDL_CreateTextureFromSurface(APP.renderer, surface_message);

    SDL_Rect message_rect;
    message_rect.x = pos.x;
    message_rect.y = pos.y;
    message_rect.w = getWidth(message, wrap) * scale;
    message_rect.h = getHeight(message, wrap) * scale;

    SDL_SetRenderDrawColor(APP.renderer, BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], BACKGROUND_COLOR[3]);
    SDL_RenderFillRect(APP.renderer, &message_rect);
    logPrint("Rendering %s %d %d\n", message, message_rect.w, message_rect.h);
    SDL_RenderCopy(APP.renderer, texture_message, NULL, &message_rect);
    SDL_FreeSurface(surface_message);
    SDL_DestroyTexture(texture_message);
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

    logPrint("drawNode(%p)\n", node);
    int x = node->pos.x;
    int y = node->pos.y;
    logPrint("Children pointer: %p, num: %lu\n", node->children, node->children->num);
    /* draw red ring for unselected nodes, green for selected */
    int width  = getWidth(node->text.buf, 1);
    int height = getHeight(node->text.buf, 1);
    if (node == CUT)
        drawBorder(APP.renderer, x, y, width, height, THICKNESS, CUT_COLOR[0], CUT_COLOR[1], CUT_COLOR[2], CUT_COLOR[3]);
    else if (node == GRAPH.selected)
        drawBorder(APP.renderer, x, y, width, height, THICKNESS, SELECTED_COLOR[0], SELECTED_COLOR[1], SELECTED_COLOR[2], SELECTED_COLOR[3]);
    else
        drawBorder(APP.renderer, x, y, width, height, THICKNESS, UNSELECTED_COLOR[0], UNSELECTED_COLOR[1], UNSELECTED_COLOR[2], UNSELECTED_COLOR[3]);

    /* draw edges between parent and child nodes */
    if (node != GRAPH.root){
        SDL_SetRenderDrawColor(APP.renderer, EDGE_COLOR[0], EDGE_COLOR[1], EDGE_COLOR[2], EDGE_COLOR[3]);
        SDL_RenderDrawLine(APP.renderer, x, y - (height/2), node->p->pos.x, node->p->pos.y + (getHeight(node->p->text.buf, 1) / 2));
    }

    Point message_pos;
    message_pos.x = x - (width / 2);
    message_pos.y = y - (height / 2);

    /* render node text */
    renderMessage(node->text.buf, message_pos, 1.0, EDIT_COLOR, 1);
    /* render hint text */
    if ( isHintMode(MODE) && strlen(node->hint_text) > 0 ){
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

    for (int i=0; i<node->children->num; i++){
        logPrint("Drawing child %d / %lu: %p...\n", i, node->children->num, node->children->array[i]);
        drawNode(node->children->array[i]);
    }
    logPrint("Finished drawing %p\n", node);
}

/* re-computes graph and draws everything onto renderer */
void prepareScene() {
    SDL_SetRenderDrawColor(APP.renderer, BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], BACKGROUND_COLOR[3]); /* Background color */
    SDL_RenderClear(APP.renderer);

    // recompute the coordinates of each node in the tree
    calculatePositions(GRAPH.root, GRAPH.selected);

    // Draw Graph
    drawNode(GRAPH.root);

    // Draw filename
    Point filename_pos;
    filename_pos.x = (int) ((0.0) * APP.window_size.x);
    filename_pos.y = (int) ((1.0) * APP.window_size.y - TEXTBOX_HEIGHT);
    renderMessage(FILENAME_BUFFER.buf, filename_pos, 1.0, EDIT_COLOR, 0);

    // Draw mode
    Point mode_text_pos;
    mode_text_pos.x = (int) ((0.0) * APP.window_size.x);
    mode_text_pos.y = (int) ((0.0) * APP.window_size.y);
    renderMessage(getModeName(MODE), mode_text_pos, 1.0, EDIT_COLOR, 0);

    //Draw hint buffer
    Point hint_buf_pos;
    hint_buf_pos.x = (int) ((1.0 * APP.window_size.x) - (HINT_BUFFER.len * TEXTBOX_WIDTH_SCALE));
    hint_buf_pos.y = (int) ((1.0) * APP.window_size.y - TEXTBOX_HEIGHT);
    renderMessage(HINT_BUFFER.buf, hint_buf_pos, 1.0, HINT_COLOR, 0);

    if ( TOGGLE_MODE ){
        Point toggle_indicator_pos;
        toggle_indicator_pos.x = (int) ((1.0 * APP.window_size.x) - (strlen(TOGGLE_INDICATOR) * TEXTBOX_WIDTH_SCALE));
        toggle_indicator_pos.y = (int) ((0.0) * APP.window_size.y);
        renderMessage(TOGGLE_INDICATOR, toggle_indicator_pos, 1.0, EDIT_COLOR, 0);
    }
}

/* actually renders the screen */
void presentScene() {
    SDL_RenderPresent(APP.renderer);
}


// INITIALIZATION AND MAIN

void initSDL() {
    HINT_NODES = initArray(10);
    int renderer_flags, window_flags;
    renderer_flags = SDL_RENDERER_ACCELERATED;
    window_flags = SDL_WINDOW_RESIZABLE;

    logPrint("Initializing video...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    logPrint("Initialized video.\n");

    APP.window = SDL_CreateWindow("dtree", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, window_flags);

    if (!APP.window) {
        printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
        exit(1);
    }
    logPrint("Creating renderer...\n");
    APP.renderer = SDL_CreateRenderer(APP.window, -1, renderer_flags);
    logPrint("Render created.\n");

    if (!APP.renderer) {
        logPrint("Failed to create renderer: %s\n", SDL_GetError());
        exit(1);
    }
    APP.quit = false;
    SDL_GetWindowSize(APP.window, &APP.window_size.x, &APP.window_size.y);

    /* start SDL_ttf */
    if(TTF_Init()==-1){
        logPrint("TTF_Init: %s\n", TTF_GetError());
        return;
    }
    atexit(TTF_Quit); /* remember to quit SDL_ttf */
    FONT = TTF_OpenFont(FONT_NAME, FONT_SIZE);
}

int main(int argc, char *argv[]) {
    /* set all bytes of App memory to zero */
    memset(&APP, 0, sizeof(App));
    /* set up window, screen, and renderer */
    initSDL();

    makeGraph(&GRAPH);

    FILENAME_BUFFER.buf = calloc(FILENAME_BUFFER.size, sizeof(char));
    if ( argc > 1 )
        strcpy(FILENAME_BUFFER.buf, argv[1]);
    else
        strcpy(FILENAME_BUFFER.buf, "unnamed.txt");


    FILENAME_BUFFER.len = strlen(FILENAME_BUFFER.buf);
    HINT_BUFFER.buf = calloc(HINT_BUFFER.size + 1, sizeof(char));

    readFile();
    calculatePositions(GRAPH.root,GRAPH.selected);
    switchMode(Travel);
    /* gracefully close windows on exit of program */
    atexit(SDL_Quit);
    APP.quit = false;

    SDL_Event e;
    /* Only updates display and processes inputs on new events */
    while ( !APP.quit && SDL_WaitEvent(&e) ) {
        if ( e.type == SDL_MOUSEMOTION) continue;
        /* Handle input before rendering */
        eventHandler(&e);
        logPrint("Event handler done\n");

        logPrint("Prepare scene start...\n");
        prepareScene();
        logPrint("Prepare scene end.\n");

        logPrint("Present scene start...\n");
        presentScene();
        logPrint("Present scene end.\n");

        SDL_Delay(0);
    }

    if (HINT_BUFFER.buf) free(HINT_BUFFER.buf);
    free(FILENAME_BUFFER.buf);
    /* delete nodes recursively, starting from root */
    removeNodeFromGraph(GRAPH.root);
    logPrint("Deleted all nodes\n");
    SDL_DestroyRenderer( APP.renderer );
    SDL_DestroyWindow( APP.window );
    SDL_Quit();
    return 0;
}
