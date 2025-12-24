#include "ShaderProgram.hpp"
#include "TextureLoader.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>
#include <vector>

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

    window = glfwCreateWindow(1920, 1080, "CG_TP_3 - Image Filter", nullptr, nullptr);
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

struct ButtonRect {
    int x;
    int y;
    int w;
    int h;
};

bool IsPointInside(int px, int py, const ButtonRect& r) {
    return px >= r.x && px <= (r.x + r.w) && py >= r.y && py <= (r.y + r.h);
}

struct SliderRect {
    int x;
    int y;
    int w;
    int h;
};

int ClampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

struct CachedBlur {
    GLuint fbo = 0;
    GLuint tex = 0;
    bool ready = false;
};

GLuint CreateColorTexture(GLsizei width, GLsizei height) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // Use linear RGBA8 to avoid sRGB double-decoding when blits/sample again.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint CreateFramebufferWithTexture(GLsizei width, GLsizei height, GLuint& outTex) {
    outTex = CreateColorTexture(width, height);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer incomplete: " << status << "\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void DestroyCachedBlur(CachedBlur& cb) {
    if (cb.tex) glDeleteTextures(1, &cb.tex);
    if (cb.fbo) glDeleteFramebuffers(1, &cb.fbo);
    cb = {};
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

    bool showFiltered = true;
    bool mouseHeld = false;
    bool sliderDragging = false;
    int radius = 1; // mean filter radius
    const int minRadius = 1;
    const int maxRadius = 5; // keep reasonable to avoid huge allocations

    // Single cached blur target reused for the current radius.
    CachedBlur cached;
    // Query texture size to allocate cache.
    GLint texWidth = 0, texHeight = 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    glBindTexture(GL_TEXTURE_2D, 0);
    cached.fbo = CreateFramebufferWithTexture(texWidth, texHeight, cached.tex);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Window and framebuffer sizes for UI scaling and scissor.
        int winWidth = 0, winHeight = 0;
        int fbWidth = 0, fbHeight = 0;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        // Define button in framebuffer coordinates (bottom-left origin for scissor).
        ButtonRect button {20, 20, 140, 40};
        // Slider next to the button.
        SliderRect slider {button.x + button.w + 20, button.y + 10, 200, 20};

        // Map cursor to framebuffer coordinates.
        double cursorX = 0.0, cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        double scaleX = winWidth > 0 ? static_cast<double>(fbWidth) / static_cast<double>(winWidth) : 1.0;
        double scaleY = winHeight > 0 ? static_cast<double>(fbHeight) / static_cast<double>(winHeight) : 1.0;
        int cursorFbX = static_cast<int>(cursorX * scaleX);
        // GLFW cursor Y origin is top-left; convert to bottom-left for scissor math.
        int cursorFbY = fbHeight - static_cast<int>(cursorY * scaleY);

        int mouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        bool mousePressed = mouseState == GLFW_PRESS;
        if (mousePressed && !mouseHeld && IsPointInside(cursorFbX, cursorFbY, button)) {
            showFiltered = !showFiltered;
            std::cout << "Filter toggled: " << (showFiltered ? "ON" : "OFF") << "\n";
        }
        // Slider drag begin.
        if (mousePressed && !mouseHeld && IsPointInside(cursorFbX, cursorFbY, {slider.x, slider.y, slider.w, slider.h})) {
            sliderDragging = true;
        }
        // Slider update.
        if (sliderDragging && mousePressed) {
            double t = (cursorFbX - slider.x) / static_cast<double>(slider.w);
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
            int newRadius = static_cast<int>(std::round(minRadius + t * (maxRadius - minRadius)));
            newRadius = ClampInt(newRadius, minRadius, maxRadius);
            if (newRadius != radius) {
                radius = newRadius;
                cached.ready = false; // invalidate cache when radius changes
                std::cout << "Radius set to: " << radius << "\n";
            }
        }
        if (!mousePressed) {
            sliderDragging = false;
        }
        mouseHeld = mousePressed;

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // If filtered view is requested and cache for current radius is not ready, render once into cache.
        if (showFiltered && !cached.ready) {
            GLint prevViewport[4];
            glGetIntegerv(GL_VIEWPORT, prevViewport);

            glBindFramebuffer(GL_FRAMEBUFFER, cached.fbo);
            glViewport(0, 0, texWidth, texHeight);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            program.Use();
            program.SetInt("uMode", 1);
            program.SetInt("uTexture", 0);
            program.SetInt("uRadius", radius);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);

            glBindVertexArray(quad.vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            cached.ready = true;
        }

        // Present: choose source texture based on mode (cached blur or original).
        program.Use();
        program.SetInt("uMode", 0);        // pass-through sampling
        program.SetInt("uTexture", 0);
        glActiveTexture(GL_TEXTURE0);
        if (showFiltered) {
            glBindTexture(GL_TEXTURE_2D, cached.tex);
        } else {
            glBindTexture(GL_TEXTURE_2D, texture);
        }

        glBindVertexArray(quad.vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // Simple on-screen button: draw a colored rectangle using scissor clear.
        // Green when filtered, gray when original. No text (keeps dependencies zero).
        glEnable(GL_SCISSOR_TEST);
        // Draw background.
        glScissor(button.x, button.y, button.w, button.h);
        if (showFiltered) {
            glClearColor(0.1f, 0.6f, 0.2f, 1.0f); // green
        } else {
            glClearColor(0.25f, 0.25f, 0.25f, 1.0f); // gray
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        // Slider track.
        glEnable(GL_SCISSOR_TEST);
        glScissor(slider.x, slider.y, slider.w, slider.h);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // Slider handle.
        double tHandle = (radius - minRadius) / static_cast<double>(maxRadius - minRadius);
        int handleW = 12;
        int handleX = slider.x + static_cast<int>(tHandle * slider.w) - handleW / 2;
        int handleY = slider.y - 2;
        int handleH = slider.h + 4;
        glScissor(handleX, handleY, handleW, handleH);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &texture);
    DestroyMesh(quad);
    DestroyCachedBlur(cached);
    glfwTerminate();
    return 0;
}

