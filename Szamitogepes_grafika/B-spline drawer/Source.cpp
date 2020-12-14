#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

#include <array>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <math.h>

#include <iostream>
#include <fstream>

using namespace std;

static int WIN_WIDTH = 1500;
static int WIN_HEIGHT = 900;

GLint dragged = -1;
GLint hovered = -1;

std::vector<glm::vec3> pointToDraw;

glm::vec3 purple = glm::vec3(0.3f, 0.33f, 1.0f);

static std::vector<glm::vec3> myPoints;

int nurbs_n;
int nurbs_p = 3;
double maxX = 0, maxY = 0;
const int knot_len = 1000;
int* knots = new int[knot_len];
/* Vertex buffer objektum és vertex array objektum az adattároláshoz.*/
GLuint VBO, kontrollPontokVBO;
GLuint VAO, kontrollPontokVAO;

GLuint renderingProgram;

bool checkOpenGLError() {
	bool foundError = false;
	int glErr = glGetError();
	while (glErr != GL_NO_ERROR) {
		cout << "glError: " << glErr << endl;
		foundError = true;
		glErr = glGetError();
	}
	return foundError;
}

void printShaderLog(GLuint shader) {
	int len = 0;
	int chWrittn = 0;
	char* log;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
	if (len > 0) {
		log = (char*)malloc(len);
		glGetShaderInfoLog(shader, len, &chWrittn, log);
		cout << "Shader Info Log: " << log << endl;
		free(log);
	}
}

void printProgramLog(int prog) {
	int len = 0;
	int chWrittn = 0;
	char* log;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
	if (len > 0) {
		log = (char*)malloc(len);
		glGetProgramInfoLog(prog, len, &chWrittn, log);
		cout << "Program Info Log: " << log << endl;
		free(log);
	}
}

string readShaderSource(const char* filePath) {
	string content;
	ifstream fileStream(filePath, ios::in);
	string line = "";

	while (!fileStream.eof()) {
		getline(fileStream, line);
		content.append(line + "\n");
	}
	fileStream.close();
	return content;
}

GLuint createShaderProgram() {

	GLint vertCompiled;
	GLint fragCompiled;
	GLint linked;

	string vertShaderStr = readShaderSource("vertexShader.glsl");
	string fragShaderStr = readShaderSource("fragmentShader.glsl");

	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

	const char* vertShaderSrc = vertShaderStr.c_str();
	const char* fragShaderSrc = fragShaderStr.c_str();

	glShaderSource(vShader, 1, &vertShaderSrc, NULL);
	glShaderSource(fShader, 1, &fragShaderSrc, NULL);

	glCompileShader(vShader);
	checkOpenGLError();
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &vertCompiled);
	if (vertCompiled != 1) {
		cout << "vertex compilation failed" << endl;
		printShaderLog(vShader);
	}


	glCompileShader(fShader);
	checkOpenGLError();
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &fragCompiled);
	if (fragCompiled != 1) {
		cout << "fragment compilation failed" << endl;
		printShaderLog(fShader);
	}

	// Shader program objektum létrehozása. Eltároljuk az ID értéket.
	GLuint vfProgram = glCreateProgram();
	glAttachShader(vfProgram, vShader);
	glAttachShader(vfProgram, fShader);

	glLinkProgram(vfProgram);
	checkOpenGLError();
	glGetProgramiv(vfProgram, GL_LINK_STATUS, &linked);
	if (linked != 1) {
		cout << "linking failed" << endl;
		printProgramLog(vfProgram);
	}

	glDeleteShader(vShader);
	glDeleteShader(fShader);

	return vfProgram;
}

GLfloat nurbs_N(int i, int p, GLfloat u) {
	if (p == 0) {
		if (knots[i] <= u && u < knots[i + 1])
			return 1;
		else
			return 0;
	}
	GLfloat ret = (((u - knots[i]) / (knots[i + p] - knots[i])) * nurbs_N(i, p - 1, u))
				+ (((knots[i + p + 1] - u) / (knots[i + p + 1] - knots[i + 1])) * nurbs_N(i + 1, p - 1, u));
	return ret;
}

GLfloat nurbs_r(int i, int p, GLfloat u) {
	GLfloat up = nurbs_N(i, p, u);
	int nurbs_j = 0;
	GLfloat sum = 0;
	for (nurbs_j = 0; nurbs_j <= nurbs_n; nurbs_j++) {
		sum += nurbs_N(nurbs_j, p, u);
	}
	return up / sum;
}

void drawBezierCurve(std::vector<glm::vec3> controlPoints)
{
	if (!pointToDraw.empty()) {
		pointToDraw.clear();
	}
	
	glm::vec3 nextPoint;
	GLfloat nurbs_u = 0.0f;
	GLfloat increment = 1.0f / 10.0f; /* hány darab szakaszból rakjuk össze a görbénket? */

	while (nurbs_u <= (float)nurbs_n + nurbs_p) {
		nextPoint = glm::vec3(0.0f, 0.0f, 0.0f);
		for (int nurbs_i = 0; nurbs_i <= nurbs_n; nurbs_i++) {
			nextPoint.x = nextPoint.x + nurbs_r(nurbs_i, nurbs_p, nurbs_u) * controlPoints.at(nurbs_i * 2).x;
			nextPoint.y = nextPoint.y + nurbs_r(nurbs_i, nurbs_p, nurbs_u) * controlPoints.at(nurbs_i * 2).y;
		}
		pointToDraw.push_back(glm::vec3(nextPoint.x, nextPoint.y, nextPoint.z));
		pointToDraw.push_back(glm::vec3(0.6f, 0.8f, 0.2f));
		nurbs_u += increment;
	}
}

GLfloat dist2(glm::vec3 P1, glm::vec3 P2)
{
	GLfloat t1 = P1.x - P2.x;
	GLfloat t2 = P1.y - P2.y;

	return t1 * t1 + t2 * t2;
}

GLint getActivePoint(vector<glm::vec3> p, GLint size, GLfloat sens, GLfloat x, GLfloat y)
{
	GLint i;
	GLfloat s = sens * sens;

	GLfloat xNorm = x / (WIN_WIDTH / 2) - 1.0f;
	GLfloat yNorm = y / (WIN_HEIGHT / 2) - 1.0f;
	glm::vec3 P = glm::vec3(xNorm, yNorm, 0.0f);

	for (i = 0; i < size; i++) {
		if (dist2(p[i], P) < s && i % 2 == 0) {
			return i;
		}
	}
	return -1;
}

void cursorPosCallback(GLFWwindow* window, double xPos, double yPos)
{
	GLint i;

	GLfloat xNorm = xPos / (WIN_WIDTH / 2) - 1.0f;
	GLfloat yNorm = (WIN_HEIGHT - yPos) / (WIN_HEIGHT / 2) - 1.0f;

	if ((i = getActivePoint(myPoints, myPoints.size(), 0.05f, xPos, WIN_HEIGHT - yPos)) != -1) {
		if (i % 2 == 0) {
			hovered = i + (GLint)1;
			myPoints.at(hovered) = glm::vec3(0.8f, 0.8f, 1.0f);

			glBindBuffer(GL_ARRAY_BUFFER, kontrollPontokVBO);
			glBufferData(GL_ARRAY_BUFFER, myPoints.size() * sizeof(glm::vec3), myPoints.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}
	else if (hovered != -1) {
		myPoints.at(hovered) = purple;

		glBindBuffer(GL_ARRAY_BUFFER, kontrollPontokVBO);
		glBufferData(GL_ARRAY_BUFFER, myPoints.size() * sizeof(glm::vec3), myPoints.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		hovered = -1;
	}

	if (dragged >= 0 && dragged % 2 == 0)
	{
		myPoints.at(dragged).x = xNorm;
		myPoints.at(dragged).y = yNorm;

		glBindBuffer(GL_ARRAY_BUFFER, kontrollPontokVBO);
		glBufferData(GL_ARRAY_BUFFER, myPoints.size() * sizeof(glm::vec3), myPoints.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		drawBezierCurve(myPoints);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	GLint i;
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if ((i = getActivePoint(myPoints, myPoints.size(), 0.05f, x, WIN_HEIGHT - y)) != -1)
		{
			dragged = i;
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		dragged = -1;
	}
}

void init(GLFWwindow* window) {
	renderingProgram = createShaderProgram();

	for (int k = 0; k < knot_len; k++) {
		knots[k] = k;
	}
	drawBezierCurve(myPoints);

	/* Létrehozzuk a szükséges Vertex buffer és vertex array objektumot. */
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &kontrollPontokVBO);
	glGenVertexArrays(1, &VAO);
	glGenVertexArrays(1, &kontrollPontokVAO);

	/* Típus meghatározása: a GL_ARRAY_BUFFER nevesített csatolóponthoz kapcsoljuk a buffert (ide kerülnek a vertex adatok). */
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	/* Másoljuk az adatokat a pufferbe! Megadjuk az aktuálisan csatolt puffert,  azt hogy hány bájt adatot másolunk,
	a másolandó adatot, majd a feldolgozás módját is meghatározzuk: most az adat nem változik a feltöltés után */
	glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_DYNAMIC_DRAW);

	/* A puffer kész, lecsatoljuk, már nem szeretnénk módosítani. */
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, kontrollPontokVBO);
	glBufferData(GL_ARRAY_BUFFER, myPoints.size() * sizeof(glm::vec3), myPoints.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Csatoljuk a vertex array objektumunkat a konfiguráláshoz. */
	glBindVertexArray(VAO);

	/* Vertex buffer objektum újracsatolása. */
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	/* Ezen adatok szolgálják a 0 indexû vertex attribútumot (itt: pozíció).
	Elsõként megadjuk ezt az azonosítószámot.
	Utána az attribútum méretét (vec3, láttuk a shaderben).
	Harmadik az adat típusa.
	Negyedik az adat normalizálása, ez maradhat FALSE jelen példában.
	Az attribútum értékek hogyan következnek egymás után? Milyen lépésköz után találom a következõ vertex adatait?
	Végül megadom azt, hogy honnan kezdõdnek az értékek a pufferben. Most rögtön, a legelejétõl veszem õket.*/
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

	/* Engedélyezzük az imént definiált 0 indexû attribútumot. */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	/* Leválasztjuk a vertex array objektumot és a puffert is.*/
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(kontrollPontokVAO);
	glBindBuffer(GL_ARRAY_BUFFER, kontrollPontokVBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

}

/** A jelenetünk utáni takarítás. */
void cleanUpScene()
{
	/** Töröljük a vertex puffer és vertex array objektumokat. */
	glDeleteVertexArrays(1, &VAO);
	glDeleteVertexArrays(1, &kontrollPontokVAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &kontrollPontokVBO);

	/** Töröljük a shader programot. */
	glDeleteProgram(renderingProgram);
}

void display(GLFWwindow* window, double currentTime) {

	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // fontos lehet minden egyes alkalommal törölni!

	// aktiváljuk a shader-program objektumunkat.
	glUseProgram(renderingProgram);

	/*Csatoljuk a vertex array objektumunkat. */
	glBindVertexArray(VAO);

	glLineWidth(4.0f);
	glDrawArrays(GL_LINE_STRIP, 0, pointToDraw.size() / 2);
	glBindVertexArray(0);

	glBindVertexArray(kontrollPontokVAO);
	glPointSize(10.0f);
	glDrawArrays(GL_POINTS, 0, myPoints.size() / 2);

	glLineWidth(1.0f);
	glDisableVertexAttribArray(1);
	glDrawArrays(GL_LINE_STRIP, 0, myPoints.size() / 2);
	glEnableVertexAttribArray(1);
	/* Leválasztjuk, nehogy bármilyen érték felülíródjon.*/
	glBindVertexArray(0);
}

int main(void) {
	std::string filename = "D:/D_nemethy/UNIversity/DE IK-PTI Msc/2020-2021 3. felev/Geometriai modellezés/Beadando_projekt(git)/export2.txt";
	ifstream MyReadFile(filename);
	string myText;
	while (getline(MyReadFile, myText)) {
		std::string delimiter = " ";
		std::string token1 = myText.substr(0, myText.find(delimiter));
		myText.erase(0, myText.find(delimiter) + 1);
		double x = (std::stof(token1));
		if (x > maxX) {
			maxX = x;
		}
		double y = (std::stof(myText));
		if (y > maxY) {
			maxY = y;
		}
	}
	MyReadFile.close();

	ifstream MyReadFile1(filename);
	getline(MyReadFile1, myText);
	getline(MyReadFile1, myText);
	while (getline(MyReadFile1, myText)) {
		std::string delimiter = " ";
		std::string token1 = myText.substr(0, myText.find(delimiter));
		myText.erase(0, myText.find(delimiter) + 1);

		// FIXME
		double x = ((std::stof(token1)) / (maxX) - 0.25) * 2;
		double y = ((std::stof(myText)) / (maxY) - 0.25) * 4;

		myPoints.push_back(glm::vec3(x, y, 0.0f));
		myPoints.push_back(purple);
	}
	MyReadFile1.close();
	myPoints.pop_back();
	myPoints.pop_back();
	myPoints.pop_back();
	myPoints.pop_back();
	nurbs_n = myPoints.size() / 2 - 1;

	/* Próbáljuk meg inicializálni a GLFW-t! */
	if (!glfwInit()) { exit(EXIT_FAILURE); }

	/* A kívánt OpenGL verzió (4.3) */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	/* Próbáljuk meg létrehozni az ablakunkat. */
	GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Bezier gorbe", NULL, NULL);

	/* Válasszuk ki az ablakunk OpenGL kontextusát, hogy használhassuk. */
	glfwMakeContextCurrent(window);

	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	/* Incializáljuk a GLEW-t, hogy elérhetõvé váljanak az OpenGL függvények. */
	if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
	glfwSwapInterval(1);

	/* Az alkalmazáshoz kapcsolódó elõkészítõ lépések, pl. hozd létre a shader objektumokat. */
	init(window);

	while (!glfwWindowShouldClose(window)) {
		/* a kód, amellyel rajzolni tudunk a GLFWwindow ojektumunkba. */
		display(window, glfwGetTime());
		/* double buffered */
		glfwSwapBuffers(window);
		/* események kezelése az ablakunkkal kapcsolatban, pl. gombnyomás */
		glfwPollEvents();
	}

	/* töröljük a GLFW ablakot. */
	glfwDestroyWindow(window);
	/* Leállítjuk a GLFW-t */

	cleanUpScene();

	glfwTerminate();
	free(knots);
	exit(EXIT_SUCCESS);
}