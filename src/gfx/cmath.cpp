
// This is for later
#if 0
#include "cmath.h"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include <cmath>


// returns rgb 0..1
glm::vec3 wavelength_to_rgb(int wavelength)
{
	glm::vec3 rgb{0, 0, 0};
	if(wavelength < 380 || wavelength >= 750)
	{
		return rgb;
	}

	const double gamma = 0.8;
	double r = 0.0;
	double g = 0.0;
	double b = 0.0;
	double factor;

	// Calculate color components based on wavelength ranges
	if(wavelength >= 380 && wavelength < 440)
	{
		r = (440.0 - wavelength) / (440.0 - 380.0);
		b = 1.0;
	}
	else if(wavelength >= 440 && wavelength < 490)
	{
		g = (wavelength - 440.0) / (490.0 - 440.0);
		b = 1.0;
	}
	else if(wavelength >= 490 && wavelength < 510)
	{
		g = 1.0;
		b = (510.0 - wavelength) / (510.0 - 490.0);
	}
	else if(wavelength >= 510 && wavelength < 580)
	{
		r = (wavelength - 510.0) / (580.0 - 510.0);
		g = 1.0;
	}
	else if(wavelength >= 580 && wavelength < 645)
	{
		r = 1.0;
		g = (645.0 - wavelength) / (645.0 - 580.0);
	}
	else if(wavelength >= 645 && wavelength < 750)
	{
		r = 1.0;
	}

	// Adjust intensity near vision limits
	if(wavelength >= 380 && wavelength < 420)
	{
		factor = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
	}
	else if(wavelength >= 420 && wavelength < 700)
	{
		factor = 1.0;
	}
	else if(wavelength >= 700 && wavelength < 750)
	{
		factor = 0.3 + 0.7 * (750.0 - wavelength) / (750.0 - 700.0);
	}
	else
	{
		factor = 0.0;
	}

	// Apply intensity factor
	r *= factor;
	g *= factor;
	b *= factor;

	// Gamma correction
	r = std::pow(r, gamma);
	g = std::pow(g, gamma);
	b = std::pow(b, gamma);

	return {r, g, b};
}
#endif