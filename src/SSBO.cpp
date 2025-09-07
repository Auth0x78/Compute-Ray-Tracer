#include "SSBO.h"

SSBO::SSBO(GLuint bindingLoc,GLuint usage, GLsizeiptr size, const void* data)
{
	// Generate a SSBO
	glGenBuffers(1, &m_id);

	m_usage = usage;
	m_bindingLocation = bindingLoc;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id); // Bind it as a SSBO
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingLoc, m_id); // binding in shader
}

SSBO::~SSBO()
{
	glDeleteBuffers(1, &m_id);
}

void SSBO::Bind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id); // Bind it as a SSBO
}

void SSBO::BindBase()
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingLocation, m_id); // binding = 0 in shader
}

void SSBO::Unbind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SSBO::UnbindBase()
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingLocation, 0);
}

void SSBO::SetData(GLsizeiptr size, const void* data)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id); // Bind it as a SSBO
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, m_usage);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind it after use
}
