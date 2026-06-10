
#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#include <stdio.h>


///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;
bool isPaused = false;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;
bool followMouse = false;
ivec2 mousePos = { -1, -1 };

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image

///////////////////////////////////////////////////////////////////////////////
// VAO
///////////////////////////////////////////////////////////////////////////////
GLuint vao;

///////////////////////////////////////////////////////////////////////////////
// Data for the particles
///////////////////////////////////////////////////////////////////////////////
struct particle {
	vec2 position;
};

GLuint posVBO;
FboInfo fbo;

particle* particles;
const int NUM_PARTICLES = 20;


void updateGrid() {
	labhelper::perf::Scope s( "Update Grid" );
}

void initParticles()
{
	// Allocate particles array
	particles = new particle[NUM_PARTICLES];

	// Define the target 2D box: from (-0.5, -0.5) to (0.5, 0.5)
	const float minX = -0.5f, maxX = 0.5f;
	const float minY = -0.5f, maxY = 0.5f;

	// Choose grid dimensions (nx * ny >= NUM_PARTICLES) to arrange particles evenly
	int nx = (int)std::ceil(std::sqrt((float)NUM_PARTICLES));
	int ny = (int)std::ceil((float)NUM_PARTICLES / (float)nx);

	int idx = 0;
	for (int j = 0; j < ny; ++j)
	{
		for (int i = 0; i < nx; ++i)
		{
			if (idx >= NUM_PARTICLES) break;
			float u = (nx == 1) ? 0.5f : (float)i / (float)(nx - 1);
			float v = (ny == 1) ? 0.5f : (float)j / (float)(ny - 1);
			particles[idx].position = vec2(
				minX + u * (maxX - minX),
				minY + v * (maxY - minY)
			);
			++idx;
		}
	}
}


void updateparticlePositions(float deltaTime, bool use_GPU)
{
	{	
		labhelper::perf::Scope s( "Update particles" );
		
	}
}

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/shader.vert", "../project/shader.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	initParticles();

	///////////////////////////////////////////////////////////////////////
	// Generate and bind buffers for graphics pipeline
	///////////////////////////////////////////////////////////////////////
	glGenBuffers(1, &posVBO);
	glBindBuffer(GL_ARRAY_BUFFER, posVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(particle) * NUM_PARTICLES, particles, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(particle), 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	fbo = FboInfo();
	fbo.resize(w, h);
	
	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glPointSize(5.0f);

	labhelper::hideGUI();
	
	//glEnable(GL_CULL_FACE);  // enables backface culling
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}

		if(fbo.width != w || fbo.height != h) fbo.resize(w, h);
	}

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////	
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebufferId);
	glViewport(0, 0, fbo.width, fbo.height);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render scene
	{
		labhelper::perf::Scope s( "Scene" );
		
		glUseProgram(shaderProgram);
		glBindVertexArray(vao);
		glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
		glBindVertexArray(0);
		
	}
	// Blit the rendered FBO to the default framebuffer (the screen)
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.framebufferId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(
		0, 0, fbo.width, fbo.height,
		0, 0, fbo.width, fbo.height,
		GL_COLOR_BUFFER_BIT, GL_NEAREST
	);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		labhelper::processEvent( &event );

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE)
		{
			isPaused = !isPaused;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if ( labhelper::isGUIvisible() )
			{
				labhelper::hideGUI();
			}
			else
			{
				labhelper::showGUI();
			}
		 
		}
		
		if (event.type == SDL_MOUSEMOTION)
		{
			mousePos.x = event.motion.x;
			mousePos.y = event.motion.y;
		}
		
	}
	
	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	ImGui::Text("Mouse control:");
	ImGui::Checkbox("Follow mouse", &followMouse);

	// ImGui::Text("particle parameters:");
	// ImGui::SliderFloat("kernelScalingFactor", &kernelScalingFactor, 0.01f, 10.0f);
	// ImGui::SliderFloat("smoothingRadius", &smoothingRadius, 0.01f, 2.0f / (float)gridSize);
	// ImGui::Checkbox("Gravity enabled", &gravityEnabled);
	// ImGui::SliderFloat("gravityStrength", &gravityStrength, 0.0f, 1.0f);

	// ImGui::SliderFloat("visualRange", &visualRange, 0.0f, 2.0f);
	// ImGui::SliderFloat("protectedRange", &protectedRange, 0.0f, 1.0f);
	// ImGui::SliderFloat("centeringFactor", &centeringFactor, 0.0f, 0.1f);
	// ImGui::SliderFloat("matchingFactor", &matchingFactor, 0.0f, 0.1f);
	// ImGui::SliderFloat("avoidFactor", &avoidFactor, 0.0f, 0.5f);
	// ImGui::SliderFloat("borderMargin", &matchingFactor, 0.0f, 0.3f);
	// ImGui::SliderFloat("turnFactor", &turnFactor, 0.0f, 0.5f);
	// ImGui::SliderFloat("minSpeed", &minSpeed, 0.0f, 0.5f);
	// ImGui::SliderFloat("maxSpeed", &maxSpeed, 0.0f, 1.0f);
	// ImGui::SliderFloat("randFactor", &randFactor, 0.0f, 1.0f);

	ImGui::GetIO().FontGlobalScale = 2.0f;

	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project", 1920, 1080);

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		if (isPaused)
		{
			continue;
		}

		// Inform imgui of new frame
		labhelper::newFrame( g_window );
		
		// Update particles
		updateparticlePositions(deltaTime, true);

		updateGrid();
		
		// render to window
		display();

		// Render overlay GUI.
		gui();

		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
