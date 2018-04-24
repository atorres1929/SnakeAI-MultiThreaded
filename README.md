SnakeAI-Multithreaded

While a make file is included, it is not recommended to run this project
in Unix based systems. When compiling, it compiles with errors unless on
a windows machine.

To run this project in Windows:
-Open a new Visual Studio project
-Copy and paste the source of this project into a the VS Project
-Open the properties of the project (not the solution) and change the include directory to be this project's include.
--Do the same for the project's source folder
-Ensure that OpenMP is enabled for the project (also in the properties of the VS project)
-Run.