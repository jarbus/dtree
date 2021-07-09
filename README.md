# dtree
A minimal decision tree planning app

# Requirements

* SDL2
* SDL2_ttf
* make
* gcc

# Usage
The application provides 5 modes of interaction with the decision tree- `Default`, `Edit`, `Travel`, `Delete`, `Move`.

In Travel Mode:

* `o` : make a child node for the current node
* `k` : select the parent node of the current node
* `e` : edit current node's text
* `x` : activate delete mode for one node
* `m` : activate move mode for one node
* `c` : persist the next mode
* `r` : edit file name
* `w` : save file
* `q` : quit the program

In Any Mode:
    - press `esc` to return to travel mode and switch mode-persist off

In Edit Mode:
    - type to enter text

## Hint Keys and Hint Modes

Some modes allow you to select nodes by entering their corresponding red characters. These characters are called "Hint Keys" and modes that use hint keys to select nodes are called "Hint Modes".

In Travel Mode:
    - press hint keys in red to travel to corresponding node

In Delete Mode:
    - press hint keys to delete the corresponding node

In Move Mode:
    - press hint keys to select a node to move, then again to select a target parent
