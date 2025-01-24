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
[![Jupiter.png](https://i.postimg.cc/Y0zZksnG/Jupiter.png)](https://postimg.cc/LghN3NM2)
[![Sun.png](https://i.postimg.cc/CKR3DJ1X/Sun.png)](https://postimg.cc/67NbxrCf)
