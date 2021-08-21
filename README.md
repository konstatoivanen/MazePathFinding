# Maze Path Finding
A small project that generates a 64 x 32 maze using depth-first search algorithm and solves paths within it using A*.

![Preview](T_Preview.gif?raw=true "Preview")

## Techical Details

### Execution Flow
- Initialize GLFW (A windowing library for creating gfx contexes for various graphics APIs. In this case OpenGL).
- Load GL function addresses (Using GLAD).
- Generate maze (Further elaboration can be found bellow).
- Initialize GFX resources (Textures, Shaders & Vertex arrays).
- Assign GFX resources to shader pipelines.
- Loop until window close condition is detected.
    - Get cursor position & mouse button states.
    - Update path end & start positions based on collected button states.
    - Regenerate path from start to end point using A* (Further elaboration can be found bellow).
    - Update path texture.
    - Draw maze & path.
- Release resources & deinitialize.

### Maze Generation
The maze generation uses the depth-first search algorithm produces a grid of nodes used for path finding & a texture for visualizing the maze.
The algorithm itself is quite simple an can be summarized  by the following pseudo code:
```
Declare a node stack (This will serve as a trace back to the starting node).
Push the starting node to the stack (Any node will do. In the project it is index 0 by default).

Loop until all nodes are visited
    Select an active node from the top of the stack.

    If the active node has any unvisited neighbours pick one from them at random.
       Mark the neighbour as visited as well as set it & the active node as connected.
       Push the neighbour into the stack.
       Continue.

    If the active node has no unvisited neighbours pop the active node from the stack 
    (This will cause us to backtrace towards the starting node until we find a node with unvisited neighbours).
```
After the maze is ready we generate a texture where for each node that isn't connected to the south or to the east we draw a border.

## Pathfinding
A* the traverses the node grid produced by maze generation. For which the nodes are defined in the following way:
```
struct PNode
{
    float distanceLocal;   // The accumulated distance it has taken for path finding to reach this node.
    float distanceGlobal;  // distanceLocal + linear distance to end node from this node.
    uint32_t flags;        // First 4 bits describe the connections to adjacent nodes. 5th bit is used to flag the cell as visited.
    uint32_t parentIndex;  // During pathfinding we will through which node did we arrive to this node.
                           // This will later be used to construct the path from end to start node.
};
```
Due to the fixed distance between each node & the simplicity of node adjacency information the A* implementation is also quite neat.
For the path heuristic we'll use linear distance between nodes.

```
Select start & end nodes.
Mark all nodes as unvisited & reset local & global distances.
Declare open set & push the start node to it.

Loop until until open set is empty
    Sort open set based on node global distance.
    
    Declare current node popping one from the top of the open set.
    
    If current node is end node.
        Break from the loop.
    
    Mark the current node as visited.

    Enumerate over the current node's neighbours        
        If the distance to the neighbour trough current node is lower than the neighbours current local distance value.
           Set neighbour local distance (current node local distance + 1).
           Set neighbour global distance (local distance + linear distance between this node & end node).
           Set neighbour parent to be current node.
           If the neighbour is not visited push it to the open set.
            
Once end node has been found construct the path by traversing through the node graph using the parent value of each node.
```

## Controls
- Hold the left mouse button to move path starting point.
- Hold the right mouse button to move path ending point.
 
## Dependencies
- OpenGL 4.5
- [Glad](https://glad.dav1d.de/)
- [GLFW](https://www.glfw.org/)

## References
- [Depth-First Search](https://www.algosome.com/articles/maze-generation-depth-first.html)
- [A*](https://en.wikipedia.org/wiki/A*_search_algorithm)
