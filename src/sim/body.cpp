#include "body.h"
#include <string>

SBody::SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters) :
	m_Name(Name),
	m_Id(Id),
	m_SimParams(SimulationParameters),
	m_RenderParams(RenderParameters) {}
