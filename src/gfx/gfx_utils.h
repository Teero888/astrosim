#ifndef GFX_UTILS_H
#define GFX_UTILS_H

#include <glm/glm.hpp>

inline glm::vec2 WorldToScreenCoordinates(glm::vec3 WorldPos, glm::mat4 Model, glm::mat4 View, glm::mat4 Projection, int ScreenWidth, int ScreenHeight)
{
	glm::mat4 mvp = Projection * View * Model;
	glm::vec4 clipSpacePos = mvp * glm::vec4(WorldPos, 1.0f);
	if(clipSpacePos.w < 0.f)
		return glm::vec2(-1.f);
	glm::vec3 ndcPos = glm::vec3(clipSpacePos) / clipSpacePos.w;
	glm::vec2 screenPos;
	screenPos.x = (ndcPos.x + 1.0f) * 0.5f * ScreenWidth;
	screenPos.y = (1.0f - ndcPos.y) * 0.5f * ScreenHeight;
	return screenPos;
}

#endif // GFX_UTILS_H
