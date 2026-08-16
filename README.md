cmake -S . -B build
cmake --build build
cloc . --exclude-dir=build,.cache
