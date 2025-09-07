#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class SSBO {
public:
	SSBO(GLuint bindingLoc, GLuint usage, GLsizeiptr size, const void* data);
	~SSBO();

	void Bind();
	void BindBase();
	
	void Unbind();
	void UnbindBase();

	void SetData(GLsizeiptr size, const void* data);
	GLuint GetID() const { return m_id; }
	GLenum GetUsage() const { return m_usage; }
private:
	GLuint m_id;
	GLuint m_bindingLocation;
	GLenum m_usage;
};