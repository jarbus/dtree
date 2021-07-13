# dtree
A minimal decision tree planning app

# Requirements

* SDL2
* SDL2_ttf
* make
* gcc

# Usage
The application provides 6 modes of interaction with the decision tree- `Travel`, `MakeChild`, `Edit`, `Delete`, `Cut`, `Paste`.

In Travel Mode:

* `k` : select the parent node of the current node
* `e` : switch to Edit mode
* `s` : clear the selected text, then switch to edit mode
* `o` : activate MakeChild mode for one node
* `x` : activate Delete mode for one node
* `m` : activate Cut mode for one node
* `p` : activate Paste mode for one node
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

* press hint keys to travel to corresponding node

In MakeChild Mode:

* press hint keys to create a child of the corresponding node

In Delete Mode:

*  press hint keys to delete the corresponding node

In Cut Mode:

* press hint keys to select a node to cut

In Paste Mode:

* select a new parent for the cut node
