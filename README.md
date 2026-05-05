OpenCV (to the project) — https://github.com/huihut/OpenCV-MinGW-Build/archive/refs/tags/OpenCV-4.5.5-x64.zip

copy it .dll — cp F:/vscode_proj/The-coffee-courier-robot/opencv/OpenCV-MinGW-Build-OpenCV-4.5.5-x64/OpenCV-MinGW-Build-OpenCV-4.5.5-x64/x64/mingw/bin/*.dll F:/vscode_proj/The-coffee-courier-robot/

запуск CMakeFile:
```bash
cd bin
cmake .. -G "MinGW Makefiles"
mingw32-make
```
_один раз_
после только 
```bash
cd bin
mingw32-make
```