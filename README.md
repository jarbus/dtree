# dtree
A minimal decision tree planning app

# Requirements

* SDL2
* SDL2_ttf
* make
* gcc

# Usage
The application provides 5 modes of interaction with the decision tree- `Default`, `Edit`, `Travel`, `Delete`, `Move`.

In Default Mode:
    - `o` : make a child node for the current node
    - `k` : select the parent node of the current node
    - `e` : edit current node's text
    - `t` : activate travel command
    - `x` : activate delete command
    - `m` : activate move command
    - `r` : edit file name
    - `w` : save file
    - `q` : quit the program

In Any Mode:
    - press `esc` to return to Default Mode

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
