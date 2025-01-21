#include "body.h"
#include "../gfx/camera.h"
#include "../gfx/shader.h"

void SBody::Render(CShader *pShader, class CCamera *pCamera)
{
	pShader->Use();

	// set transformation matrices
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(m_Position.x / RENDER_SCALE, m_Position.y / RENDER_SCALE, m_Position.z / RENDER_SCALE));

	pShader->SetMat4("model", model);
	pShader->SetMat4("view", pCamera->m_View);
	pShader->SetMat4("projection", pCamera->m_Projection);

	// set light properties
	pShader->SetVec3("lightPos", glm::vec3(2.0f, 2.0f, 2.0f));
	pShader->SetVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
	pShader->SetVec3("objectColor", glm::vec3(0.8f, 0.3f, 0.2f));

	// bind the vao and draw the sphere
	glBindVertexArray(pShader->VAO);
	glDrawElements(GL_TRIANGLES, pShader->m_NumIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}
