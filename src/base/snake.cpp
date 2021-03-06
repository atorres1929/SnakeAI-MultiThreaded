#include "base/snake.h"
#include "util/util.h"
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <future>
#include <omp.h>

using std::vector;
using std::list;
using std::queue;
using util::Random;

Snake::Snake() {}

Snake::~Snake() {}

void Snake::setDirection(const Direction &d) {
	direc = d;
}

void Snake::setMap(Map *const m) {
	map = m;
}

Direction Snake::getDirection() const {
	return direc;
}

bool Snake::isDead() const {
	return dead;
}

void Snake::testMinPath(const Pos &from, const Pos &to, std::list<Direction> &path) {
	map->setTestEnabled(true);
	findMinPath(from, to, path);
	map->showTestPath(from, path);
	map->setTestEnabled(false);
}

void Snake::testMaxPath(const Pos &from, const Pos &to, std::list<Direction> &path) {
	map->setTestEnabled(true);
	findMaxPath(from, to, path);
	Pos cur = from;
	for (const Direction d : path) {
		map->getPoint(cur).setDist(Point::EMPTY_DIST);
		cur = cur.getAdj(d);
	}
	map->getPoint(from).setDist(0);
	map->getPoint(to).setDist(1);
	map->showTestPath(from, path);
	map->setTestEnabled(false);
}

void Snake::testHamilton() {
	map->setTestEnabled(true);
	enableHamilton();
	SizeType row = map->getRowCount(), col = map->getColCount();
	for (SizeType i = 1; i < row - 1; ++i) {
		for (SizeType j = 1; j < col - 1; ++j) {
			Pos pos = Pos(i, j);
			Point &point = map->getPoint(pos);
			point.setDist(point.getIdx());
			map->showTestPos(pos);
		}
	}
}

void Snake::testPathSearch() {
	while (bodies.size() < 4 && !endTest) {
		decideNext();
		move();
	}
}

bool Snake::isThreaded() {
	return threaded;
}

double Snake::getMaxTimeBFS() {
	return maxTimeBFS;
}

double Snake::getTotalTimeBFS() {
	return totalTimeBFS;
}

double Snake::getTotalTimeGraphSearch() {
	return totalTimeGraphSearch;
}

double Snake::getMaxTimeGraphSearch() {
	return maxTimeGraphSearch;
}

int Snake::getMaxNumThreadsBFS() {
	return maxNumThreadsBFS;
}

int Snake::getMaxNumThreadsGraphSearch() {
	return maxNumThreadsGraphSearch;
}

void Snake::addBody(const Pos &p) {
	if (bodies.size() == 0) {  // Insert a head
		map->getPoint(p).setType(Point::Type::SNAKE_HEAD);
	}
	else {  // Insert a body
		if (bodies.size() > 1) {
			const Pos &oldTail = getTail();
			map->getPoint(oldTail).setType(Point::Type::SNAKE_BODY);
		}
		map->getPoint(p).setType(Point::Type::SNAKE_TAIL);
	}
	bodies.push_back(p);
}

void Snake::move() {
	if (isDead() || direc == NONE) {
		return;
	}
	map->getPoint(getHead()).setType(Point::Type::SNAKE_BODY);
	Pos newHead = getHead().getAdj(direc);
	bodies.push_front(newHead);
	if (!map->isSafe(newHead)) {
		dead = true;
	}
	else {
		if (map->getPoint(newHead).getType() != Point::Type::FOOD) {
			removeTail();
		}
		else {
			map->removeFood();
		}
	}
	map->getPoint(newHead).setType(Point::Type::SNAKE_HEAD);
}

void Snake::move(const std::list<Direction> &path) {
	for (const Direction &d : path) {
		setDirection(d);
		move();
	}
}

void Snake::enableHamilton() {
	if (map->getRowCount() % 2 == 1 && map->getColCount() % 2 == 1) {
		throw std::range_error("Snake.enableHamilton(): require even amount of rows or columns.");
	}
	hamiltonEnabled = true;
	buildHamilton();
}

void Snake::enableThreaded() {
	threaded = true;
}

void Snake::decideNext() {
	if (isDead()) {
		return;
	}
	else if (!map->hasFood()) {
		direc = NONE;
		return;
	}

	if (hamiltonEnabled) {  // AI based on the Hamiltonian cycle

		SizeType size = map->getSize();
		Pos head = getHead(), tail = getTail();
		Point::ValueType tailIndex = map->getPoint(tail).getIdx();
		Point::ValueType headIndex = map->getPoint(head).getIdx();
		// Try to take shortcuts when the snake is not long enough
		if (bodies.size() < size * 3 / 4) {
			list<Direction> minPath;
			findMinPathToFood(minPath);
			if (!minPath.empty()) {
				Direction nextDirec = *minPath.begin();
				Pos nextPos = head.getAdj(nextDirec);
				Point::ValueType nextIndex = map->getPoint(nextPos).getIdx();
				Point::ValueType foodIndex = map->getPoint(map->getFood()).getIdx();
				headIndex = util::getDistance(tailIndex, headIndex, (Point::ValueType)size);
				nextIndex = util::getDistance(tailIndex, nextIndex, (Point::ValueType)size);
				foodIndex = util::getDistance(tailIndex, foodIndex, (Point::ValueType)size);
				if (nextIndex > headIndex && nextIndex <= foodIndex) {
					direc = nextDirec;
					return;
				}
			}
		}
		// Move along the hamitonian cycle
		headIndex = map->getPoint(head).getIdx();
		vector<Pos> adjPositions = head.getAllAdj();

		for (const Pos &adjPos : adjPositions) {
			const Point &adjPoint = map->getPoint(adjPos);
			Point::ValueType adjIndex = adjPoint.getIdx();
			if (adjIndex == (headIndex + 1) % size) {
				direc = head.getDirectionTo(adjPos);
			}
		}

	}
	else {  // AI based on graph search
		list<Direction> pathToFood, pathToTail;
		// Create a virtual snake
		Map tmpMap = *map;
		Snake tmpSnake(*this);
		tmpSnake.setMap(&tmpMap);
		// Step 1
		tmpSnake.findMinPathToFood(pathToFood);
		totalTimeGraphSearch = tmpSnake.getTotalTimeGraphSearch();
		maxTimeGraphSearch = tmpSnake.getMaxTimeGraphSearch();
		maxNumThreadsGraphSearch = tmpSnake.getMaxNumThreadsGraphSearch();
		if (!pathToFood.empty()) {
			// Step 2
			tmpSnake.move(pathToFood);
			totalTimeGraphSearch = tmpSnake.getTotalTimeGraphSearch();
			maxTimeGraphSearch = tmpSnake.getMaxTimeGraphSearch();
			maxNumThreadsGraphSearch = tmpSnake.getMaxNumThreadsGraphSearch();
			if (tmpMap.isAllBody()) {
				this->setDirection(*(pathToFood.begin()));
				return;
			}
			else {
				// Step 3
				tmpSnake.findMaxPathToTail(pathToTail);
				totalTimeGraphSearch = tmpSnake.getTotalTimeGraphSearch();
				maxTimeGraphSearch = tmpSnake.getMaxTimeGraphSearch();
				maxNumThreadsGraphSearch = tmpSnake.getMaxNumThreadsGraphSearch();
				if (pathToTail.size() > 1) {
					this->setDirection(*(pathToFood.begin()));
					return;
				}
			}
		}

		if (tmpSnake.bodies.size() == 4)
			endTest = true;

		// Step 4
		this->findMaxPathToTail(pathToTail);
		if (pathToTail.size() > 1) {
			this->setDirection(*(pathToTail.begin()));
			return;
		}
		// Step 5
		direc = Direction::DOWN;  // A default direction
		SizeType max = 0;
		Pos head = getHead();
		vector<Pos> adjPositions = head.getAllAdj();
		for (const Pos &adjPos : adjPositions) {
			if (map->isSafe(adjPos)) {
				SizeType dist = Map::distance(adjPos, map->getFood());
				if (dist >= max) {
					max = dist;
					direc = head.getDirectionTo(adjPos);
				}
			}
		}
	}
}

const Pos& Snake::getHead() const {
	return *bodies.begin();
}

const Pos& Snake::getTail() const {
	return *bodies.rbegin();
}

void Snake::removeTail() {
	map->getPoint(getTail()).setType(Point::Type::EMPTY);
	bodies.pop_back();
	if (bodies.size() > 1) {
		map->getPoint(getTail()).setType(Point::Type::SNAKE_TAIL);
	}
}

void Snake::findMinPathToFood(list<Direction> &path) {
	findPathTo(0, map->getFood(), path);
}

void Snake::findMaxPathToTail(list<Direction> &path) {
	findPathTo(1, getTail(), path);
}

void Snake::findPathTo(const int pathType, const Pos &goal, list<Direction> &path) {
	Point::Type oriType = map->getPoint(goal).getType();
	map->getPoint(goal).setType(Point::Type::EMPTY);
	if (pathType == 0) {
		std::chrono::system_clock::time_point beginTime = std::chrono::system_clock::now();
		if (threaded) {
			findMinPathThreaded(getHead(), goal, path);
		}
		else {
			findMinPath(getHead(), goal, path);
		}
		std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = endTime - beginTime;
		if (maxTimeBFS < elapsed_seconds.count()) {
			maxTimeBFS = elapsed_seconds.count();
		}
		totalTimeBFS += elapsed_seconds.count();
	}
	else if (pathType == 1) {
		std::chrono::system_clock::time_point beginTime = std::chrono::system_clock::now();
		if (threaded) {
			findMaxPathThreaded(getHead(), goal, path);
		}
		else {
			findMaxPath(getHead(), goal, path);
		}
		std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = endTime - beginTime;
		if (maxTimeGraphSearch < elapsed_seconds.count()) {
			maxTimeGraphSearch = elapsed_seconds.count();
		}
		totalTimeGraphSearch += elapsed_seconds.count();
	}
	map->getPoint(goal).setType(oriType);  // Retore point type
}
/********************************************************************************************************************
FOR THE PROFESSOR: THE FOLLOWING IS THE THREADED IMPLEMENTATION OF THE BFS
Basically the only code I ended up adding was the OpenMP stuff.
A lot of testing was done before this, as can be seen by the commits on our github, but in the end, the solution
ended up being fairly simple.
*********************************************************************************************************************/
void Snake::findMinPathThreaded(const Pos &from, const Pos &to, list<Direction> &path) {
	// Init
	SizeType row = map->getRowCount(), col = map->getColCount();
	for (SizeType i = 1; i < row - 1; ++i) {
		for (SizeType j = 1; j < col - 1; ++j) {
			map->getPoint(Pos(i, j)).setDist(Point::MAX_VALUE);
		}
	}
	path.clear();
	map->getPoint(from).setDist(0);

	//Push first element to queue
	queue<Pos> openList;
	openList.push(from);


	// BFS
	while (!openList.empty()) {
		int queueSize = openList.size();
		//Make every iteration of the for loop a separate thread
#pragma omp parallel for //Will happen every iteration of the while loop
		for (int i = 0; i < queueSize; i++) {
			Pos curPos;
#pragma omp critical //Restrict access to queue to 1 thread at a time
			{
				curPos = openList.front();
				openList.pop();
			}
			const Point &curPoint = map->getPoint(curPos);
			map->showTestPos(curPos);
			if (curPos == to) {
				buildPath(from, to, path);
				openList.empty();
			}
			vector<Pos> adjPositions = curPos.getAllAdj();
			Random<>::getInstance()->shuffle(adjPositions.begin(), adjPositions.end());
			// Arrange the order of traversing to make the result path as straight as possible
			Direction bestDirec = (curPos == from ? direc : curPoint.getParent().getDirectionTo(curPos));
			for (SizeType i = 0; i < adjPositions.size(); ++i) {
				if (bestDirec == curPos.getDirectionTo(adjPositions[i])) {
					util::swap(adjPositions[0], adjPositions[i]);
					break;
				}
			}

			// Traverse the adjacent positions
			/*
			This was not parallelized due to the fact that there
			was not much work being done in the for loop. Even in the 
			max case (4), there would be no benefit. In fact, I suspect there
			would be a detriment. Starting each thread, and waiting for them all
			to finish would be slower than simply doing it sequentially.
			*/
			for (int j = 0; j < adjPositions.size(); j++) {
				const Pos &adjPos = adjPositions.at(j);
				Point &adjPoint = map->getPoint(adjPos);
				if (map->isEmpty(adjPos) && adjPoint.getDist() == Point::MAX_VALUE) {
					adjPoint.setParent(curPos);
					adjPoint.setDist(curPoint.getDist() + 1);
#pragma omp critical //Restrict access to queue to 1 thread at a time
					openList.push(adjPos);
				}
				if (maxNumThreadsBFS < omp_get_num_threads()) {
					maxNumThreadsBFS = omp_get_num_threads();
				}
			}
		}
	}
}

void Snake::findMinPath(const Pos &from, const Pos &to, list<Direction> &path) {
	// Init
	SizeType row = map->getRowCount(), col = map->getColCount();
	for (SizeType i = 1; i < row - 1; ++i) {
		for (SizeType j = 1; j < col - 1; ++j) {
			map->getPoint(Pos(i, j)).setDist(Point::MAX_VALUE);
		}
	}
	path.clear();
	map->getPoint(from).setDist(0);
	queue<Pos> openList;
	openList.push(from);

	// BFS
	while (!openList.empty()) {
		Pos curPos = openList.front();
		const Point &curPoint = map->getPoint(curPos);
		openList.pop();
		map->showTestPos(curPos);
		if (curPos == to) {
			buildPath(from, to, path);
			break;
		}
		vector<Pos> adjPositions = curPos.getAllAdj();
		Random<>::getInstance()->shuffle(adjPositions.begin(), adjPositions.end());
		// Arrange the order of traversing to make the result path as straight as possible
		Direction bestDirec = (curPos == from ? direc : curPoint.getParent().getDirectionTo(curPos));
		for (SizeType i = 0; i < adjPositions.size(); ++i) {
			if (bestDirec == curPos.getDirectionTo(adjPositions[i])) {
				util::swap(adjPositions[0], adjPositions[i]);
				break;
			}
		}

		// Traverse the adjacent positions

		for (const Pos &adjPos : adjPositions) {
			Point &adjPoint = map->getPoint(adjPos);
			if (map->isEmpty(adjPos) && adjPoint.getDist() == Point::MAX_VALUE) {
				adjPoint.setParent(curPos);
				adjPoint.setDist(curPoint.getDist() + 1);
				openList.push(adjPos);
			}
			if (maxNumThreadsBFS < omp_get_num_threads()) {
				maxNumThreadsBFS = omp_get_num_threads();
			}
		}
	}
}

void Snake::findMaxPath(const Pos &from, const Pos &to, list<Direction> &path) {
	// Get the shortest path
	bool oriEnabled = map->isTestEnabled();
	map->setTestEnabled(false);
	findMinPath(from, to, path);
	map->setTestEnabled(oriEnabled);
	// Init
	SizeType row = map->getRowCount(), col = map->getColCount();
	for (SizeType i = 1; i < row - 1; ++i) {
		for (SizeType j = 1; j < col - 1; ++j) {
			map->getPoint(Pos(i, j)).setVisit(false);
		}
	}
	// Make all points on the path visited
	Pos cur = from;
	for (const Direction d : path) {
		map->getPoint(cur).setVisit(true);
		cur = cur.getAdj(d);
	}
	map->getPoint(cur).setVisit(true);
	// Extend the path between each pair of the points
	for (auto it = path.begin(); it != path.end();) {
		if (it == path.begin()) {
			cur = from;
		}
		bool extended = false;
		Direction curDirec = *it;
		Pos next = cur.getAdj(curDirec);
		switch (curDirec) {
		case LEFT:
		case RIGHT: {
			Pos curUp = cur.getAdj(UP);
			Pos nextUp = next.getAdj(UP);
			// Check two points above
			if (map->isEmptyNotVisit(curUp) && map->isEmptyNotVisit(nextUp)) {
				map->getPoint(curUp).setVisit(true);
				map->getPoint(nextUp).setVisit(true);
				it = path.erase(it);
				it = path.insert(it, DOWN);
				it = path.insert(it, curDirec);
				it = path.insert(it, UP);
				it = path.begin();
				extended = true;
			}
			else {
				Pos curDown = cur.getAdj(DOWN);
				Pos nextDown = next.getAdj(DOWN);
				// Check two points below
				if (map->isEmptyNotVisit(curDown) && map->isEmptyNotVisit(nextDown)) {
					map->getPoint(curDown).setVisit(true);
					map->getPoint(nextDown).setVisit(true);
					it = path.erase(it);
					it = path.insert(it, UP);
					it = path.insert(it, curDirec);
					it = path.insert(it, DOWN);
					it = path.begin();
					extended = true;
				}
			}
			break;
		}
		case UP:
		case DOWN: {
			Pos curLeft = cur.getAdj(LEFT);
			Pos nextLeft = next.getAdj(LEFT);
			// Check two points on the left
			if (map->isEmptyNotVisit(curLeft) && map->isEmptyNotVisit(nextLeft)) {
				map->getPoint(curLeft).setVisit(true);
				map->getPoint(nextLeft).setVisit(true);
				it = path.erase(it);
				it = path.insert(it, RIGHT);
				it = path.insert(it, curDirec);
				it = path.insert(it, LEFT);
				it = path.begin();
				extended = true;
			}
			else {
				Pos curRight = cur.getAdj(RIGHT);
				Pos nextRight = next.getAdj(RIGHT);
				// Check two points on the right
				if (map->isEmptyNotVisit(curRight) && map->isEmptyNotVisit(nextRight)) {
					map->getPoint(curRight).setVisit(true);
					map->getPoint(nextRight).setVisit(true);
					it = path.erase(it);
					it = path.insert(it, LEFT);
					it = path.insert(it, curDirec);
					it = path.insert(it, RIGHT);
					it = path.begin();
					extended = true;
				}
			}
			break;
		}
		default:
			break;
		}
		if (!extended) {
			++it;
			cur = next;
		}
	}
	if (maxNumThreadsGraphSearch < omp_get_num_threads()) {
		maxNumThreadsGraphSearch = omp_get_num_threads();
	}
}

void Snake::findMaxPathThreaded(const Pos &from, const Pos &to, list<Direction> &path) {
	// Get the shortest path
	bool oriEnabled = map->isTestEnabled();
	map->setTestEnabled(false);
	findMinPath(from, to, path);
	map->setTestEnabled(oriEnabled);
	// Init
	SizeType row = map->getRowCount(), col = map->getColCount();
	for (SizeType i = 1; i < row - 1; ++i) {
		for (SizeType j = 1; j < col - 1; ++j) {
			map->getPoint(Pos(i, j)).setVisit(false);
		}
	}
	// Make all points on the path visited
	Pos cur = from;
	for (const Direction d : path) {
		map->getPoint(cur).setVisit(true);
		cur = cur.getAdj(d);
	}
	map->getPoint(cur).setVisit(true);
	// Extend the path between each pair of the points
	for (auto it = path.begin(); it != path.end();) {
		if (it == path.begin()) {
			cur = from;
		}
		bool extended = false;
		Direction curDirec = *it;
		Pos next = cur.getAdj(curDirec);
		switch (curDirec) {
		case LEFT:
		case RIGHT: {
			Pos curUp = cur.getAdj(UP);
			Pos nextUp = next.getAdj(UP);
			// Check two points above
			if (map->isEmptyNotVisit(curUp) && map->isEmptyNotVisit(nextUp)) {
				map->getPoint(curUp).setVisit(true);
				map->getPoint(nextUp).setVisit(true);
				it = path.erase(it);
				it = path.insert(it, DOWN);
				it = path.insert(it, curDirec);
				it = path.insert(it, UP);
				it = path.begin();
				extended = true;
			}
			else {
				Pos curDown = cur.getAdj(DOWN);
				Pos nextDown = next.getAdj(DOWN);
				// Check two points below
				if (map->isEmptyNotVisit(curDown) && map->isEmptyNotVisit(nextDown)) {
					map->getPoint(curDown).setVisit(true);
					map->getPoint(nextDown).setVisit(true);
					it = path.erase(it);
					it = path.insert(it, UP);
					it = path.insert(it, curDirec);
					it = path.insert(it, DOWN);
					it = path.begin();
					extended = true;
				}
			}
			break;
		}
		case UP:
		case DOWN: {
			Pos curLeft = cur.getAdj(LEFT);
			Pos nextLeft = next.getAdj(LEFT);
			// Check two points on the left
			if (map->isEmptyNotVisit(curLeft) && map->isEmptyNotVisit(nextLeft)) {
				map->getPoint(curLeft).setVisit(true);
				map->getPoint(nextLeft).setVisit(true);
				it = path.erase(it);
				it = path.insert(it, RIGHT);
				it = path.insert(it, curDirec);
				it = path.insert(it, LEFT);
				it = path.begin();
				extended = true;
			}
			else {
				Pos curRight = cur.getAdj(RIGHT);
				Pos nextRight = next.getAdj(RIGHT);
				// Check two points on the right
				if (map->isEmptyNotVisit(curRight) && map->isEmptyNotVisit(nextRight)) {
					map->getPoint(curRight).setVisit(true);
					map->getPoint(nextRight).setVisit(true);
					it = path.erase(it);
					it = path.insert(it, LEFT);
					it = path.insert(it, curDirec);
					it = path.insert(it, RIGHT);
					it = path.begin();
					extended = true;
				}
			}
			break;
		}
		default:
			break;
		}
		if (!extended) {
			++it;
			cur = next;
		}
	}
	if (maxNumThreadsGraphSearch < omp_get_num_threads()) {
		maxNumThreadsGraphSearch = omp_get_num_threads();
	}
}

void Snake::buildPath(const Pos &from, const Pos &to, list<Direction> &path) const {
	Pos tmp = to, parent;
	while (tmp != from) {
		parent = map->getPoint(tmp).getParent();
		path.push_front(parent.getDirectionTo(tmp));
		tmp = parent;
	}
}

void Snake::buildHamilton() {
	// Change the initial body to a wall temporarily
	Pos bodyPos = *(++bodies.begin());
	Point &bodyPoint = map->getPoint(bodyPos);
	map->getPoint(*(++bodies.begin())).setType(Point::Type::WALL);
	// Get the longest path
	bool oriEnabled = map->isTestEnabled();
	map->setTestEnabled(false);
	list<Direction> maxPath;
	findMaxPathToTail(maxPath);
	map->setTestEnabled(oriEnabled);
	bodyPoint.setType(Point::Type::SNAKE_BODY);
	// Initialize the first three incides of the cycle
	Point::ValueType idx = 0;
	for (auto it = bodies.crbegin(); it != bodies.crend(); ++it) {
		map->getPoint(*it).setIdx(idx++);
	}
	// Build remaining cycle
	SizeType size = map->getSize();
	Pos cur = getHead();
	for (const Direction d : maxPath) {
		Pos next = cur.getAdj(d);
		map->getPoint(next).setIdx((map->getPoint(cur).getIdx() + 1) % size);
		cur = next;
	}
}
