#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <thread>
#include <chrono>
#include <fstream>
#include <complex>
#include <random>

#include "orbital_math.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace glm;
using namespace std;

// ================= Constants ================= //
const float a0 = 1;
const double zmSpeed = 10.0;

// ================= Simulation State ================= //
struct Particle {
    vec3 pos;
    vec3 vel = vec3(0.0f);
    vec4 color;
    Particle(vec3 p, vec4 c = vec4(0.0f, 0.5f, 1.0f, 1.0f)) : pos(p), color(c){}
};

struct AppState {
    orbital::SimulationState sim;
    vector<Particle> particles;
    float electron_r = 1.5f;
};

// ================= Particle Generation ================= //
vec3 toGlmVec3(orbital::Vec3 v) { return vec3(v.x, v.y, v.z); }

vec4 toGlmColor(orbital::Color4 c) { return vec4(c.r, c.g, c.b, c.a); }

void generateParticles(AppState& state) {
    state.particles.clear();
    auto& sim = state.sim;
    for (int i = 0; i < sim.particle_count; ++i) {
        float r = static_cast<float>(sim.radial_sampler.sample(sim.n, sim.l, sim.rng, a0));
        float theta = static_cast<float>(sim.theta_sampler.sample(sim.l, sim.m, sim.rng));
        float phi = static_cast<float>(orbital::sample_phi(sim.rng));
        vec3 pos = toGlmVec3(orbital::spherical_to_cartesian(r, theta, phi));

        float len = length(pos);
        double ptheta = acos(pos.y / len);
        orbital::Color4 col = orbital::orbital_color(len, ptheta, sim.n, sim.l, sim.m,
                                                     1.5f * static_cast<float>(pow(5, sim.n)), a0);
        state.particles.emplace_back(pos, toGlmColor(col));
    }
}

// ================= Camera ================= //
struct Camera {
    vec3 target = vec3(0.0f, 0.0f, 0.0f);
    float radius = 50.0f;
    float azimuth = 0.0f;
    float elevation = M_PI / 2.0f;
    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = zmSpeed;
    bool dragging = false;
    bool panning = false;
    double lastX = 0.0, lastY = 0.0;

    vec3 position() const {
        float clampedElevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        return vec3(
            radius * sin(clampedElevation) * cos(azimuth),
            radius * cos(clampedElevation),
            radius * sin(clampedElevation) * sin(azimuth)
        );
    }
    void update() {
        target = vec3(0.0f, 0.0f, 0.0f);
    }

    void processMouseMove(double x, double y) {
        float dx = float(x - lastX);
        float dy = float(y - lastY);
        if (dragging) {
            azimuth += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        }
        lastX = x;
        lastY = y;
        update();
    }
    void processMouseButton(int button, int action, int mods, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                dragging = true;
                glfwGetCursorPos(win, &lastX, &lastY);
            } else if (action == GLFW_RELEASE) {
                dragging = false;
            }
        }
    }
    void processScroll(double xoffset, double yoffset) {
        radius -= yoffset * zoomSpeed;
        if (radius < 1.0f) radius = 1.0f;
        update();
    };
};

// ================= Combined Window State ================= //
// Passed to GLFW callbacks via glfwSetWindowUserPointer.
struct WindowState {
    Camera camera;
    AppState* app;
};

// ================= Engine ================= //
struct Engine {
    GLFWwindow* window;
    int WIDTH = 800;
    int HEIGHT = 600;

    // Rendering resources
    GLuint sphereVAO, sphereVBO;
    int sphereVertexCount;
    GLuint shaderProgram;
    GLint modelLoc, viewLoc, projLoc, colorLoc;

    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout(location=0) in vec3 aPos; uniform mat4 model; uniform mat4 view;
        uniform mat4 projection; out float lightIntensity;
        void main() { gl_Position = projection * view * model * vec4(aPos, 1.0);
            vec3 normal = normalize(aPos);
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            lightIntensity = max(dot(normal, lightDir), 0.5); // 0.2 is ambient light
        } )glsl";

    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        in float lightIntensity; 
        out vec4 FragColor; 
        uniform vec4 objectColor;

        void main() { 
            // Increase the power to make the 'center-facing' spot tighter and brighter
            float glow = pow(lightIntensity, 2.0); 
            FragColor = vec4(objectColor.rgb , objectColor.a); 
        } )glsl";

    Engine() {
        if (!glfwInit()) exit(-1);
        window = glfwCreateWindow(800, 600, "Atom Prob-Flow", NULL, NULL);
        glfwMakeContextCurrent(window);
        glewInit();
        glEnable(GL_DEPTH_TEST);

        // Generate Sphere Vertices manually (like you did in the gravity sim)
        vector<float> vertices;
        float r = 0.05f; // Small sphere for particles
        int stacks = 10, sectors = 10;
        for(int i = 0; i <= stacks; ++i){
            float t1 = (float)i / stacks * M_PI;
            float t2 = (float)(i+1) / stacks * M_PI;
            for(int j = 0; j < sectors; ++j){
                float p1 = (float)j / sectors * 2 * M_PI;
                float p2 = (float)(j+1) / sectors * 2 * M_PI;
                auto getPos = [&](float t, float p) {
                    return vec3(r*sin(t)*cos(p), r*cos(t), r*sin(t)*sin(p));
                };
                vec3 v1 = getPos(t1, p1), v2 = getPos(t1, p2), v3 = getPos(t2, p1), v4 = getPos(t2, p2);
                vertices.insert(vertices.end(), {v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z});
                vertices.insert(vertices.end(), {v2.x, v2.y, v2.z, v4.x, v4.y, v4.z, v3.x, v3.y, v3.z});
            }
        }
        sphereVertexCount = vertices.size() / 3;
        CreateVBOVAO(sphereVAO, sphereVBO, vertices);

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        // Get uniform locations
        modelLoc = glGetUniformLocation(shaderProgram, "model");
        viewLoc  = glGetUniformLocation(shaderProgram, "view");
        projLoc  = glGetUniformLocation(shaderProgram, "projection");
        colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    }

    void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const vector<float>& vertices) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    void drawSpheres(const Camera& camera, vector<Particle>& particles, float electron_r) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        mat4 projection = perspective(radians(45.0f), 800.0f/600.0f, 0.1f, 2000.0f);
        mat4 view = lookAt(camera.position(), camera.target, vec3(0, 1, 0)); 

        // Send view and projection to the shader
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projection));

        glBindVertexArray(sphereVAO);

        for (auto& p : particles) {
            if (p.pos.x < 0 && p.pos.y > 0) continue;
            mat4 model = translate(mat4(1.0f), p.pos);
            model = scale(model, vec3(electron_r));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));
            glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.color.a);
            
            glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
        }
    }
    void setupCallbacks(WindowState* ws) {
        glfwSetWindowUserPointer(window, ws);
        glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
            auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
            ws->camera.processMouseButton(button, action, mods, win);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
            auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
            ws->camera.processMouseMove(x, y);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
            auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
            ws->camera.processScroll(xoffset, yoffset);
        });
        // Key callback: modify quantum numbers via SimulationState
        glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
            if (!(action == GLFW_PRESS || action == GLFW_REPEAT)) return;
            auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
            auto& sim = ws->app->sim;

            if (key == GLFW_KEY_W) {
                sim.n += 1;
            } else if (key == GLFW_KEY_S) {
                sim.n -= 1;
            } else if (key == GLFW_KEY_E) {
                sim.l += 1;
            } else if (key == GLFW_KEY_D) {
                sim.l -= 1;
            } else if (key == GLFW_KEY_R) {
                sim.m += 1;
            } else if (key == GLFW_KEY_F) {
                sim.m -= 1;
            } else if (key == GLFW_KEY_T) {
                sim.particle_count += 100000;
            } else if (key == GLFW_KEY_G) {
                sim.particle_count -= 100000;
            }

            sim.clamp_quantum_numbers();
            ws->app->electron_r = float(sim.n) / 3.0f;
            generateParticles(*ws->app);
            cout << "Quantum numbers updated: n=" << sim.n << " l=" << sim.l
                 << " m=" << sim.m << " N=" << sim.particle_count << "\n";
        });
    }
};

// ================= Grid ================= //
struct Grid {
    GLuint gridVAO, gridVBO;
    vector<float> vertices;

    void init(Engine& engine) {
        vertices = CreateGridVertices(500.0f, 2);
        engine.CreateVBOVAO(gridVAO, gridVBO, vertices.data(), vertices.size());
    }
    void Draw(Engine& engine) {
        glUseProgram(engine.shaderProgram);
        glUniform4f(engine.colorLoc, 1.0f, 1.0f, 1.0f, 0.5f);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        DrawGrid(engine.shaderProgram, gridVAO, vertices.size());
    }
    void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
        glUseProgram(shaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(gridVAO);
        glPointSize(2.0f);
        glDrawArrays(GL_LINES, 0, vertexCount / 3);
        glBindVertexArray(0);
    }
    vector<float> CreateGridVertices(float size, int divisions) {
        
        std::vector<float> vertices;
        float step = size / divisions;
        float halfSize = size / 2.0f;

        // amount to extend the central X-axis line (in same units as size)
        float extra = step * 3.0f;
        int midZ = divisions / 2;

        // x axis
        for (int yStep = 3; yStep <= 3; ++yStep) {
            float y = 0;
            for (int zStep = 0; zStep <= divisions; ++zStep) {
                float z = -halfSize + zStep * step;
                for (int xStep = 0; xStep < divisions; ++xStep) {
                    float xStart = -halfSize + xStep * step;
                    float xEnd = xStart + step;

                    if (zStep == midZ) {
                        if (xStep == 0) {
                            xStart -= extra;
                        }
                        if (xStep == divisions - 1) {
                            xEnd += extra;
                        }
                    }

                    vertices.push_back(xStart); vertices.push_back(y); vertices.push_back(z);
                    vertices.push_back(xEnd);   vertices.push_back(y); vertices.push_back(z);
                }
            }
        }
        // z axis
        for (int xStep = 0; xStep <= divisions; ++xStep) {
            float x = -halfSize + xStep * step;
            for (int yStep = 3; yStep <= 3; ++yStep) {
                float y = 0;
                for (int zStep = 0; zStep < divisions; ++zStep) {
                    float zStart = -halfSize + zStep * step;
                    float zEnd = zStart + step;
                    vertices.push_back(x); vertices.push_back(y); vertices.push_back(zStart);
                    vertices.push_back(x); vertices.push_back(y); vertices.push_back(zEnd);
                }
            }
        }

        return vertices;
    }
};

// ================= Main Loop ================= //
int main () {
    // All state is local to main — no file-scope globals
    AppState app;
    app.sim.n = 2;
    app.sim.l = 1;
    app.sim.m = 0;
    app.sim.particle_count = 250000;
    app.electron_r = float(app.sim.n) / 3.0f;

    Engine engine;

    WindowState ws;
    ws.app = &app;

    Grid grid;
    grid.init(engine);

    GLint modelLoc = glGetUniformLocation(engine.shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(engine.shaderProgram, "objectColor");
    glUseProgram(engine.shaderProgram);
    engine.setupCallbacks(&ws);

    // --- Sample particles ---
    generateParticles(app);

    // Material setup
    GLfloat spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat shininess[] = { 50.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shininess);

    float dt = 0.5f;
    cout << "Starting simulation..." << endl;
    while (!glfwWindowShouldClose(engine.window)) {
        grid.Draw(engine);

        // ------ Update & Draw Particles ------
        for (Particle& p : app.particles) {
            double r = length(p.pos);
            if (r > 1e-6) {
                double theta = acos(p.pos.y / r);
                orbital::Vec3 flow = orbital::probability_flow(p.pos.x, p.pos.y, p.pos.z, app.sim.m);
                p.vel = vec3(flow.x, flow.y, flow.z);
                vec3 temp_pos = p.pos + p.vel * dt;
                double new_phi = atan2(temp_pos.z, temp_pos.x);
                p.pos = toGlmVec3(orbital::spherical_to_cartesian(
                    static_cast<float>(r), static_cast<float>(theta), static_cast<float>(new_phi)));
            }
        }
        engine.drawSpheres(ws.camera, app.particles, app.electron_r);

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}