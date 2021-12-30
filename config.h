#ifndef CONFIG_H_
#define CONFIG_H_
#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

/* User Customizable Variables*/
static const SDL_Color EDIT_COLOR =       {220, 220, 220};
static const SDL_Color HINT_COLOR =       {220, 0, 0};
static const SDL_Color SELECTED_COLOR =   {0, 220, 0};
static const SDL_Color UNSELECTED_COLOR = {0, 55, 0};
static const SDL_Color CUT_COLOR =        {0, 0, 220};
static const SDL_Color EDGE_COLOR =       {220, 220, 220};
static const SDL_Color BACKGROUND_COLOR = {15, 15, 15};
static int TEXTBOX_WIDTH_SCALE = 25;                        // width of char
static int TEXTBOX_HEIGHT = 50;                             // height of char
static double UI_SCALE = 1.0;
static double GRAPH_SCALE = 1.0;
static const double ZOOM_SPEED = 1.1; // rate at which graph zooms out, > 1
static const int FILENAME_BUFFER_MAX_SIZE = 64;
static const int HINT_BUFFER_MAX_SIZE = 3;
static const int MAX_TEXT_LEN = 128;                              // Max Num of chars in a node
static int NUM_CHARS_B4_WRAP = 20;
// radius and thickness of node box
static const int RADIUS = 50;
static const int THICKNESS = 5;
static int FONT_SIZE = 40;
static const char* FONT_NAME = "./assets/SourceCodePro-Regular.otf";   // Default Font name
static const char* HINT_CHARS = "adfghjkl;\0";              // characters to use for hints




#endif // CONFIG_H_
