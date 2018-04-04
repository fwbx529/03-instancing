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

GLuint color_buffer;
GLuint model_matrix_buffer;
GLuint render_prog;
GLint model_matrix_loc;
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

    // Load the object
    object.LoadFromVBM("armadillo_low.vbm", 0, 1, 2);

    // Bind its vertex array object so that we can append the instanced attributes
    object.BindVertexArray();

    // Get the locations of the vertex attributes in 'prog', which is the
    // (linked) program object that we're going to be rendering with. Note
    // that this isn't really necessary because we specified locations for
    // all the attributes in our vertex shader. This code could be made
    // more concise by assuming the vertex attributes are where we asked
    // the compiler to put them.
    int position_loc = glGetAttribLocation(render_prog, "position");
    int normal_loc = glGetAttribLocation(render_prog, "normal");
    int color_loc = glGetAttribLocation(render_prog, "color");
    int matrix_loc = glGetAttribLocation(render_prog, "model_matrix");

    // Configure the regular vertex attribute arrays - position and color.
    /*
    // This is commented out here because the VBM object takes care
    // of it for us.
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(position_loc, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(position_loc);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(normal_loc);
    */

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

    glGenBuffers(1, &color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_DYNAMIC_DRAW);

    // Now we set up the color array. We want each instance of our geometry
    // to assume a different color, so we'll just pack colors into a buffer
    // object and make an instanced vertex attribute out of it.
    glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(color_loc);
    // This is the important bit... set the divisor for the color array to
    // 1 to get OpenGL to give us a new value of 'color' per-instance
    // rather than per-vertex.
    glVertexAttribDivisor(color_loc, 1);

    // Likewise, we can do the same with the model matrix. Note that a
    // matrix input to the vertex shader consumes N consecutive input
    // locations, where N is the number of columns in the matrix. So...
    // we have four vertex attributes to set up.
    glGenBuffers(1, &model_matrix_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, model_matrix_buffer);
    glBufferData(GL_ARRAY_BUFFER, INSTANCE_COUNT * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
    // Loop over each column of the matrix...
    for (int i = 0; i < 4; i++)
    {
        // Set up the vertex attribute
        glVertexAttribPointer(matrix_loc + i,              // Location
                              4, GL_FLOAT, GL_FALSE,       // vec4
                              sizeof(glm::mat4),                // Stride
                              (void *)(sizeof(glm::vec4) * i)); // Start offset
                                                           // Enable it
        glEnableVertexAttribArray(matrix_loc + i);
        // Make it instanced
        glVertexAttribDivisor(matrix_loc + i, 1);
    }

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
    float t = float(app_time & 0x3FFF) / float(0x3FFF);
    static float q = 0.0f;
    static const glm::vec3 X(1.0f, 0.0f, 0.0f);
    static const glm::vec3 Y(0.0f, 1.0f, 0.0f);
    static const glm::vec3 Z(0.0f, 0.0f, 1.0f);
    int n;

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Setup
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Bind the weight VBO and change its data
    glBindBuffer(GL_ARRAY_BUFFER, model_matrix_buffer);

    // Set model matrices for each instance
    glm::mat4 * matrices = (glm::mat4 *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

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

    glUnmapBuffer(GL_ARRAY_BUFFER);

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