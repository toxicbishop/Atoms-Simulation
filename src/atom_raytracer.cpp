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
const float electron_r = 0.25f;
const double zmSpeed = 10.0;
const float LIGHTING_SCALER = 700.0f;

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
};

// ================= Helpers ================= //
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
                                                     LIGHTING_SCALER, a0);
        state.particles.emplace_back(pos, toGlmColor(col));
    }
}

// ================= Raytracer ================= //
struct Sphere { vec4 center_radius; vec4 color; };

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
struct WindowState {
    Camera camera;
    AppState* app;
};

// ================= Engine ================= //
struct Engine {
    GLFWwindow* window;
    int WIDTH = 800;
    int HEIGHT = 600;

    // Raytracing resources
    GLuint raytracingShaderProgram;
    GLuint fullscreen_VAO, fullscreen_VBO;
    GLuint ssbo_spheres;

    Engine(int particle_count) {
        // --- Init GLFW ---
        if (!glfwInit()) { cerr << "GLFW init failed\n"; exit(EXIT_FAILURE); } 

        // --- Init Window ---
        window = glfwCreateWindow(WIDTH, HEIGHT, "Quantum Simulation - Raytraced", nullptr, nullptr);
        if (!window) { cerr << "Failed to create GLFW window\n"; glfwTerminate(); exit(EXIT_FAILURE); } 
        glfwMakeContextCurrent(window); glViewport(0, 0, WIDTH, HEIGHT); glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        cout << "Window created successfully." << endl;
        
        // --- Init GLEW ---
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) { cerr << "Failed to initialize GLEW\n"; glfwTerminate(); exit(EXIT_FAILURE); }
        cout << "GLEW initialized successfully." << endl;

        // blending for smooth rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // --- Initialize the SSBO ---
        glGenBuffers(1, &ssbo_spheres);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_spheres);
        glBufferData(GL_SHADER_STORAGE_BUFFER, particle_count * sizeof(Sphere), NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_spheres);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        raytracingShaderProgram = CreateRaytracingShaderProgram();
        cout << "Shader program created." << endl;
        setupFullscreenQuad();
        cout << "Fullscreen quad setup." << endl;
    }
    
    // ==== Raytracer functions ==== //
    void setupFullscreenQuad() {
        float quadVertices[] = {
            -1.0f,  1.0f,
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f
        };
        glGenVertexArrays(1, &fullscreen_VAO);
        glGenBuffers(1, &fullscreen_VBO);
        glBindVertexArray(fullscreen_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, fullscreen_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }
    GLuint CreateRaytracingShaderProgram() {
        const char* vertexShaderSource = R"(
            #version 430 core
            layout (location = 0) in vec2 aPos;
            out vec2 ScreenPos;
            void main() {
                ScreenPos = aPos;
                gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 430 core
            out vec4 FragColor;
            in vec2 ScreenPos;

            uniform vec3 camera_pos;
            uniform mat4 inv_view_proj;
            uniform vec3 light_pos;
            uniform float light_intensity;
            uniform vec3 ambient_light;

            struct Sphere {
                vec4 center_radius; // xyz = center, w = radius
                vec4 color;
            };

            layout(std430, binding = 0) buffer SphereBuffer {
                Sphere spheres[];
            };

            // Ray-sphere intersection test. Returns distance t or -1.0 for no hit.
            float intersect_sphere(vec3 ray_origin, vec3 ray_dir, vec3 sphere_center, float sphere_radius) {
                vec3 oc = ray_origin - sphere_center;
                float a = dot(ray_dir, ray_dir);
                float b = 2.0 * dot(oc, ray_dir);
                float c = dot(oc, oc) - sphere_radius * sphere_radius;
                float discriminant = b * b - 4.0 * a * c;
                if (discriminant < 0.0) {
                    return -1.0;
                } else {
                    return (-b - sqrt(discriminant)) / (2.0 * a);
                }
            }

            // Checks for any intersection along a ray up to a max distance. For shadows.
            bool any_hit(vec3 ray_origin, vec3 ray_dir, float max_dist) {
                uint num_spheres = spheres.length();
                for (uint i = 0; i < num_spheres; ++i) {
                    float t = intersect_sphere(ray_origin, ray_dir, spheres[i].center_radius.xyz, spheres[i].center_radius.w);
                    if (t > 0.0 && t < max_dist) {
                        return true;
                    }
                }
                return false;
            }

            void main() {
                // Construct ray from camera through the screen
                vec4 target = inv_view_proj * vec4(ScreenPos, 1.0, 1.0);
                vec3 ray_dir = normalize(vec3(target / target.w) - camera_pos);

                // Find closest sphere intersection
                float t_min = 1e20;
                int closest_sphere_idx = -1;
                uint num_spheres = spheres.length();
                for (uint i = 0; i < num_spheres; ++i) {
                    float t = intersect_sphere(camera_pos, ray_dir, spheres[i].center_radius.xyz, spheres[i].center_radius.w);
                    if (t > 0.0 && t < t_min) {
                        t_min = t;
                        closest_sphere_idx = int(i);
                    }
                }

                if (closest_sphere_idx != -1) {
                    // We hit a sphere, calculate lighting
                    vec3 hit_pos = camera_pos + t_min * ray_dir;
                    vec3 normal = normalize(hit_pos - spheres[closest_sphere_idx].center_radius.xyz);
                    vec3 sphere_color = spheres[closest_sphere_idx].color.rgb;

                    // Shadow calculation
                    vec3 light_dir = normalize(light_pos - hit_pos);
                    float light_dist = length(light_pos - hit_pos);
                    float shadow_factor = 1.0;
                    if (any_hit(hit_pos + normal * 0.001, light_dir, light_dist)) {
                        shadow_factor = 0.0;
                    }

                    // Diffuse lighting from point light
                    float diff = max(dot(normal, light_dir), 0.0);
                    vec3 diffuse = diff * sphere_color * shadow_factor * light_intensity;
                    
                    // Ambient lighting
                    vec3 ambient = ambient_light * sphere_color * light_intensity;
                    
                    FragColor = vec4(ambient + diffuse, spheres[closest_sphere_idx].color.a);
                } else {
                    // Nothing hit, draw background
                    FragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
                }
            }
        )";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }
    void runRayTracer(const Camera& camera, vector<Sphere>& sphere_data) {
        if (sphere_data.empty()) return;
        // Update GPU buffer with current sphere positions
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_spheres);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sphere_data.size() * sizeof(Sphere), sphere_data.data());
        
        // Link the buffer to binding point 0 for the shader
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_spheres);

        // Raytracing Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(raytracingShaderProgram);

        mat4 view = lookAt(camera.position(), camera.target, vec3(0,1,0));
        mat4 projection = perspective(radians(45.0f), (float)WIDTH/HEIGHT, 0.1f, 10000.0f);
        mat4 invViewProj = inverse(projection * view);

        // Define light properties and pass them to the shader
        vec3 light_pos = vec3(0.0f, 50.0f, 50.0f);
        vec3 ambient_light = vec3(0.2f);
        float light_intensity = 3.0f;

        glUniform3fv(glGetUniformLocation(raytracingShaderProgram, "camera_pos"), 1, value_ptr(camera.position()));
        glUniformMatrix4fv(glGetUniformLocation(raytracingShaderProgram, "inv_view_proj"), 1, GL_FALSE, value_ptr(invViewProj));
        glUniform3fv(glGetUniformLocation(raytracingShaderProgram, "light_pos"), 1, value_ptr(light_pos));
        glUniform3fv(glGetUniformLocation(raytracingShaderProgram, "ambient_light"), 1, value_ptr(ambient_light));
        glUniform1f(glGetUniformLocation(raytracingShaderProgram, "light_intensity"), light_intensity);
        
        glBindVertexArray(fullscreen_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
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
                sim.particle_count *= 10;
            } else if (key == GLFW_KEY_G) {
                sim.particle_count /= 10;
            }

            sim.clamp_quantum_numbers();
            generateParticles(*ws->app);
            cout << "Quantum numbers updated: n=" << sim.n << " l=" << sim.l
                 << " m=" << sim.m << " N=" << sim.particle_count << "\n";
        });
    }
};


// ================= Main Loop ================= //
int main () {
    // All state is local to main — no file-scope globals
    AppState app;
    app.sim.n = 3;
    app.sim.l = 1;
    app.sim.m = 1;
    app.sim.particle_count = 100000;

    Engine engine(app.sim.particle_count);

    WindowState ws;
    ws.app = &app;

    engine.setupCallbacks(&ws);

    // --- Sample initial particles ---
    generateParticles(app);

    float dt = 0.5f;
    cout << "Starting simulation..." << endl;
    while (!glfwWindowShouldClose(engine.window)) {

        // ------ Update & Build Sphere Data ------
        vector<Sphere> sphere_data;
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
            if (p.pos.z < 0 || p.pos.y < 0)
                sphere_data.push_back({vec4(p.pos, electron_r), p.color});
        }
        engine.runRayTracer(ws.camera, sphere_data);

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    glDeleteBuffers(1, &engine.ssbo_spheres);
    
    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}