# sdl3-migration-clang-tidy-plugin
This is a clang-based tool that checks for SDL2 usage and suggests changes to SDL3 usage.

To run the tests and iterate on it from the build folder run:

`make && python3 ../tests/run_tests.py`

The test runner basically checks that the test_<plugin>_before.cpp file transforms through clang-tidy into the test_<plugin>_after.cpp file. On failure it writes a .diff file between the expected file and the transformed file.

To check the fixes for a specific file:

`make && clang-tidy --load=./SDL3MigrationCheck.so --checks='-*,sdl3
-migration-<plugin>' --fix  ../tests/test_<plugin>_before.cpp`

Make sure to revert the changes in your editor after running the command above.

