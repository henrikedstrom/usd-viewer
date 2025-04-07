#pragma once

// Standard Library Headers
#include <cstdint>
#include <memory>
#include <string>

// Project Headers
#include "camera.h"
#include "orbit_controls.h"
#include "usd_headers.h"

// Forward Declarations
struct GLFWwindow;

// Application Class
class Application
{
  public:
    // Static Instance Getter
    static Application *GetInstance();

    // Constructor and Destructor
    explicit Application(uint32_t width, uint32_t height);
    ~Application();

    // Deleted Functions
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;

    // Public Interface
    void Run();
    void OnKeyPressed(int key, int mods);
    void OnResize(int width, int height);
    void OnFileDropped(const std::string &filename, uint8_t *data = 0, int length = 0);

  private:
    // Private Member Functions
    void MainLoop();
    void ProcessFrame();
    void LoadScene(const std::string &filename);
    void SetupLighting();

    // Static Instance
    static Application *s_instance;

    // App Variables
    uint32_t m_width;
    uint32_t m_height;
    bool m_quitApp = false;

    // Window and Camera Controls
    GLFWwindow *m_window = nullptr;
    Camera m_camera;
    std::unique_ptr<OrbitControls> m_controls;

    // USD Stage and Renderer
    pxr::UsdStageRefPtr m_stage;
    std::unique_ptr<pxr::UsdImagingGLEngine> m_renderer;
};