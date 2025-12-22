#include "ShaderProgram.hpp"
#include "TextureLoader.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

void FramebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

bool InitGL(GLFWwindow*& window) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1280, 720, "CG_TP_3 - Image Filter", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return false;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    return true;
}

struct QuadMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
};

QuadMesh CreateFullscreenQuad() {
    // Positions (x, y) and UVs (u, v)
    float vertices[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    unsigned int indices[] = {0, 1, 2, 2, 3, 0};

    QuadMesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return mesh;
}

void DestroyMesh(QuadMesh& mesh) {
    glDeleteBuffers(1, &mesh.ebo);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteVertexArrays(1, &mesh.vao);
    mesh = {};
}

} // namespace

int main() {
    GLFWwindow* window = nullptr;
    if (!InitGL(window)) {
        return 1;
    }

    const fs::path shaderDir = fs::path(PROJECT_SOURCE_DIR) / "assets" / "shaders";
    ShaderProgram program;
    std::string error;
    if (!program.LoadFromFiles(shaderDir / "filter.vert", shaderDir / "filter.frag", &error)) {
        std::cerr << error << "\n";
        glfwTerminate();
        return 1;
    }

    const fs::path texturePath = fs::path(PROJECT_SOURCE_DIR) / "assets" / "textures" / "cyberpunk.png";
    GLuint texture = 0;
    if (!gfx::LoadTexture2D(texturePath, texture, &error)) {
        std::cerr << error << "\n";
        glfwTerminate();
        return 1;
    }

    QuadMesh quad = CreateFullscreenQuad();

    program.Use();
    program.SetInt("uTexture", 0);
    glUseProgram(0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        program.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(quad.vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &texture);
    DestroyMesh(quad);
    glfwTerminate();
    return 0;
}

