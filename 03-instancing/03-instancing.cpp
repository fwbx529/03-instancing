/* $URL$
   $Rev$
   $Author$
   $Date$
   $Id$
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>

#include <windows.h>

#include "LoadShaders.h"
#include "vbm.h"

float aspect;
GLuint update_prog;
GLuint vao[2];
GLuint vbo[2];
GLuint xfb;

GLuint weight_vbo;
GLuint color_vbo;
GLuint render_prog;
GLint render_model_matrix_loc;
GLint render_projection_matrix_loc;

GLuint geometry_tex;

GLuint geometry_xfb;
GLuint particle_xfb;

GLint model_matrix_loc;
GLint projection_matrix_loc;
GLint triangle_count_loc;
GLint time_step_loc;

VBObject object;

ULONGLONG m_appStartTime;

#define INSTANCE_COUNT 200

static unsigned int seed = 0x13371337;

static inline float random_float()
{
    float res;
    unsigned int tmp;

    seed *= 16807;

    tmp = seed ^ (seed >> 4) ^ (seed << 15);

    *((unsigned int *) &res) = (tmp >> 9) | 0x3F800000;

    return (res - 1.0f);
}

static glm::vec3 random_vector(float minmag = 0.0f, float maxmag = 1.0f)
{
    glm::vec3 randomvec(random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f);
    randomvec = normalize(randomvec);
    randomvec *= (random_float() * (maxmag - minmag) + minmag);

    return randomvec;
}

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

    // "model_matrix" is actually an array of 4 matrices
    render_model_matrix_loc = glGetUniformLocation(render_prog, "model_matrix");
    render_projection_matrix_loc = glGetUniformLocation(render_prog, "projection_matrix");

    // Load the object
    object.LoadFromVBM("armadillo_low.vbm", 0, 1, 2);

    // Bind its vertex array object so that we can append the instanced attributes
    object.BindVertexArray();

    // Generate the colors of the objects
    glm::vec4 colors[INSTANCE_COUNT];

    for (int n = 0; n < INSTANCE_COUNT; n++)
    {
        float a = float(n) / 4.0f;
        float b = float(n) / 5.0f;
        float c = float(n) / 6.0f;

        colors[n][0] = 0.5f * (sinf(a + 1.0f) + 1.0f);
        colors[n][1] = 0.5f * (sinf(b + 2.0f) + 1.0f);
        colors[n][2] = 0.5f * (sinf(c + 3.0f) + 1.0f);
        colors[n][3] = 1.0f;
    }

    // Create and allocate the VBO to hold the weights
    // Notice that we use the 'colors' array as the initial data, but only because
    // we know it's the same size.
    glGenBuffers(1, &weight_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, weight_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_DYNAMIC_DRAW);

    // Here is the instanced vertex attribute - set the divisor
    glVertexAttribDivisor(3, 1);
    // It's otherwise the same as any other vertex attribute - set the pointer and enable it
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(3);

    // Same with the instance color array
    glGenBuffers(1, &color_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, color_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

    glVertexAttribDivisor(4, 1);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(4);

    // Done (unbind the object's VAO)
    glBindVertexArray(0);
}

static inline int min(int a, int b)
{
    return a < b ? a : b;
}

void Display()
{
    ULONGLONG currentTime = ::GetTickCount64();
    unsigned int app_time = (unsigned int)(currentTime - m_appStartTime);
    float t = float(app_time & 0x3FFFF) / float(0x3FFFF);
    static float q = 0.0f;
    static const glm::vec3 X(1.0f, 0.0f, 0.0f);
    static const glm::vec3 Y(0.0f, 1.0f, 0.0f);
    static const glm::vec3 Z(0.0f, 0.0f, 1.0f);
    int n;

    // Set weights for each instance
    glm::vec4 weights[INSTANCE_COUNT];

    for (n = 0; n < INSTANCE_COUNT; n++)
    {
        float a = float(n) / 4.0f;
        float b = float(n) / 5.0f;
        float c = float(n) / 6.0f;

        weights[n][0] = 0.5f * (sinf(t * 6.28318531f * 8.0f + a) + 1.0f);
        weights[n][1] = 0.5f * (sinf(t * 6.28318531f * 26.0f + b) + 1.0f);
        weights[n][2] = 0.5f * (sinf(t * 6.28318531f * 21.0f + c) + 1.0f);
        weights[n][3] = 0.5f * (sinf(t * 6.28318531f * 13.0f + a + b) + 1.0f);
    }

    // Bind the weight VBO and change its data
    glBindBuffer(GL_ARRAY_BUFFER, weight_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(weights), weights, GL_DYNAMIC_DRAW);

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Setup
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Activate instancing program
    glUseProgram(render_prog);

    // Set four model matrices
    glm::mat4 model_matrix[4];

    for (n = 0; n < 4; n++)
    {
        model_matrix[n] = (glm::scale(glm::mat4(), glm::vec3(5.0f, 5.0f, 5.0f)) *
                           glm::rotate(glm::mat4(), glm::radians(t * 360.0f * 40.0f + float(n + 1) * 29.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                           glm::rotate(glm::mat4(), glm::radians(t * 360.0f * 20.0f + float(n + 1) * 35.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
                           glm::rotate(glm::mat4(), glm::radians(t * 360.0f * 30.0f + float(n + 1) * 67.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                           glm::translate(glm::mat4(), glm::vec3((float)n * 10.0f - 15.0f, 0.0f, 0.0f)) *
                           glm::scale(glm::mat4(), glm::vec3(0.01f, 0.01f, 0.01f)));
    }

    glUniformMatrix4fv(render_model_matrix_loc, 4, GL_FALSE, &model_matrix[0][0][0]);

    // Set up the projection matrix
    glm::mat4 projection_matrix(glm::frustum(-1.0f, 1.0f, -aspect, aspect, 1.0f, 5000.0f) * glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -100.0f)));

    glUniformMatrix4fv(render_projection_matrix_loc, 1, GL_FALSE, &projection_matrix[0][0]);

    // Render INSTANCE_COUNT objects
    object.Render(0, INSTANCE_COUNT);
}

void Finalize(void)
{
    glUseProgram(0);
    glDeleteProgram(update_prog);
    glDeleteVertexArrays(2, vao);
    glDeleteBuffers(2, vbo);
}

int main(int argc, char** argv)
{
    const int width = 800;
    const int height = 600;
    aspect = float(height) / float(width);

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(width, height, "Instancing Example", NULL, NULL);
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