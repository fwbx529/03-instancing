/* $URL$
   $Rev$
   $Author$
   $Date$
   $Id$
 */

#include <stdio.h>
#include <windows.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "LoadShaders.h"
#include "vbm.h"

float aspect;

GLuint color_buffer;
GLuint model_matrix_buffer;
GLuint color_tbo;
GLuint model_matrix_tbo;
GLuint render_prog;

GLint view_matrix_loc;
GLint projection_matrix_loc;

VBObject object;

ULONGLONG m_appStartTime;

#define INSTANCE_COUNT 100

void Initialize()
{
    m_appStartTime = ::GetTickCount64();

    ShaderInfo shader_info[] =
    {
        { GL_VERTEX_SHADER, "render.vs.glsl" },
        { GL_FRAGMENT_SHADER, "render.fs.glsl" },
        { GL_NONE, NULL }
    };

    render_prog = LoadShaders(shader_info);

    glUseProgram(render_prog);

    // Get the location of the projetion_matrix uniform
    view_matrix_loc = glGetUniformLocation(render_prog, "view_matrix");
    projection_matrix_loc = glGetUniformLocation(render_prog, "projection_matrix");

    // Set up the TBO samplers
    GLuint color_tbo_loc = glGetUniformLocation(render_prog, "color_tbo");
    GLuint model_matrix_tbo_loc = glGetUniformLocation(render_prog, "model_matrix_tbo");

    // Set them to the right texture unit indices
    glUniform1i(color_tbo_loc, 0);
    glUniform1i(model_matrix_tbo_loc, 1);

    // Load the object
    object.LoadFromVBM("armadillo_low.vbm", 0, 1, 2);

    /*

    THIS IS COMMENTED OUT HERE BECAUSE THE VBM OBJECT TAKES
    CARE OF IT FOR US

    // Get the locations of the vertex attributes in 'prog', which is the
    // (linked) program object that we're going to be rendering with. Note
    // that this isn't really necessary because we specified locations for
    // all the attributes in our vertex shader. This code could be made
    // more concise by assuming the vertex attributes are where we asked
    // the compiler to put them.
    int position_loc    = glGetAttribLocation(prog, "position");
    int normal_loc      = glGetAttribLocation(prog, "normal");

    // Configure the regular vertex attribute arrays - position and normal.
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(position_loc, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(position_loc);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(normal_loc);

    */

    // Now we set up the TBOs for the instance colors and the model matrices...

    // First, create the TBO to store colors, bind a buffer to it and initialize
    // its format. The buffer has previously been created and sized to store one
    // vec4 per-instance.
    glGenTextures(1, &color_tbo);
    glBindTexture(GL_TEXTURE_BUFFER, color_tbo);

    // Generate the colors of the objects
    glm::vec4 colors[INSTANCE_COUNT];

    for (int n = 0; n < INSTANCE_COUNT; n++)
    {
        float a = float(n) / 4.0f;
        float b = float(n) / 5.0f;
        float c = float(n) / 6.0f;

        colors[n][0] = 0.5f + 0.25f * (sinf(a + 1.0f) + 1.0f);
        colors[n][1] = 0.5f + 0.25f * (sinf(b + 2.0f) + 1.0f);
        colors[n][2] = 0.5f + 0.25f * (sinf(c + 3.0f) + 1.0f);
        colors[n][3] = 1.0f;
    }

    // Create the buffer, initialize it and attach it to the buffer texture
    glGenBuffers(1, &color_buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, color_buffer);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, color_buffer);

    // Now do the same thing with a TBO for the model matrices. The buffer object
    // (model_matrix_buffer) has been created and sized to store one mat4 per-
    // instance.
    glGenTextures(1, &model_matrix_tbo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, model_matrix_tbo);
    glGenBuffers(1, &model_matrix_buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, model_matrix_buffer);
    glBufferData(GL_TEXTURE_BUFFER, INSTANCE_COUNT * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, model_matrix_buffer);
    glActiveTexture(GL_TEXTURE0);
}

static inline int min(int a, int b)
{
    return a < b ? a : b;
}

void Display()
{
    ULONGLONG currentTime = ::GetTickCount64();
    unsigned int app_time = (unsigned int)(currentTime - m_appStartTime);
    float t = float(app_time & 0x3FFF) / float(0x3FFF);
    static float q = 0.0f;
    static const glm::vec3 X(1.0f, 0.0f, 0.0f);
    static const glm::vec3 Y(0.0f, 1.0f, 0.0f);
    static const glm::vec3 Z(0.0f, 0.0f, 1.0f);
    int n;

    // Set model matrices for each instance
    glm::mat4 matrices[INSTANCE_COUNT];

    for (n = 0; n < INSTANCE_COUNT; n++)
    {
        float a = 50.0f * float(n) / 4.0f;
        float b = 50.0f * float(n) / 5.0f;
        float c = 50.0f * float(n) / 6.0f;

        matrices[n] = glm::rotate(glm::mat4(), glm::radians(a + t * 360.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(), glm::radians(b + t * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(), glm::radians(c + t * 360.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::translate(glm::mat4(), glm::vec3(10.0f + a, 40.0f + b, 50.0f + c));
    }

    // Bind the weight VBO and change its data
    glBindBuffer(GL_TEXTURE_BUFFER, model_matrix_buffer);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(matrices), matrices, GL_DYNAMIC_DRAW);

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Setup
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Activate instancing program
    glUseProgram(render_prog);

    // Set up the view and projection matrices
    glm::mat4 view_matrix(glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -1500.0f)) * glm::rotate(glm::mat4(), glm::radians(t * 360.0f * 2.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::mat4 projection_matrix(glm::frustum(-1.0f, 1.0f, -aspect, aspect, 1.0f, 5000.0f));

    glUniformMatrix4fv(view_matrix_loc, 1, GL_FALSE, &view_matrix[0][0]);
    glUniformMatrix4fv(projection_matrix_loc, 1, GL_FALSE, &projection_matrix[0][0]);

    // Render INSTANCE_COUNT objects
    object.Render(0, INSTANCE_COUNT);
}

void Finalize(void)
{
    glUseProgram(0);
    glDeleteProgram(render_prog);
    glDeleteBuffers(1, &color_buffer);
    glDeleteBuffers(1, &model_matrix_buffer);
}

int main(int argc, char** argv)
{
    const int width = 800;
    const int height = 600;
    aspect = float(height) / float(width);

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(width, height, "gl_InstanceID Example", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();
    
    Initialize();
    while (!glfwWindowShouldClose(window))
    {
        Display();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    Finalize();

    glfwDestroyWindow(window);
    glfwTerminate();
}