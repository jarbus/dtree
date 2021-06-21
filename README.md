# dtree
A minimal decision tree planning app

# Requirements

* SDL2
* SDL2_ttf
* make
* gcc

# Usage
The application provides three modes of interaction with the decision tree- `Default`, `Edit`, and `Travel`.

In Default Mode:
 - `o` : make a child node for the current node
 - `d` : delete the current node
 - `h` : select the leftmost child of the current node
 - `l` : select the rightmost child of the current node
 - `k` : select the parent node of the current node
 - `t` : enter travel mode
 - `e` : enter edit mode
 - `q` : quit the program

In Edit Mode:
 - type to enter text
 - press `esc` to return to Default Mode

In Travel Mode:
 - press `esc` to return to Default Mode
