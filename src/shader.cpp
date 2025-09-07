#include "shader.h"


// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char* filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (in.is_open())
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

std::string get_directory_from_path(const char* filepath)
{
	std::string pathStr(filepath);
	size_t lastSlash = pathStr.find_last_of("/\\");
	if (lastSlash == std::string::npos)
		return "";

	return pathStr.substr(0, lastSlash + 1);
}

// Constructor that build the Shader Program from 2 different shaders
Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	shaderType = GL_VERTEX_SHADER; // When shaderType is Vertex then Fragment is also assumed
	compileShader(vertexFile, fragmentFile);
}

// Constructor that builds the Compute Shader Program
Shader::Shader(const char* shaderFile, GLenum type)
{
	shaderType = type;

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();

	std::string code = get_file_contents(shaderFile);

	// Parse include directive of each shader
	parsePreprocessorDirectives(code, get_directory_from_path(shaderFile));
	
	// Convert the shader source strings into character arrays
	const char* sourceCode = code.c_str();
	
	// Create Vertex Shader Object and get its reference
	GLuint shaderT = glCreateShader(type);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(shaderT, 1, &sourceCode, NULL);

	// Compile the Vertex Shader into machine code
	glCompileShader(shaderT);
	// Checks if Shader compiled successfully
	switch (type) {
		case GL_COMPUTE_SHADER:
			compileErrors(shaderT, "COMPUTE");
			break;
		default:
			std::cerr << "Unsupported shader type!" << std::endl;
			throw(errno);
	}

	// Attach the Vertex and Fragment Shaders to the Shader Program
	glAttachShader(ID, shaderT);
	// Wrap-up/Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// Checks if Shaders linked successfully
	compileErrors(ID, "PROGRAM");

	// Delete the now useless Vertex and Fragment Shader objects
	glDeleteShader(shaderT);
}

// Activates the Shader Program
void Shader::Activate()
{
	glUseProgram(ID);
}

// Deactivates the Shader Program
void Shader::Deactivate()
{
	glUseProgram(0);
}

// Deletes the Shader Program
void Shader::Delete()
{
	glDeleteProgram(ID);
}

void Shader::Reload(const char* vertexFile, const char* fragmentFile)
{
	glDeleteProgram(ID);
	compileShader(vertexFile, fragmentFile);
}

void Shader::SetUniform1i(const std::string& name, int32_t i)
{
	glUniform1i(GetUniformLocation(name), i);
}

void Shader::SetUniform1f(const std::string& name, float f)
{
	glUniform1f(GetUniformLocation(name), f);
}

void Shader::SetUniform2fv(const std::string& name, const glm::vec2& vec)
{
	glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(vec));
}

void Shader::SetUniform3fv(const std::string& name, const glm::vec3& vec)
{
	glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(vec));
}

void Shader::SetUniformMatrix4fv(const std::string& name, const glm::mat4& mat4)
{
	glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat4));
}

int Shader::GetUniformLocation(const std::string& name)
{
	if (uniformLocationCache.find(name) != uniformLocationCache.end())
		return uniformLocationCache[name];

	int loc = glGetUniformLocation(ID, name.c_str());
	uniformLocationCache[name] = loc;
	return loc;
}

GLenum Shader::GetShaderType()
{
	return shaderType;
}

// Checks if the different Shaders have compiled properly
void Shader::compileErrors(unsigned int shader, const char* type)
{
	// Stores status of compilation
	GLint hasCompiled;
	// Character array to store error message in
	char infoLog[1024];
	if (strcmp(type, "PROGRAM") != 0)
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
}

void Shader::compileShader(const char* vertexFile, const char* fragmentFile)
{
	// Create Shader Program Object and get its reference
	ID = glCreateProgram();

	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);

	// Parse include directive of each shader
	parsePreprocessorDirectives(vertexCode, get_directory_from_path(vertexFile));
	parsePreprocessorDirectives(fragmentCode, get_directory_from_path(fragmentFile));

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	
	const char* fragmentSource = fragmentCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(vertexShader);
	// Checks if Shader compiled successfully
	compileErrors(vertexShader, "VERTEX");

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	// Attach Fragment Shader source to the Fragment Shader Object
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(fragmentShader);
	// Checks if Shader compiled successfully
	compileErrors(fragmentShader, "FRAGMENT");

	// Attach the Vertex and Fragment Shaders to the Shader Program
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	// Wrap-up/Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// Checks if Shaders linked successfully
	compileErrors(ID, "PROGRAM");

	// Delete the now useless Vertex and Fragment Shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::parsePreprocessorDirectives(std::string& source, const std::string& parentDir)
{
	int len = source.length();
	for (int i = 0; i < len; ++i)
	{
		int lineEnd = i;
		while(lineEnd < len && source[lineEnd] != '\n')
			++lineEnd;
		
		// Does not start with
		if (source[i] != '#') 
		{
			i = lineEnd;
			continue;
		}
		
		std::string directive = source.substr(i+1, lineEnd - i);
		if (directive.find("pragma include") != std::string::npos)
		{
			for (int j = 0; j < directive.length(); ++j)
			{
				if (directive[j] == '\"')
				{
					int start = j + 1;
					int end = directive.find('\"', start);
					if (end != std::string::npos)
					{
						std::string includeFile = parentDir + directive.substr(start, end - start);
						std::string includeCode = get_file_contents(includeFile.c_str());

						// TODO: Remove this debug statement
						std::cout << "Including file: " << includeFile << std::endl;

						// exclude the #version directive on first line
						if (includeCode._Starts_with("#version")) {
							int includeLineEnd = includeCode.find('\n');
							if (includeLineEnd != std::string::npos)
								includeCode = includeCode.substr(includeLineEnd + 1);
							else 
								includeCode = "";
						}

						// Recursively Parse Each included file
						parsePreprocessorDirectives(includeCode, get_directory_from_path(includeFile.c_str()));

						source.replace(i, lineEnd - i, includeCode);
						len = source.length();
						lineEnd = i + includeCode.length();
					}
					break;
				}
			}
		}
		
		i = lineEnd;
	}
}
