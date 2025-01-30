#ifndef SHADER_H
#define SHADER_H
#include <GL/glew.h>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

static void CheckShaderCompileErrors(GLuint Shader)
{
	GLint Success;
	char aInfoLog[2048];

	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Success);
	if(!Success)
	{
		glGetShaderInfoLog(Shader, 1024, NULL, aInfoLog);
		fprintf(stderr, "ERROR: Shader Compilation Failed\n%s\n", aInfoLog);
		return;
	}
	// dont print this. too annoying
	// printf("Shader compilation successful!\n");
}

static void CheckProgramLinkErrors(GLuint Program)
{
	GLint Success;
	char aInfoLog[2048];

	glGetProgramiv(Program, GL_LINK_STATUS, &Success);
	if(!Success)
	{
		glGetProgramInfoLog(Program, 1024, NULL, aInfoLog);
		fprintf(stderr, "ERROR: Program Linking Failed\n%s\n", aInfoLog);
		return;
	}
	// dont print this. too annoying
	// printf("Shader program linking successful!\n");
}

class CShader
{
public:
	GLuint m_Program = -1;

	unsigned int VBO = -1, VAO = -1, EBO = -1;
	unsigned int m_NumIndices = 0;

	bool IsCompiled() { return m_Program != -1; }

	void CompileShader(const char *pVertex, const char *pFragment)
	{
		if(IsCompiled())
			Destroy();

		GLuint Vertex, Fragment;

		Vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(Vertex, 1, &pVertex, NULL);
		glCompileShader(Vertex);
		CheckShaderCompileErrors(Vertex);

		Fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(Fragment, 1, &pFragment, NULL);
		glCompileShader(Fragment);
		CheckShaderCompileErrors(Fragment);

		m_Program = glCreateProgram();
		glAttachShader(m_Program, Vertex);
		glAttachShader(m_Program, Fragment);
		glLinkProgram(m_Program);
		CheckProgramLinkErrors(m_Program);

		glDeleteShader(Fragment);
		glDeleteShader(Vertex);
	}

	void Use() { glUseProgram(m_Program); }

	void Destroy()
	{
		if(EBO != -1)
		{
			glDeleteBuffers(1, &EBO);
			EBO = -1;
		}
		if(VBO != -1)
		{
			glDeleteBuffers(1, &VBO);
			VBO = -1;
		}
		if(VAO != -1)
		{
			glDeleteVertexArrays(1, &VAO);
			VAO = -1;
		}

		glDeleteProgram(m_Program);
		m_Program = -1;
	}

	// Set a float uniform
	void SetFloat(const char *pName, float Value)
	{
		glUniform1f(glGetUniformLocation(m_Program, pName), Value);
	}

	// Set an integer uniform
	void SetInt(const char *pName, int Value)
	{
		glUniform1i(glGetUniformLocation(m_Program, pName), Value);
	}

	// Set a boolean uniform
	void SetBool(const char *pName, bool Value)
	{
		glUniform1i(glGetUniformLocation(m_Program, pName), (int)Value);
	}

	// Set a 2D vector uniform
	void SetVec2(const char *pName, const glm::vec2 &Value)
	{
		glUniform2fv(glGetUniformLocation(m_Program, pName), 1, glm::value_ptr(Value));
	}

	// Set a 3D vector uniform
	void SetVec3(const char *pName, const glm::vec3 &Value)
	{
		glUniform3fv(glGetUniformLocation(m_Program, pName), 1, glm::value_ptr(Value));
	}

	// Set a 4D vector uniform
	void SetVec4(const char *pName, const glm::vec4 &Value)
	{
		glUniform4fv(glGetUniformLocation(m_Program, pName), 1, glm::value_ptr(Value));
	}

	// Set a 2x2 matrix uniform
	void SetMat2(const char *pName, const glm::mat2 &Value)
	{
		glUniformMatrix2fv(glGetUniformLocation(m_Program, pName), 1, GL_FALSE, glm::value_ptr(Value));
	}

	// Set a 3x3 matrix uniform
	void SetMat3(const char *pName, const glm::mat3 &Value)
	{
		glUniformMatrix3fv(glGetUniformLocation(m_Program, pName), 1, GL_FALSE, glm::value_ptr(Value));
	}

	// Set a 4x4 matrix uniform
	void SetMat4(const char *pName, const glm::mat4 &Value)
	{
		glUniformMatrix4fv(glGetUniformLocation(m_Program, pName), 1, GL_FALSE, glm::value_ptr(Value));
	}
};

#endif // SHADER_H
