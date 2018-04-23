#include "gamectrl.h"
#include "util/util.h"
#include <stdexcept>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstdlib>

#ifdef OS_WIN
#include <Windows.h>
#endif

using std::string;
using std::list;
using std::cout;
using std::cin;
using std::endl;

const string GameCtrl::MSG_BAD_ALLOC = "Not enough memory to run the game.";
const string GameCtrl::MSG_LOSE = "Oops! You lose!";
const string GameCtrl::MSG_WIN = "Congratulations! You Win!";
const string GameCtrl::MSG_ESC = "Game ended.";
const string GameCtrl::MAP_INFO_FILENAME = "movements.txt";

GameCtrl::GameCtrl() {}

GameCtrl::~GameCtrl() {
    if (map) {
        delete map;
        map = nullptr;
    }
    if (movementFile) {
        fclose(movementFile);
        movementFile = nullptr;
    }
}

GameCtrl* GameCtrl::getInstance() {
    static GameCtrl instance;
    return &instance;
}

void GameCtrl::setThreaded(const bool threaded) {
	isThreaded = threaded;
}

void GameCtrl::setVisibleGUI(const bool visible) {
	visibleGUI = visible;
}

void GameCtrl::setFPS(const double fps_) {
    fps = fps_;
}

void GameCtrl::setUnlockMovement(const bool unlockMovement_) {
	unlockMovement = unlockMovement_;
}

void GameCtrl::setEnableAI(const bool enableAI_) {
    enableAI = enableAI_;
}

void GameCtrl::setEnableHamilton(const bool enableHamilton_) {
    enableHamilton = enableHamilton_;
}

void GameCtrl::setMoveInterval(const long ms) {
    moveInterval = ms;
}

void GameCtrl::setRecordMovements(const bool b) {
    recordMovements = b;
}

void GameCtrl::setRunTest(const bool b) {
    runTest = b;
}

void GameCtrl::setMapRow(const SizeType n) {
    mapRowCnt = n;
}

void GameCtrl::setMapCol(const SizeType n) {
    mapColCnt = n;
}

int GameCtrl::run() {
    try {
        init();
        if (runTest) {
            test();
        } else {
            mainLoop();
        }
		bool finished = true;
		while (finished) {
			char g;
			cout << "Enter R to Restart || Enter E to Exit";
			cin >> g;
			if (g == 'R' || g == 'r') {
				finished = false;
				cout << endl << endl;
				cout << "==================================================" << endl;
				GameCtrl::run();
			}
			else {
				return 0;
			}
		}
    } catch (const std::exception &e) {
        exitGameErr(e.what());
        return -1;
    }
}

void GameCtrl::sleepFPS() const {
	if (visibleGUI) {
		util::sleep((long)((1.0 / fps) * 1000));
	}
}

void GameCtrl::exitGame(const std::string &msg) {
    mutexExit.lock();
    if (runMainThread) {
        util::sleep(100);
        runSubThread = false;
        util::sleep(100);
        printMsg(msg);
    }
    mutexExit.unlock();
    runMainThread = false;

	std::chrono::duration<double> elapsed_seconds = endTime - beginTime;
	if (runTest) {
		if (snake.isThreaded()) {
			cout << "Threaded" << endl;
		}
		else {
			cout << "Sequential" << endl;
		}
		if (enableHamilton) {
			cout << "Elapsed Time for BFS: ";
			Console::writeWithColor(std::to_string(snake.getTotalTimeBFS()) + "s\n", ConsoleColor(GREEN, BLACK, true, false));
			cout << "Biggest Time for BFS: ";
			Console::writeWithColor(std::to_string(snake.getMaxTimeBFS()) + "s\n", ConsoleColor(CYAN, BLACK, true, false));
			cout << "Elapsed Time for AI:  ";
			Console::writeWithColor(std::to_string(elapsed_seconds.count()) + "s\n", ConsoleColor(YELLOW, BLACK, true, false));
			cout << "Max Threads: " << snake.getMaxNumThreadsBFS() << endl;
		}
		else {
			cout << "Elapsed Time for Graph Search: ";
			Console::writeWithColor(std::to_string(snake.getTotalTimeGraphSearch()) + "s\n", ConsoleColor(GREEN, BLACK, true, false));
			cout << "Biggest Time for Graph Search: ";
			Console::writeWithColor(std::to_string(snake.getMaxTimeGraphSearch()) + "s\n", ConsoleColor(CYAN, BLACK, true, false));
			cout << "Elapsed Time for AI:  ";
			Console::writeWithColor(std::to_string(elapsed_seconds.count()) + "s\n", ConsoleColor(YELLOW, BLACK, true, false));
			cout << "Max Threads: " << snake.getMaxNumThreadsGraphSearch() << endl;
		}
		cout << endl;
	}
}

void GameCtrl::exitGameErr(const std::string &err) {
    exitGame("ERR: " + err);
}

void GameCtrl::printMsg(const std::string &msg) {
	if (visibleGUI && !runTest) {
		Console::setCursor(0, (int)mapRowCnt);
		Console::writeWithColor(msg + "\n", ConsoleColor(WHITE, BLACK, true, false));
	}
}

void GameCtrl::mainLoop() {
    while (runMainThread) {
        if (!pause) {
            if (enableAI) {
                snake.decideNext();
            }
            if (map->isAllBody()) {
                exitGame(MSG_WIN);
            } else if (snake.isDead()) {
                exitGame(MSG_LOSE);
            } else {
                moveSnake();
            }
        }
		if (!unlockMovement) {
			util::sleep(moveInterval);
		}
    }
}

void GameCtrl::moveSnake() {
    mutexMove.lock();
    try {
        snake.move();
        if (recordMovements && snake.getDirection() != NONE) {
            saveMapContent();
        }
        if (!map->hasFood()) {
            map->createRandFood();
        }
        mutexMove.unlock();
    } catch (const std::exception) {
        mutexMove.unlock();
        throw;
    }
}

void GameCtrl::saveMapContent() const {
    if (!movementFile) {
        return;
    }
    SizeType rows = map->getRowCount(), cols = map->getColCount();
    for (SizeType i = 0; i < rows; ++i) {
        for (SizeType j = 0; j < cols; ++j) {
            switch (map->getPoint(Pos(i, j)).getType()) {
                case Point::Type::EMPTY:
                    fwrite("  ", sizeof(char), 2, movementFile); break;
                case Point::Type::WALL:
                    fwrite("# ", sizeof(char), 2, movementFile); break;
                case Point::Type::FOOD:
                    fwrite("F ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_HEAD:
                    fwrite("H ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_BODY:
                    fwrite("B ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_TAIL:
                    fwrite("T ", sizeof(char), 2, movementFile); break;
                default:
                    break;
            }
        }
        fwrite("\n", sizeof(char), 1, movementFile);
    }
    fwrite("\n", sizeof(char), 1, movementFile);
}

void GameCtrl::init() {
	if (visibleGUI) {
		Console::clear();
	}
    initMap();
    if (!runTest) {
        initSnake();
        if (recordMovements) {
            initFiles();
        }
    }
    startSubThreads();
}

void GameCtrl::initMap() {
    if (mapRowCnt < 5 || mapColCnt < 5) {
        string msg = "GameCtrl.initMap(): Map size at least 5*5. Current size "
            + util::toString(mapRowCnt) + "*" + util::toString(mapColCnt) + ".";
        throw std::range_error(msg.c_str());
    }
    map = new Map(mapRowCnt, mapColCnt);
    if (!map) {
        exitGameErr(MSG_BAD_ALLOC);
    } else {
        // Add some extra walls manully
    }
}

void GameCtrl::initSnake() {
    snake.setMap(map);
    snake.addBody(Pos(1, 3));
    snake.addBody(Pos(1, 2));
    snake.addBody(Pos(1, 1));
    if (enableHamilton) {
        snake.enableHamilton();
    }
	if (isThreaded) {
		snake.enableThreaded();
	}
}

void GameCtrl::initFiles() {
#ifdef defined(OSWIN)
    errno_t error = fopen_s(&movementFile, MAP_INFO_FILENAME.c_str(), "w");
#endif
    if (!movementFile) {
        throw std::runtime_error("GameCtrl.initFiles(): Fail to open file: " + MAP_INFO_FILENAME);
    } else {
        // Write content description to the file
        string str = "Content description:\n";
        str += "#: wall\nH: snake head\nB: snake body\nT: snake tail\nF: food\n\n";
        str += "Movements:\n\n";
        fwrite(str.c_str(), sizeof(char), str.length(), movementFile);
    }
}

void GameCtrl::startSubThreads() {
    runSubThread = true;
	if (visibleGUI) {
		drawThread = std::thread(&GameCtrl::draw, this);
		drawThread.detach();
	}
    keyboardThread = std::thread(&GameCtrl::keyboard, this);
    keyboardThread.detach();
}

void GameCtrl::draw() {
    try {
        while (runSubThread) {
            drawMapContent();
            sleepFPS();
        }
    } catch (const std::exception &e) {
        exitGameErr(e.what());
    }
}

void GameCtrl::drawMapContent() const {
    Console::setCursor();
    SizeType row = map->getRowCount(), col = map->getColCount();
    for (SizeType i = 0; i < row; ++i) {
        for (SizeType j = 0; j < col; ++j) {
            const Point &point = map->getPoint(Pos(i, j));
            switch (point.getType()) {
                case Point::Type::EMPTY:
                    Console::writeWithColor("  ", ConsoleColor(BLACK, BLACK));
                    break;
                case Point::Type::WALL:
                    Console::writeWithColor("  ", ConsoleColor(WHITE, WHITE, true, true));
                    break;
                case Point::Type::FOOD:
                    Console::writeWithColor("  ", ConsoleColor(YELLOW, YELLOW, true, true));
                    break;
                case Point::Type::SNAKE_HEAD:
                    Console::writeWithColor("  ", ConsoleColor(RED, RED, true, true));
                    break;
                case Point::Type::SNAKE_BODY:
                    Console::writeWithColor("  ", ConsoleColor(GREEN, GREEN, true, true));
                    break;
                case Point::Type::SNAKE_TAIL:
                    Console::writeWithColor("  ", ConsoleColor(BLUE, BLUE, true, true));
                    break;
                case Point::Type::TEST_VISIT:
                    drawTestPoint(point, ConsoleColor(BLUE, GREEN, true, true));
                    break;
                case Point::Type::TEST_PATH:
                    drawTestPoint(point, ConsoleColor(BLUE, RED, true, true));
                    break;
                default:
                    break;
            }
        }
        Console::write("\n");
    }
}

void GameCtrl::drawTestPoint(const Point &p, const ConsoleColor &consoleColor) const {
    string pointStr = "";
    if (p.getDist() == Point::MAX_VALUE) {
        pointStr = "In";
    } else if (p.getDist() == Point::EMPTY_DIST) {
        pointStr = "  ";
    } else {
        Point::ValueType dist = p.getDist();
        pointStr = util::toString(p.getDist());
        if (dist / 10 == 0) {
            pointStr.insert(0, " ");
        } 
    }
    Console::writeWithColor(pointStr, consoleColor);
}

void GameCtrl::keyboard() {
    try {
        while (runSubThread) {
            if (Console::kbhit()) {
                switch (Console::getch()) {
                    case 'w':
                        keyboardMove(snake, Direction::UP);
                        break;
                    case 'a':
                        keyboardMove(snake, Direction::LEFT);
                        break;
                    case 's':
                        keyboardMove(snake, Direction::DOWN);
                        break;
                    case 'd':
                        keyboardMove(snake, Direction::RIGHT);
                        break;
                    case ' ':
                        pause = !pause;  // Pause or resume game
                        break;
                    case 27:  // Esc
                        exitGame(MSG_ESC);
                        break;
                    default:
                        break;
                }
            }
            sleepFPS();
        }
    } catch (const std::exception &e) {
        exitGameErr(e.what());
    }
}

void GameCtrl::keyboardMove(Snake &s, const Direction d) {
    if (pause) {
        s.setDirection(d);
        moveSnake();
    } else if (!enableAI) {
        if (s.getDirection() == d) {
            moveSnake();  // Accelerate
        } else {
            s.setDirection(d);
        }
    }
}

void GameCtrl::test() {
    //testFood();
    //testSearch();
    //testHamilton();
	testSequentialPathSearch();
	//testSequentialPathSearch();
	//testThreadedPathSearch();
	//testThreadedPathSearch();
}


void GameCtrl::testSequentialPathSearch() {
	if (mapRowCnt < 10 || mapColCnt < 10) {
		throw std::range_error("GameCtrl.testSequentialPathSearch() requires map size 10x10");
	}
	if (mapRowCnt == 20 && mapColCnt == 20) {
		map->createFood(Pos(18, 18));
	}
	else if (mapRowCnt == 10 && mapColCnt == 10) {
		map->createFood(Pos(8, 8));
	}
	else if (mapRowCnt == 100 && mapColCnt == 100) {
		map->createFood(Pos(98, 98));
	}
	else if (mapRowCnt == 50 && mapColCnt == 50) {
		map->createFood(Pos(48, 48));
	}
	else if (mapRowCnt == 30 && mapColCnt == 30) {
		map->createFood(Pos(28, 28));
	}
	snake = Snake();
	snake.setMap(map);
	snake.addBody(Pos(1, 3));
	snake.addBody(Pos(1, 2));
	snake.addBody(Pos(1, 1));
	if (enableHamilton)
		snake.enableHamilton();
	beginTime = std::chrono::system_clock::now();
	snake.testPathSearch();
	endTime = std::chrono::system_clock::now();
	exitGame("testSequentialPathSearch() finished.");
}

void GameCtrl::testThreadedPathSearch() {
	if (mapRowCnt < 10 || mapColCnt < 10) {
		throw std::range_error("GameCtrl.testThreadedPathSearch() requires map size 10x10");
	}
	if (mapRowCnt == 20 && mapColCnt == 20) {
		map->createFood(Pos(18, 18));
	}
	else if (mapRowCnt == 10 && mapColCnt == 10) {
		map->createFood(Pos(8, 8));
	}
	else if (mapRowCnt == 100 && mapColCnt == 100) {
		map->createFood(Pos(98, 98));
	}
	else if (mapRowCnt == 50 && mapColCnt == 50) {
		map->createFood(Pos(48, 48));
	}
	else if (mapRowCnt == 30 && mapColCnt == 30) {
		map->createFood(Pos(28, 28));
	}
	snake = Snake();
	snake.setMap(map);
	snake.enableThreaded();
	snake.addBody(Pos(1, 3));
	snake.addBody(Pos(1, 2));
	snake.addBody(Pos(1, 1));
	if (enableHamilton)
		snake.enableHamilton();
	beginTime = std::chrono::system_clock::now();
	snake.testPathSearch();
	endTime = std::chrono::system_clock::now();
	exitGame("testThreadedPathSearch() finished.");
}


void GameCtrl::testFood() {
    SizeType cnt = 0;
    while (runMainThread && cnt++ < map->getSize()) {
        map->createRandFood();
        sleepFPS();
    }
    exitGame("testFood() finished.");
}

void GameCtrl::testSearch() {
    if (mapRowCnt != 20 || mapColCnt != 20) {
        throw std::range_error("GameCtrl.testSearch(): Require map size 20*20.");
    }

    list<Direction> path;
    snake.setMap(map);

    // Add walls for testing
    for (int i = 4; i < 16; ++i) {
        map->getPoint(Pos(i, 9)).setType(Point::Type::WALL);   // vertical
        map->getPoint(Pos(4, i)).setType(Point::Type::WALL);   // horizontal #1
        map->getPoint(Pos(15, i)).setType(Point::Type::WALL);  // horizontal #2
    }
   
    Pos from(6, 7), to(14, 13);
    snake.testMinPath(from, to, path);
    //snake.testMaxPath(from, to, path);

    // Print path info
    string info = "Path from " + from.toString() + " to " + to.toString()
        + " of length " + util::toString(path.size()) + ":\n";
    for (const Direction &d : path) {
        switch (d) {
            case LEFT:
                info += "L "; break;
            case UP:
                info += "U "; break;
            case RIGHT:
                info += "R "; break;
            case DOWN:
                info += "D "; break;
            case NONE:
            default:
                info += "NONE "; break;
        }
    }
    info += "\ntestSearch() finished.";
    exitGame(info);
}

void GameCtrl::testHamilton() {
    snake.setMap(map);
    snake.addBody(Pos(1, 3));
    snake.addBody(Pos(1, 2));
    snake.addBody(Pos(1, 1));
    snake.testHamilton();
    exitGame("testHamilton() finished.");
}
