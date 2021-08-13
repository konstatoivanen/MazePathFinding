#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <GLAD/glad.h>
#if _WIN32
#include <Windows.h>
#endif 

const char* shader_vertex =
" #version 450 core														\n"
" 																		\n"
" out vec2 vs_TEXCOORD0;												\n"
" 																		\n"
" void main()															\n"
" {																		\n"
"     vs_TEXCOORD0 = vec2( (gl_VertexID << 1) & 2, gl_VertexID & 2 );	\n"
"     gl_Position = vec4(vs_TEXCOORD0 * 2.0 - 1.0, 0.0, 1.0 );			\n"
" }																		\n";

const char* shader_fragment =
"	#version 450 core															            \n"
"																				            \n"
"	uniform sampler2D pk_Maze;												                \n"
"	uniform sampler2D pk_Path;												                \n"
"																				            \n"
"	in vec2 vs_TEXCOORD0;														            \n"
"	out vec4 SV_Target0;														            \n"
"																				            \n"
"	void main()																	            \n"
"	{																			            \n"
"       vec4 offset = vec4(0.25f, 0.25f, -0.25f, -0.25f) / textureSize(pk_Maze, 0).xyxy;    \n"
"                                                                                           \n"
"       float border = 0.0f;                                                                \n"
"       border += texture(pk_Maze, vs_TEXCOORD0 + offset.xy).r;                             \n"
"       border += texture(pk_Maze, vs_TEXCOORD0 + offset.zw).r;                             \n"
"       border += texture(pk_Maze, vs_TEXCOORD0 + offset.xw).r;                             \n"
"       border += texture(pk_Maze, vs_TEXCOORD0 + offset.zy).r;                             \n"
"       border = smoothstep(border, 0.9f, 0.5f);                                            \n"
"       border *= border;                                                                   \n"
"       border *= 0.75f;                                                                    \n"
"                                                                                           \n"
"       float path = step(0.5f, texture(pk_Path, vs_TEXCOORD0).r);                          \n"
"                                                                                           \n"
"       SV_Target0 = vec4(mix(border.xxx, vec3(1,0,0), path), 1.0f);                        \n"
"   }																			            \n";

const static char CELL_VISITED = 1 << 5;
const int OFFSETSX[4] = { 1,  0, -1, 0 };
const int OFFSETSY[4] = { 0, -1,  0, 1 };

struct ShaderSource
{
	GLenum type;
	const char* source;
};

struct PNode
{
	float distanceLocal;
	float distanceGlobal;
	uint32_t flags;
	uint32_t parentIndex;
};

static GLuint CompileShader(std::initializer_list<ShaderSource> sources)
{
	auto program = glCreateProgram();

	auto stageCount = sources.size();

	GLenum* glShaderIDs = reinterpret_cast<GLenum*>(alloca(sizeof(GLenum) * stageCount));

	int glShaderIDIndex = 0;

	for (auto& kv : sources)
	{
		auto type = kv.type;
		const auto& source = kv.source;

		auto glShader = glCreateShader(type);

		glShaderSource(glShader, 1, &source, 0);
		glCompileShader(glShader);

		GLint isCompiled = 0;
		glGetShaderiv(glShader, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(glShader, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(glShader, maxLength, &maxLength, &infoLog[0]);

			glDeleteShader(glShader);

			printf(infoLog.data());
			glDeleteShader(glShader);
			continue;
		}

		glAttachShader(program, glShader);
		glShaderIDs[glShaderIDIndex++] = glShader;
	}

	glLinkProgram(program);

	GLint isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);

	if (isLinked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

		glDeleteProgram(program);

		for (auto i = 0; i < stageCount; ++i)
		{
			glDeleteShader(glShaderIDs[i]);
		}

		printf("Shader link failure! \n%s", infoLog.data());
		glDeleteProgram(program);
		return 0;
	}

	for (auto i = 0; i < stageCount; ++i)
	{
		glDetachShader(program, glShaderIDs[i]);
		glDeleteShader(glShaderIDs[i]);
	}

	return program;
}

static void GenerateMaze(uint32_t w, uint32_t h, uint32_t pw, char* texture, PNode* nodes)
{
    auto tw = w * pw;
    auto th = h * pw;

    auto maze = reinterpret_cast<char*>(calloc(w * h, sizeof(char)));
    auto stack = reinterpret_cast<uint32_t*>(malloc(w * h * sizeof(uint32_t)));

    auto offs = pw - 1u;
    auto stackSize = 1u;
    auto ncells = w * h - 1u;
    maze[0] = CELL_VISITED;
    stack[0] = 0u;

    while (ncells > 0)
    {
        uint32_t i = stack[stackSize - 1];
        uint32_t n = 0u;
        uint32_t cx = i % w;
        uint32_t cy = i / w;

        auto dir = 0xFF;

        for (uint32_t j = rand(), k = j + 4; j < k; ++j)
        {
            int nx = cx + OFFSETSX[j % 4];
            int ny = cy + OFFSETSY[j % 4];
            n = nx + ny * w;

            if (nx >= 0 && nx < w && ny >= 0 && ny < h && (maze[n] & CELL_VISITED) == 0)
            {
                dir = j % 4;
                break;
            }
        }

        if (dir == 0xFF)
        {
            stackSize--;
            continue;
        }

        maze[n] |= CELL_VISITED | (1 << ((dir + 2) % 4));
        maze[i] |= (1 << dir);

        stack[stackSize++] = n;
        ncells--;
    }

    for (auto x = 0u; x < w; ++x)
    for (auto y = 0u; y < h; ++y)
    {
        auto cell = maze[x + y * w];
        nodes[x + y * w].flags = cell;

        auto tx = x * pw;
        auto ty = y * pw;

        if ((cell & (1 << 3)) == 0 && tx > 0)
        {
            texture[tx - 1 + (ty + offs) * tw] = 255;
        }

        for (auto i = 0; i < pw; ++i)
        {
            if ((cell & (1 << 0)) == 0)
            {
                texture[tx + offs + (ty + i) * tw] = 255;
            }

            if ((cell & (1 << 3)) == 0)
            {
                texture[tx + i + (ty + offs) * tw] = 255;
            }
        }
    }

    free(maze);
    free(stack);
}

static void QuickSortNodes(PNode* nodes, uint32_t* open, int low, int high)
{
	int i = low;
	int j = high;
	auto pivot = open[(i + j) / 2];

	while (i <= j)
	{
		while (nodes[open[i]].distanceGlobal > nodes[pivot].distanceGlobal)
		{
			i++;
		}

		while (nodes[open[j]].distanceGlobal < nodes[pivot].distanceGlobal)
		{
			j--;
		}

		if (i <= j)
		{
			auto temp = open[i];
			open[i] = open[j];
			open[j] = temp;
			i++;
			j--;
		}
	}

	if (j > low)
	{
		QuickSortNodes(nodes, open, low, j);
	}

	if (i < high)
	{
		QuickSortNodes(nodes, open, i, high);
	}
}

static bool GeneratePath(std::vector<uint32_t>& openset, PNode* nodes, uint32_t w, uint32_t h, uint32_t pw, uint32_t starti, uint32_t endi, char* outTexture)
{
    auto tw = w * pw;
    auto th = h * pw;

    memset(outTexture, 0, tw * th * sizeof(char));

    uint32_t count = w * h;

    for (auto i = 0; i < count; ++i)
    {
        nodes[i].parentIndex = 0xFFFFFFFF;
        nodes[i].distanceGlobal = 0xFFFFFFFF;
        nodes[i].distanceLocal = 0xFFFFFFFF;
        nodes[i].flags &= ~CELL_VISITED;
    }

    nodes[starti].distanceLocal = 0.0f;

    auto deltax = (int)(starti % w) - (int)(endi % w);
    auto deltay = (int)(starti / w) - (int)(endi / w);
    nodes[starti].distanceGlobal = sqrtf(deltax * deltax + deltay * deltay);

    openset.clear();
    openset.push_back(starti);

    while (openset.size() > 0)
    {
        QuickSortNodes(nodes, openset.data(), 0, openset.size() - 1);

        auto current = openset.back();
        openset.pop_back();

        if (current == endi)
        {
            break;
        }

        nodes[current].flags |= CELL_VISITED;

        float distance = nodes[current].distanceLocal + 1;

        auto x = current % w;
        auto y = current / w;

        for (auto dir = 0; dir < 4; ++dir)
        {
            if ((nodes[current].flags & (1 << dir)) == 0)
            {
                continue;
            }

            auto nx = x + OFFSETSX[dir];
            auto ny = y + OFFSETSY[dir];
            auto neighbour = nx + ny * w;

            if (distance >= nodes[neighbour].distanceLocal)
            {
                continue;
            }

            if ((nodes[neighbour].flags & CELL_VISITED) == 0)
            {
                openset.push_back(neighbour);
            }

            nodes[neighbour].parentIndex = current;
            nodes[neighbour].distanceLocal = distance;

            auto deltax = (int)x - (int)nx;
            auto deltay = (int)y - (int)ny;
            nodes[neighbour].distanceGlobal = distance + sqrtf(deltax * deltax + deltay * deltay);
        }
    }

    auto trace = endi;

    if (nodes[trace].parentIndex == 0xFFFFFFFF)
    {
        return false;
    }

    auto prevx = (trace % w) * pw + 2;
    auto prevy = (trace / w) * pw + 2;

    while (trace != 0xFFFFFFFF)
    {
        auto x = (trace % w) * pw + 2;
        auto y = (trace / w) * pw + 2;

        auto dx = ((int)x - (int)prevx) < 0 ? -1 : 1;
        auto dy = ((int)y - (int)prevy) < 0 ? -1 : 1;

        for (int px = prevx; px != x; px += dx)
        {
            outTexture[px + y * tw] = 255;
        }

        for (auto py = prevy; py != y; py += dy)
        {
            outTexture[x + py * tw] = 255;
        }

        prevx = x;
        prevy = y;

        trace = nodes[trace].parentIndex;
    }

    return true;
}

int main(int argc, char* argv[])
{
	GLFWwindow* window = nullptr;

    #if _WIN32
        ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
    #endif

    if (!glfwInit())
	{

		printf("Failed to initialize glfw!");
		return 0;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow((int)1024, (int)512, "Maze Path Finding", nullptr, nullptr);

	if (!window)
	{
		printf("Failed to create a window!");
		glfwTerminate();
		return 0;
	}

	glfwMakeContextCurrent(window);

	int gladstatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	if (gladstatus == 0)
	{
		printf("Failed to load glad!");
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;
	}

    auto graphWidth = 64;
    auto graphHeight = 32;
    auto graphPadding = 6;
    auto texWidth = graphWidth * graphPadding;
    auto texHeight = graphHeight * graphPadding;
    
    std::vector<uint32_t> openset;
    uint32_t cellIndexStart = 0u;
    uint32_t cellIndexEnd = 0u;

    auto nodes = reinterpret_cast<PNode*>(calloc(texWidth * texHeight, sizeof(PNode)));
    auto mazeTexture = reinterpret_cast<char*>(calloc(texWidth * texHeight, sizeof(char)));
    auto pathTexture = reinterpret_cast<char*>(calloc(texWidth * texHeight, sizeof(char)));

    GenerateMaze(graphWidth, graphHeight, graphPadding, mazeTexture, nodes);

	GLuint textureIds[2];
	glCreateTextures(GL_TEXTURE_2D, 2, textureIds);
	glTextureStorage2D(textureIds[0], 1, GL_R8, texWidth, texHeight);
	glTextureStorage2D(textureIds[1], 1, GL_R8, texWidth, texHeight);

    glTextureSubImage2D(textureIds[0], 0, 0, 0, texWidth, texHeight, GL_RED, GL_UNSIGNED_BYTE, mazeTexture);
    free(mazeTexture);

    auto shaderDisplay = CompileShader({ {GL_VERTEX_SHADER, shader_vertex}, {GL_FRAGMENT_SHADER, shader_fragment} });

	glUseProgram(shaderDisplay);
	glUniform1i(glGetUniformLocation(shaderDisplay, "pk_Maze"), 0);
	glUniform1i(glGetUniformLocation(shaderDisplay, "pk_Path"), 1);
	glBindTextureUnit(0, textureIds[0]);
	glBindTextureUnit(1, textureIds[1]);

	GLuint vertexArrayObject;
	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        ypos /= height;
        xpos /= width;

        auto coordx = (uint32_t)(xpos * graphWidth);
        auto coordy = (uint32_t)((1.0f - ypos) * graphHeight);
        
        if (coordx >= graphWidth)
        {
            coordx = graphWidth - 1;
        }

        if (coordy >= graphHeight)
        {
            coordy = graphHeight - 1;
        }

        auto cellidx = coordx + coordy * graphWidth;

        if (glfwGetMouseButton(window, 0))
        {
            cellIndexStart = cellidx;
        }

        if (glfwGetMouseButton(window, 1))
        {
            cellIndexEnd = cellidx;
        }

        if (GeneratePath(openset, nodes, graphWidth, graphHeight, graphPadding, cellIndexStart, cellIndexEnd, pathTexture))
        {
            glTextureSubImage2D(textureIds[1], 0, 0, 0, texWidth, texHeight, GL_RED, GL_UNSIGNED_BYTE, pathTexture);
        }

        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
    }

    free(nodes);
    free(pathTexture);

    glDeleteVertexArrays(1, &vertexArrayObject);
	glDeleteTextures(2, textureIds);
	glDeleteProgram(shaderDisplay);
	glfwDestroyWindow(window);
	glfwTerminate();
    
}
