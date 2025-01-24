# AstroSim
AstroSim is a small pet project that aims to simulate our solar system using OpenGL for rendering and ImGui for the UI.

Building on Linux
---------------------------
```
git clone --recursive https://github.com/Teero888/astrosim.git && cd astrosim
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./astrosim
```
Building on Windows
---------------------------
No idea figure it out

Screenshots
--------------------------
[![Jupiter](https://i.postimg.cc/WbBBc01P/Jupiter.png)](https://postimg.cc/gnKSvXhM)
[![Sun](https://i.postimg.cc/1zPbfPFC/Sun.png)](https://postimg.cc/WdW5HBXg)
