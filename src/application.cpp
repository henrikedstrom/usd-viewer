// Standard Library Headers
#include <algorithm>
#include <filesystem>
#include <iostream>

// Third-Party Library Headers
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Project Headers
#include "application.h"

// Static Application Instance
Application *Application::s_instance = nullptr;

//----------------------------------------------------------------------
// Internal Utility Functions

namespace
{

#define CHECK_GL_ERROR(line)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        GLenum error = glGetError();                                                                                   \
        if (error != GL_NO_ERROR)                                                                                      \
        {                                                                                                              \
            std::cerr << "OpenGL error at line " << line << ": " << error << std::endl;                                \
        }                                                                                                              \
    } while (0)

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    static bool keyState[GLFW_KEY_LAST] = {false};

    if (key >= 0 && key < GLFW_KEY_LAST)
    {
        bool keyPressed = action == GLFW_PRESS && !keyState[key];
        bool keyReleased = action == GLFW_RELEASE && keyState[key];

        if (action == GLFW_PRESS)
        {
            keyState[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            keyState[key] = false;
        }

        if (keyPressed)
        {
            Application::GetInstance()->OnKeyPressed(key, mods);
        }
    }
}

void ComputeSceneBounds(const pxr::UsdStageRefPtr &stage, glm::vec3 &minBounds, glm::vec3 &maxBounds)
{
    if (!stage)
    {
        std::cerr << "ComputeSceneBounds: stage is null." << std::endl;
        return;
    }

    pxr::UsdPrim pseudoRoot = stage->GetPseudoRoot();
    if (!pseudoRoot)
    {
        std::cerr << "ComputeSceneBounds: pseudo-root is null." << std::endl;
        return;
    }

    pxr::GfBBox3d bbox;
    {
        pxr::UsdGeomBBoxCache bboxCache(pxr::UsdTimeCode::Default(), pxr::UsdGeomImageable::GetOrderedPurposeTokens(),
                                        /* useExtentsHint = */ true);
        bbox = bboxCache.ComputeWorldBound(pseudoRoot);
    }

    pxr::GfRange3d range = bbox.GetRange();
    minBounds = glm::vec3(range.GetMin()[0], range.GetMin()[1], range.GetMin()[2]);
    maxBounds = glm::vec3(range.GetMax()[0], range.GetMax()[1], range.GetMax()[2]);

    // Print the bounds
    std::cout << "Scene Bounds: " << minBounds.x << ", " << minBounds.y << ", " << minBounds.z << " to " << maxBounds.x
              << ", " << maxBounds.y << ", " << maxBounds.z << std::endl;
}

pxr::GfMatrix4d ToGfMatrix(const glm::mat4 &m)
{
    pxr::GfMatrix4d result;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            result[row][col] = static_cast<double>(m[row][col]);
    return result;
}

pxr::GfMatrix3d ToGfMatrix(const glm::mat3 &m)
{
    pxr::GfMatrix3d result;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            result[row][col] = static_cast<double>(m[row][col]);
    return result;
}

pxr::GfMatrix4d ExpandTo4x4(const glm::mat3 &m)
{
    pxr::GfMatrix4d result(1.0);
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            result[row][col] = static_cast<double>(m[row][col]);
    return result;
}

// Convert one sRGB channel into linear space:
static float SrgbToLinear(float cs) {
    if (cs <= 0.04045f) {
        return cs / 12.92f;
    } else {
        return std::pow((cs + 0.055f) / 1.055f, 2.4f);
    }
}

// Convert an entire GfVec4f (rgb in [0,1], alpha pass‑through) to linear:
static pxr::GfVec4f ToLinear(const pxr::GfVec4f &c) {
    return {
        SrgbToLinear(c[0]),
        SrgbToLinear(c[1]),
        SrgbToLinear(c[2]),
        c[3]
    };
}


} // namespace

//----------------------------------------------------------------------
// Application Class Implementation

Application *Application::GetInstance()
{
    return s_instance;
}

Application::Application(uint32_t width, uint32_t height) : m_windowWidth(width), m_windowHeight(height)
{
    assert(!s_instance); // Ensure only one instance exists
    s_instance = this;
}

Application::~Application()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
    s_instance = nullptr;
}

void Application::Run()
{
    if (!glfwInit())
    {
        return;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    // Create a windowed mode window and its OpenGL context
    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "USD Viewer", nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
 
    // Make the window's context current
    glfwMakeContextCurrent(m_window);

    // Disable V-Sync
    glfwSwapInterval(0);

    // Initialize GLAD
    if (!gladLoadGL())
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    CHECK_GL_ERROR(__LINE__);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << "\n";

    // Handle high-DPI/Retina displays
    int actualWidth, actualHeight;
    glfwGetWindowSize(m_window, &actualWidth, &actualHeight);
    OnResize(actualWidth, actualHeight);

    // Setup input callbacks
    m_controls = std::make_unique<OrbitControls>(m_window, &m_camera);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
        Application::GetInstance()->OnResize(width, height);
    });

    // Setup file drop callback
    glfwSetDropCallback(m_window, [](GLFWwindow *window, int count, const char **paths) {
        if (count > 0)
        {
            Application::GetInstance()->OnFileDropped(paths[0]);
        }
    });

    // Initialize GL Context Capabilities
    pxr::GlfContextCaps::InitInstance();

    // Load the default scene
    LoadScene("assets/Kitchen_set/Kitchen_set.usd");

    // Enter the main loop
    MainLoop();
}

void Application::OnKeyPressed(int key, int mods)
{
    if (key == GLFW_KEY_ESCAPE)
    {
        m_quitApp = true;
    }
    else if (key == GLFW_KEY_HOME)
    {
        glm::vec3 minBounds, maxBounds;
        ComputeSceneBounds(m_stage, minBounds, maxBounds);
        m_camera.ResetToModel(minBounds, maxBounds);
    }
}

void Application::OnResize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    m_camera.ResizeViewport(width, height);

    // Framebuffer size - may differ from window size on high-DPI displaysdow size on high-DPI displays
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    m_framebufferWidth = framebufferWidth;
    m_framebufferHeight = framebufferHeight;
}

void Application::OnFileDropped(const std::string &filename, uint8_t *data, int length)
{
    // Get the extension in lower‑case
    auto ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".exr" || ext == ".hdr")
    {
        // Reload dome light texture
        m_domeLightTexture = filename;
        InitHydra();
    }
    else if (ext == ".usd" || ext == ".usda" || ext == ".usdc" || ext == ".usdz")
    {
        // Load USD scene
        LoadScene(filename);
    }
    else
    {
        std::cerr << "Unsupported file type: " << filename << std::endl;
    }
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(m_window) && !m_quitApp)
    {
        glfwPollEvents();

        ProcessFrame();

        m_fpsCounter.tick(m_window);
    }

    // Destroy Hydra resources flush the GL pipeline
    m_engine.reset();
    m_hgiInterop.reset();
    glFinish();

    // Destroy the window and terminate GLFW
    glfwDestroyWindow(m_window);
    m_window = nullptr;
}

void Application::ProcessFrame()
{ 
    // Update camera
    pxr::GfMatrix4d viewMatrix = ToGfMatrix(m_camera.GetViewMatrix());
    pxr::GfMatrix4d projMatrix = ToGfMatrix(m_camera.GetProjectionMatrix());
    m_engine->SetCameraState(viewMatrix, projMatrix);

    // Update viewport and render buffer size
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    m_engine->SetRenderViewport(pxr::GfVec4d(0, 0, m_framebufferWidth, m_framebufferHeight));
    m_engine->SetRenderBufferSize(pxr::GfVec2i(m_framebufferWidth, m_framebufferHeight));
    m_engine->SetWindowPolicy(pxr::CameraUtilConformWindowPolicy::CameraUtilFit);
    m_engine->SetRendererAov(pxr::HdAovTokens->color);

    // Clear the screen
    pxr::GfVec4f clearColor(0.09f, 0.24f, 0.43f, 1.0f);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    CHECK_GL_ERROR(__LINE__);

    // Init render params
    pxr::UsdImagingGLRenderParams renderParams{};
    renderParams.cullStyle = pxr::UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
    renderParams.clearColor = clearColor;
    renderParams.showProxy = false;
    renderParams.showRender = true;
    renderParams.gammaCorrectColors = false;
    renderParams.colorCorrectionMode = pxr::HdxColorCorrectionTokens->sRGB;

    // Render the scene
    m_engine->Render(m_stage->GetPseudoRoot(), renderParams);

    // Get the color AOV texture and transfer it to OpenGL back buffer
    pxr::HgiTextureHandle aovTexture = m_engine->GetAovTexture(pxr::HdAovTokens->color);
    if (aovTexture)
    {
        uint32_t framebuffer = 0;
        m_hgiInterop->TransferToApp(m_engine->GetHgi(), aovTexture,
                                    /*srcDepth*/ pxr::HgiTextureHandle(), pxr::HgiTokens->OpenGL,
                                    pxr::VtValue(framebuffer),
                                    pxr::GfVec4i(0, 0, m_framebufferWidth, m_framebufferHeight));
        CHECK_GL_ERROR(__LINE__);
    }
    else
    {
        std::cerr << "Failed to get AOV texture." << std::endl;
    }

    // Swap front and back buffers
    glfwSwapBuffers(m_window);
}

void Application::LoadScene(const std::string &filename)
{
    // Open the stage from disk
    pxr::UsdStageRefPtr srcStage = pxr::UsdStage::Open(filename);
    if (!srcStage)
    {
        std::cerr << "Failed to load stage: " << filename << std::endl;
        return;
    }

    // Create an in‑memory stage
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();

    // Define a World root
    pxr::UsdPrim world = stage->DefinePrim(pxr::SdfPath("/World"), pxr::TfToken("Scope"));

    // Under the root, create the Model scope (Xform if needed)
    pxr::UsdPrim modelPrim;
    bool needsRot = (pxr::UsdGeomGetStageUpAxis(srcStage) == pxr::UsdGeomTokens->z);
    if (needsRot)
    {
        modelPrim = stage->DefinePrim(pxr::SdfPath("/World/Model"), pxr::TfToken("Xform"));
        pxr::UsdGeomXformable xf(modelPrim);
        auto rot = xf.AddXformOp(pxr::UsdGeomXformOp::TypeRotateX);
        rot.Set(-90.0, pxr::UsdTimeCode::Default());
    }
    else
    {
        modelPrim = stage->DefinePrim(pxr::SdfPath("/World/Model"), pxr::TfToken("Scope"));
    }

    // Reference the scene under /World/Model
    modelPrim.GetReferences().AddReference(filename);

    // Use the new stage
    m_stage = stage;

    // Reset camera position
    glm::vec3 minBounds, maxBounds;
    ComputeSceneBounds(m_stage, minBounds, maxBounds);
    m_camera.ResetToModel(minBounds, maxBounds);

    // Reset Hydra engine and HgiInterop
    InitHydra();
}

void Application::InitHydra()
{
    // Initialize Engine and HgiInterop
    m_engine.reset(new pxr::UsdImagingGLEngine());
    m_hgiInterop.reset(new pxr::HgiInterop());

    std::cout << "Renderer plugin: " << m_engine->GetCurrentRendererId() << std::endl;
    std::cout << "Renderer HGI backend: " << m_engine->GetRendererHgiDisplayName() << std::endl;

    // Setup lighting
    if (m_domeLightTexture.empty())
    {
        SetupDefaultLighting();
    }
    else
    {
        SetupDomeLight();
    }
}

void Application::SetupDefaultLighting()
{
    // Setup default lighting

    pxr::GlfSimpleLightVector lights;

    // === Key Light:
    {
        pxr::GlfSimpleLight key;
        key.SetPosition(pxr::GfVec4f(800.0f, 200.0f, 800.0f, 1.0f));

        pxr::GfVec4f warmSRGB{1.0f,0.95f,0.9f,1.0f};
        key.SetDiffuse(0.8f * ToLinear(warmSRGB));
        key.SetAmbient(ToLinear({0.05f, 0.05f, 0.05f, 1.0f}));
        key.SetSpecular(ToLinear({0.8f, 0.8f, 0.8f, 1.0f}));
        key.SetSpotDirection(pxr::GfVec3f(0, 0, -1));
        lights.push_back(key);
    }

    // === Fill Light:
    {
        pxr::GlfSimpleLight fill;
        fill.SetPosition(pxr::GfVec4f(-600.0f, 100.0f, 900.0f, 1.0f));

        fill.SetDiffuse(ToLinear({0.6f, 0.7f, 1.0f, 1.0f}));
        fill.SetAmbient(ToLinear({0.05f, 0.05f, 0.05f, 1.0f}));
        fill.SetSpecular(ToLinear({0.5f, 0.5f, 0.5f, 1.0f}));
        fill.SetSpotDirection(pxr::GfVec3f(0, 0, -1));
        lights.push_back(fill);
    }

    // === Back Light:
    {
        pxr::GlfSimpleLight back;
        back.SetPosition(pxr::GfVec4f(0.0f, 300.0f, -400.0f, 1.0f));

        back.SetDiffuse(ToLinear({0.3f, 0.3f, 0.4f, 1.0f}));
        back.SetAmbient(ToLinear({0.02f, 0.02f, 0.02f, 1.0f}));
        back.SetSpecular(ToLinear({0.3f, 0.3f, 0.3f, 1.0f}));
        back.SetSpotDirection(pxr::GfVec3f(0, 0, 1));
        lights.push_back(back);
    }

    // Fallback material
    pxr::GlfSimpleMaterial material;
    material.SetAmbient(ToLinear({0.1f, 0.1f, 0.1f, 1.0f}));
    material.SetSpecular(ToLinear({0.6f, 0.6f, 0.6f, 0.6f}));
    material.SetShininess(16.0);

    // Ambient light
    pxr::GfVec4f sceneAmbient = ToLinear({0.1f, 0.1f, 0.1f, 1.0f});

    // Update renderer
    m_engine->SetLightingState(lights, material, sceneAmbient);
}

void Application::SetupDomeLight()
{
    // Setup dome light
    pxr::UsdLuxDomeLight domeLight = pxr::UsdLuxDomeLight::Define(m_stage, pxr::SdfPath("/World/DomeLight"));
    domeLight.CreateTextureFileAttr().Set(pxr::SdfAssetPath(m_domeLightTexture));
}