#include "RenderObject.hpp"

#define GLM_FORCE_RADIANS

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <vector>

#define BUFFER_OFFSET(i) ((char *)nullptr + (i))

renderObject::renderObject()
{
	modelMatrix = glm::mat4(1.0);

	generated = false;

	mapWidth = 1024;
	mapHeight = 1024;
	mapSize = mapWidth * mapHeight;
	quadSize = 4;
	g_HeightMap = new unsigned char[mapSize];

	gridWidth = mapWidth / quadSize;
	gridHeight = mapHeight / quadSize;

	vertices = new VertexPosition[gridHeight * gridWidth];	// Allocate memory for the individual vertices of the terrain
	rgbColor = 1.0f;
}

void renderObject::genBuffer(GLuint shader)
{

	glGenBuffers(1, &VBOHeightMap);
	glBindBuffer(GL_ARRAY_BUFFER, VBOHeightMap);

	int vIndex = 0;
	for (int _w = 0; _w < mapWidth; _w += quadSize)
	{
		for (int _h = 0; _h < mapHeight; _h += quadSize)
		{
			rgbColor = setVertexColor(_w, _h);
			vertices[vIndex++] = VertexPosition{ _w, (getHeight(_w, _h)), _h, rgbColor, rgbColor, rgbColor };
		}
	}
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPosition)* vIndex, &vertices[0], GL_STATIC_DRAW);

	/*glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	
	struct IndexTriangle
	{
		GLuint v0, v1, v2;
	};

	std::vector<IndexTriangle> indexHolder;
	for (int _w = 0; _w < (gridWidth - 1); ++_w)
	{
		for (int _h = 0; _h < (gridHeight - 1); ++_h)
		{
			GLuint vertexIndex = (_w * gridWidth) + _h;

			IndexTriangle top;
			top.v0 = vertexIndex;
			top.v1 = vertexIndex + gridWidth + 1;
			top.v2 = vertexIndex + 1;

			IndexTriangle bottom;
			bottom.v0 = top.v0;
			bottom.v1 = vertexIndex + gridWidth;
			bottom.v2 = top.v1;

			indexHolder.push_back(top);
			indexHolder.push_back(bottom);
		}
	}
	nrIndex = indexHolder.size();
	
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(IndexTriangle)* indexHolder.size(), &indexHolder[0], GL_STATIC_DRAW);

	*/

	quadTree = createQuadTree(2, 0, 0, mapWidth, mapHeight);

	glGenVertexArrays(1, &VAOHeightMap);
	glBindVertexArray(VAOHeightMap);

	glEnableVertexAttribArray(0); //the vertex attribute object will remember its enabled attributes
	glEnableVertexAttribArray(1);


	/// this should be moved out from this class
	/// as it is bound to shader, and can be used across multiple objects
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPosition), BUFFER_OFFSET(0));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPosition), BUFFER_OFFSET(sizeof(float) * 3));
	/// 

	generated = true;
}

void renderObject::update()
{

	glm::mat4 rotMatrix = glm::mat4(
		cos((glm::pi<float>() / 180)*ry), 0.0f, -sin((glm::pi<float>() / 180)*ry), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		sin((glm::pi<float>() / 180)*ry), 0.0f, cos((glm::pi<float>() / 180)*ry), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	modelMatrix = glm::translate(glm::mat4(1.0), glm::vec3(0, 0, -2.0));
	modelMatrix = modelMatrix * rotMatrix;

	ry += 1.0f;

}

const GLfloat * renderObject::getModelMatrix() const
{
	return &modelMatrix[0][0];
}

renderObject::~renderObject()
{
	delete g_HeightMap;
	delete vertices;

	releaseQuadTree(quadTree);
}

void renderObject::render()
{

	glBindBuffer(GL_ARRAY_BUFFER, VBOHeightMap);
	glBindVertexArray(VAOHeightMap);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

	// draw points 0-3 from the currently bound VAO with current in-use shader
	//glDrawElements(GL_TRIANGLES, 12 * nrIndex, GL_UNSIGNED_INT, (void*)0);
	renderQuadTree(quadTree);
}

void renderObject::renderQuadTree(QuadTree* qt)
{
	if (qt->botLeft)
	{
		renderQuadTree(qt->botLeft);
		renderQuadTree(qt->botRight);
		renderQuadTree(qt->topLeft);
		renderQuadTree(qt->topRight);
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, qt->q_IndexBuffer);
		// draw points 0-3 from the currently bound VAO with current in-use shader
		glDrawElements(GL_TRIANGLES, 12 * qt->nrIndex, GL_UNSIGNED_INT, (void*)0);
	}
}

bool renderObject::loadRawFile(char* fileName)
{
	bool loadFromFile = false;
	FILE* file = nullptr;

	// Opens the file in Read/Binary mode.
	file = fopen(fileName, "rb");

	// Check if file was found and could open it
	if (file != nullptr)
	{
		// Read the .raw file into data array.
		fread(g_HeightMap, 1, mapSize, file);

		// Check for data error
		int result = ferror(file);
		result = errno;
		if (result)
		{
			// Reading failed - check errno.h
			// https://msdn.microsoft.com/en-us/library/t3ayayh1.aspx
			loadFromFile = false;
		}
		else
		{
			loadFromFile = true;
		}

	}
	else
	{
		// Height map not found
		loadFromFile = false;
	}
	fclose(file);
	return loadFromFile;
}

int renderObject::getHeight(int _x, int _y)
{
	// Force x and y to cap at (mapSize - 1)
	int x = _x % mapWidth;
	int y = _y % mapHeight;

	// Check if empty
	if (!g_HeightMap)
	{
		// TODO: printf to console
		return 0;
	}

	// Treat the array like a 2D array (.raw format is a single array)
	return g_HeightMap[x + (y * mapHeight)];	// Index into our height array and return the height
}
float renderObject::setVertexColor(int x, int y)
{
	float color = -0.15f + (getHeight(x, y) / 256.0f);
	return color;
}

QuadTree* renderObject::createQuadTree(int levels, GLfloat startX, GLfloat startY, GLfloat endX, GLfloat endY)
{
	QuadTree* root = new QuadTree();

	GLfloat x = ((endX - startX) / 2) + startX;
	GLfloat y = ((endY - startY) / 2) + startY;
	GLfloat size = abs((endX - startX) / 2);

	root->x = x;
	root->y = y;
	root->size = size;

	root->q_IndexBuffer = 0;
	if (levels != 0)
	{

		QuadTree* topLeft = createQuadTree(levels - 1, x - size, y, x, y - size);
		QuadTree* topRight = createQuadTree(levels - 1, x, y - size, x + size, y);
		QuadTree* botLeft = createQuadTree(levels - 1, x - size, y, x, y + size);
		QuadTree* botRight = createQuadTree(levels - 1, x, y, x + size, y + size);

		root->topLeft = topLeft;
		root->topRight = topRight;
		root->botLeft = botLeft;
		root->botRight = botRight;
	}
	else
	{
		root->topLeft = nullptr;
		root->topRight = nullptr;
		root->botLeft = nullptr;
		root->botRight = nullptr;

		glGenBuffers(1, &root->q_IndexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, root->q_IndexBuffer);

		struct IndexTriangle
		{
			GLuint v0, v1, v2;
		};

		std::vector<IndexTriangle> indexHolder;
		
		/*for (int _w = (x - size) / quadSize; _w < ((x + size) / quadSize) - 1 ; ++_w)
		{
			for (int _h = (y - size) / quadSize; _h < ((y + size) / quadSize) -1; ++_h)
			{
				//GLuint vertexIndex = (_w * gridWidth) + _h;

				IndexTriangle top;
				top.v0 = _h + (gridWidth * _w);
				top.v1 = (_h+1) + (gridWidth *( _w + 1));
				top.v2 = (_h+1) + (gridWidth * _w);

				IndexTriangle bottom;
				bottom.v0 = top.v0;
				bottom.v1 = _h + (gridWidth * (_w + 1));
				bottom.v2 = top.v1;

				indexHolder.push_back(top);
				indexHolder.push_back(bottom);
			}
		}*/

		int countSize = (size * 2) / quadSize;

		int xOffset = abs(x - size) / quadSize;
		int yOffset = abs(y - size) / quadSize;

		for (int _w = 0; _w < (countSize - 1); _w++)
		{
			for (int _h = 0; _h < (countSize - 1); _h++)
			{
				GLuint vertexIndex = ((_w + xOffset) * gridWidth) + (_h + yOffset);

				IndexTriangle top;
				top.v0 = vertexIndex;
				top.v1 = vertexIndex + gridWidth + 1;
				top.v2 = vertexIndex + 1;

				IndexTriangle bottom;
				bottom.v0 = top.v0;
				bottom.v1 = vertexIndex + gridWidth;
				bottom.v2 = top.v1;

				indexHolder.push_back(top);
				indexHolder.push_back(bottom);
			}
		}

		root->nrIndex = indexHolder.size();

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(IndexTriangle)* root->nrIndex, &indexHolder[0], GL_STATIC_DRAW);

	}
	return root;
}

void renderObject::releaseQuadTree(QuadTree* qt)
{
	if (qt->botLeft)
	{
		releaseQuadTree(qt->botLeft);
		releaseQuadTree(qt->botRight);
		releaseQuadTree(qt->topLeft);
		releaseQuadTree(qt->topRight);
	}
	delete qt;
}