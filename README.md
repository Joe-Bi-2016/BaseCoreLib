# Base core Libarary
Support window, linux, macos and android crossplatform base c/c++ feature kit libarary, it contain file operation, ,mutex, condition variable, semaphore, thread pool, circular queue, looper that similar with android and support c++11 and posix standard.
spdlog library for log.
 
Note: If want generate library, should change basecore/CMakeLists.txt's add_executable to target_link_libraries.

window: 
use Cmake-GUI to generate vs project and build it.

linux/android/macos:
1. enter BaseCore directory, mkdir build
2. cmake & make