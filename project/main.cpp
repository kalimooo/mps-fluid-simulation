
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
// Global constants
///////////////////////////////////////////////////////////////////////////////
#define DIM                  2
#define PARTICLE_DISTANCE    0.025
#define DT                   0.001
#define OUTPUT_INTERVAL      20
#define ARRAY_SIZE           5000
#define FINISH_TIME          2.0
#define KINEMATIC_VISCOSITY  (1.0E-6)
#define FLUID_DENSITY        1000.0 
#define GRAVITY_X  0.0      
#define GRAVITY_Y  -9.8
#define GRAVITY_Z  0.0      
#define RADIUS_FOR_NUMBER_DENSITY  (2.1*PARTICLE_DISTANCE) 
#define RADIUS_FOR_GRADIENT        (2.1*PARTICLE_DISTANCE) 
#define RADIUS_FOR_LAPLACIAN       (3.1*PARTICLE_DISTANCE) 
#define COLLISION_DISTANCE         (0.5*PARTICLE_DISTANCE)
#define THRESHOLD_RATIO_OF_NUMBER_DENSITY  0.97   
#define COEFFICIENT_OF_RESTITUTION 0.2
#define COMPRESSIBILITY (0.45E-9)
#define EPS             (0.01 * PARTICLE_DISTANCE)     
#define ON              1
#define OFF             0
#define RELAXATION_COEFFICIENT_FOR_PRESSURE 0.2
#define GHOST  -1
#define FLUID   0
#define WALL    2
#define DUMMY_WALL  3
#define GHOST_OR_DUMMY  -1
#define SURFACE_PARTICLE 1    
#define INNER_PARTICLE   0      
#define DIRICHLET_BOUNDARY_IS_NOT_CONNECTED 0 
#define DIRICHLET_BOUNDARY_IS_CONNECTED     1 
#define DIRICHLET_BOUNDARY_IS_CHECKED       2 

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
vec2* particlePositions;
vec2* particleVelocities;
uint num_particles;
int* particleTypes;


GLuint posVBO;
FboInfo fbo;


void calculateConstantParameter() {

}

void updateGrid() {
	labhelper::perf::Scope s( "Update Grid" );
}

void initParticles()
{
	// Allocate particles array
	particlePositions = new vec2[ARRAY_SIZE];
	particleVelocities = new vec2[ARRAY_SIZE];
	particleTypes = new int[ARRAY_SIZE];

	int i = 0;

	int nX = (int)(1.0/PARTICLE_DISTANCE)+5;  
	int nY = (int)(0.6/PARTICLE_DISTANCE)+5;
	bool particleGenerated = false;

	for (int iX = -4; iX < nX; iX++) {
		for (int iY = -4; iY < nY; iY++) {
			float x = PARTICLE_DISTANCE * (float)iX;
			float y = PARTICLE_DISTANCE * (float)iY;
			
			if( ((x>-4.0*PARTICLE_DISTANCE+EPS)&&(x<=1.00+4.0*PARTICLE_DISTANCE+EPS))&&( (y>0.0-4.0*PARTICLE_DISTANCE+EPS )&&(y<=0.6+EPS)) ){  /* dummy wall region */
				particleTypes[i]=DUMMY_WALL;
				particleGenerated = true;
			}
				
			if( ((x>-2.0*PARTICLE_DISTANCE+EPS)&&(x<=1.00+2.0*PARTICLE_DISTANCE+EPS))&&( (y>0.0-2.0*PARTICLE_DISTANCE+EPS )&&(y<=0.6+EPS)) ){ /* wall region */
				particleTypes[i]=WALL;
				particleGenerated = true;
			}
			
			if( ((x>-4.0*PARTICLE_DISTANCE+EPS)&&(x<=1.00+4.0*PARTICLE_DISTANCE+EPS))&&( (y>0.6-2.0*PARTICLE_DISTANCE+EPS )&&(y<=0.6+EPS)) ){  /* wall region */
				particleTypes[i]=WALL;
				particleGenerated = true;
			}
			
			if( ((x>0.0+EPS)&&(x<=1.00+EPS))&&( y>0.0+EPS )){  /* empty region */
				particleGenerated = false;
			}
			
			if( ((x>0.0+EPS)&&(x<=0.25+EPS)) &&((y>0.0+EPS)&&(y<=0.50+EPS)) ){  /* fluid region */
				particleTypes[i]=FLUID;
				particleGenerated = true;
			}

			if( particleGenerated == true){
				particlePositions[i] = vec2(x, y);
				i++;
			}
		}
	}
	num_particles = i;
	for(i=0;i<num_particles;i++) particleVelocities[i] = vec2(0.f);
	printf("Particle amount: %d\n", num_particles);
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * num_particles, particlePositions, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), 0);
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
		glDrawArrays(GL_POINTS, 0, num_particles);
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
