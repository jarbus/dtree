# dtree
A minimal decision tree planning app

# Requirements

* SDL2
* SDL2_ttf
* make
* gcc

# Usage
The application provides three modes of interaction with the decision tree- `Default`, `Edit`, `Travel`, and `Delete`.

In Default Mode:
 - `o` : make a child node for the current node
 - `k` : select the parent node of the current node
 - `d` : delete the current node
 - `t` : enter travel mode
 - `e` : enter edit mode
 - `x` : enter delete mode
 - `r` : edit file name
 - `q` : quit the program

In Edit Mode:
 - type to enter text
 - press `esc` to return to Default Mode

In Travel Mode:
 - press `esc` or `t` to return to Default Mode
 - press keys in red to travel to corresponding node

In Delete Mode:
 - press `esc` or `x` to return to Default Mode
 - press keys in red to delete the corresponding node
