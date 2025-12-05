# Project Altina

A toy project.

## Running Tests

Tests live under `Source/Tests` and are registered with CTest. To build
and run the tests locally (out-of-source build) using PowerShell:

```powershell
mkdir build; cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --config RelWithDebInfo
ctest -C RelWithDebInfo --output-on-failure
```

You can also run the test executable directly; it will be located in the
build runtime folder at `.../Source/Tests/AltinaEngineTests(.exe)` on Windows.
