#ifndef SNAKE_SNAKE_H
#define SNAKE_SNAKE_H

#include "base/map.h"

/*
Game snake.
*/
class Snake {
public:
    typedef Map::SizeType SizeType;

public:
    Snake();
    ~Snake();

    bool isDead() const;

    void setMap(Map *const m);

    void setDirection(const Direction &d);
    Direction getDirection() const;

    void addBody(const Pos &p);

    /*
    Move the snake at current direction.
    */
    void move();

    /*
    Move the snake along a given path.
    */
    void move(const std::list<Direction> &path);

    /*
    Enable the snake AI based on the Hamiltonian cycle.
    */
    void enableHamilton();

	/*
	Changes the game to run on threads
	*/
	void enableThreaded();

	/*
	Get the time that the longest BFS took
	*/
	double getMaxTimeBFS();

	/*
	Get the total amount of time to complete the BFS
	*/
	double getTotalTimeBFS();
	
	/*
	Get the total amount of time to complete the Graph Search
	*/
	double getTotalTimeGraphSearch();

	/*
	Get the time that the longest Graph Search took
	*/
	double getMaxTimeGraphSearch();

	/*
	Is the algorithm threaded
	*/
	bool isThreaded();

    /*
    Decide the next moving direction. After its execution,
    the next moving direction can be got by calling getDirection().
    */
    void decideNext();

    void testMinPath(const Pos &from, const Pos &to, std::list<Direction> &path);
    void testMaxPath(const Pos &from, const Pos &to, std::list<Direction> &path);
    void testHamilton();
	void testPathSearch();
	int getMaxNumThreadsBFS();
	int getMaxNumThreadsGraphSearch();

private:
    void removeTail();

    const Pos& getHead() const;
    const Pos& getTail() const;

    void findMinPathToFood(std::list<Direction> &path);
    void findMaxPathToTail(std::list<Direction> &path);

    /*
    Find path from the snake's head to a given position.

    @param type 0->shortest path, 1->longest path
    @param to   The given position
    @param path The result path will be stored in this field.
    */
    void findPathTo(const int type, const Pos &to, std::list<Direction> &path);

    /*
    Find the shortest path AS STRAIGHT AS POSSIBLE between two positions.

    @param from The starting position
    @param to   The ending position
    @param path The result will be stored in this field
    */
    void findMinPath(const Pos &from, const Pos &to, std::list<Direction> &path);


	/*
	Find the shortest path AS STRAIGHT AS POSSIBLE between two positions.
	This implementation was threaded to increase performance

	@param from The starting position
	@param to   The ending position
	@param path The result will be stored in this field
	*/
	void findMinPathThreaded(const Pos &from, const Pos &to, std::list<Direction> &path);

    /*
    Find the longest path between two positions.

    @param from The starting position
    @param to   The ending position
    @param path The result will be stored in this field
    */
    void findMaxPath(const Pos &from, const Pos &to, std::list<Direction> &path);

	/*
	Find the longest path between two positions.
	This implementation was threaded to increase performance

	@param from The starting position
	@param to   The ending position
	@param path The result will be stored in this field
	*/
	void findMaxPathThreaded(const Pos &from, const Pos &to, std::list<Direction> &path);

    /*
    Build a path between two positions.

    @param from The start position
    @param to   The end position
    @param path The result will be stored in this field.
    */
    void buildPath(const Pos &from, const Pos &to, std::list<Direction> &path) const;

    /*
    Build a Hamiltonian cycle on the map.
    The path index will be stored in the 'value' field of each Point.
    */
    void buildHamilton();

private:
    Map *map = nullptr;
    std::list<Pos> bodies;
    Direction direc = NONE;
    bool dead = false;
    bool hamiltonEnabled = false;
	bool threaded = false;
	bool endTest = false;
	double maxTimeBFS = 0;
	double totalTimeBFS = 0;
	double totalTimeGraphSearch = 0;
	double maxTimeGraphSearch = 0;
	int maxNumThreadsBFS = 0;
	int maxNumThreadsGraphSearch = 0;
};

#endif
