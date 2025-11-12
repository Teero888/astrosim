#ifndef MARCHINGCUBES_H
#define MARCHINGCUBES_H

// see: https://paulbourke.net/geometry/polygonise/
struct MarchingCubesData
{
	static const int ms_EdgeTable[256];
	static const int ms_TriTable[256][16];
};

#endif // MARCHINGCUBES_H