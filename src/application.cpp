// Standard Library Headers
#include <algorithm>
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

    // Initialize the renderer and HgiInterop
    m_renderer = std::make_unique<pxr::UsdImagingGLEngine>();
    m_hgiInterop = std::make_unique<pxr::HgiInterop>();

    pxr::TfToken apiName = m_renderer->GetHgi()->GetAPIName();
    std::cout << "Storm Hgi Backend: " << apiName << "\n";

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
    LoadScene(filename);
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(m_window) && !m_quitApp)
    {
        glfwPollEvents();

        ProcessFrame();
    }

    // Destroy the renderer and flush the GL pipeline
    m_renderer.reset();
    m_hgiInterop.reset();
    glFinish();

    // Destroy the window and terminate GLFW
    glfwDestroyWindow(m_window);
    m_window = nullptr;
}

void Application::ProcessFrame()
{
    CHECK_GL_ERROR(__LINE__);

    // Update camera
    pxr::GfMatrix4d viewMatrix = ToGfMatrix(m_camera.GetViewMatrix());
    pxr::GfMatrix4d projMatrix = ToGfMatrix(m_camera.GetProjectionMatrix());
    m_renderer->SetCameraState(viewMatrix, projMatrix);

    // Update viewport and render buffer size
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    m_renderer->SetRenderViewport(pxr::GfVec4d(0, 0, m_framebufferWidth, m_framebufferHeight));
    m_renderer->SetRenderBufferSize(pxr::GfVec2i(m_framebufferWidth, m_framebufferHeight));
    m_renderer->SetWindowPolicy(pxr::CameraUtilConformWindowPolicy::CameraUtilFit);
    m_renderer->SetRendererAov(pxr::HdAovTokens->color);

    // Clear the screen
    const float r = 85 / 255.0f;
    const float g = 134 / 255.0f;
    const float b = 165 / 255.0f;
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    CHECK_GL_ERROR(__LINE__);

    // Init render params
    pxr::UsdImagingGLRenderParams renderParams{};
    renderParams.drawMode = pxr::UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    renderParams.enableLighting = true;
    renderParams.frame = 0;
    renderParams.complexity = 1;
    renderParams.cullStyle = pxr::UsdImagingGLCullStyle::CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED;
    renderParams.enableSceneMaterials = false;
    renderParams.highlight = true;
    renderParams.clearColor = pxr::GfVec4f(r, g, b, 1.0f);

    // Render the scene
    m_renderer->Render(m_stage->GetPseudoRoot(), renderParams);

    // Get the color AOV texture and transfer it to OpenGL back buffer
    pxr::HgiTextureHandle aovTexture = m_renderer->GetAovTexture(pxr::HdAovTokens->color);
    if (aovTexture)
    {
        uint32_t framebuffer = 0;
        m_hgiInterop->TransferToApp(m_renderer->GetHgi(), aovTexture,
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
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(filename);
    if (!stage)
    {
        std::cerr << "Failed to load stage: " << filename << std::endl;
        return;
    }

    // Create a new parent stage in memory
    pxr::UsdStageRefPtr parentStage = pxr::UsdStage::CreateInMemory();

    // Check if a rotation is needed based on the stage's up axis
    bool needsRotation = (pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->z);

    if (needsRotation)
    {
        // Define a root Xform to rotate the scene
        pxr::UsdPrim rootXform = parentStage->DefinePrim(pxr::SdfPath("/World"), pxr::TfToken("Xform"));
        pxr::UsdGeomXformable xformable(rootXform);
        pxr::UsdGeomXformOp rotOp = xformable.AddXformOp(pxr::UsdGeomXformOp::TypeTransform,
                                                         pxr::UsdGeomXformOp::PrecisionDouble, pxr::TfToken(), false);
        pxr::GfMatrix4d rotMatrix;
        rotMatrix.SetIdentity();
        rotMatrix.SetRotate(pxr::GfRotation(pxr::GfVec3d(1, 0, 0), -90.0));
        rotOp.Set(rotMatrix, pxr::UsdTimeCode::Default());
    }
    else
    {
        // Define a root without any transformation
        parentStage->DefinePrim(pxr::SdfPath("/World"), pxr::TfToken("Scope"));
    }

    // Under the root, define a child prim that references the original stage
    pxr::UsdPrim childPrim = parentStage->DefinePrim(pxr::SdfPath("/World/ReferencedStage"), pxr::TfToken("Scope"));
    childPrim.GetReferences().AddReference(filename);

    // Use the new stage
    m_stage = parentStage;

    // Reset camera position
    glm::vec3 minBounds, maxBounds;
    ComputeSceneBounds(m_stage, minBounds, maxBounds);
    m_camera.ResetToModel(minBounds, maxBounds);

    // Reset renderer
    m_renderer.reset();
    m_renderer = std::make_unique<pxr::UsdImagingGLEngine>();
    m_hgiInterop.reset();
    m_hgiInterop = std::make_unique<pxr::HgiInterop>();

    // Setup lighting
    SetupLighting();
}

void Application::SetupLighting()
{
    // Setup default lighting

    pxr::GlfSimpleLightVector lights;

    // === Key Light:
    {
        pxr::GlfSimpleLight key;
        key.SetPosition(pxr::GfVec4f(800.0f, 200.0f, 800.0f, 1.0f));
        key.SetDiffuse(0.8f * pxr::GfVec4f(1.0f, 0.95f, 0.9f, 1.0f)); // warm
        key.SetAmbient(pxr::GfVec4f(0.05f));
        key.SetSpecular(pxr::GfVec4f(0.8f));
        key.SetSpotDirection(pxr::GfVec3f(0, 0, -1));
        lights.push_back(key);
    }

    // === Fill Light:
    {
        pxr::GlfSimpleLight fill;
        fill.SetPosition(pxr::GfVec4f(-600.0f, 100.0f, 900.0f, 1.0f));
        fill.SetDiffuse(pxr::GfVec4f(0.6f, 0.7f, 1.0f, 1.0f)); // cooler
        fill.SetAmbient(pxr::GfVec4f(0.05f));
        fill.SetSpecular(pxr::GfVec4f(0.5f));
        fill.SetSpotDirection(pxr::GfVec3f(0, 0, -1));
        lights.push_back(fill);
    }

    // === Back Light:
    {
        pxr::GlfSimpleLight back;
        back.SetPosition(pxr::GfVec4f(0.0f, 300.0f, -400.0f, 1.0f));
        back.SetDiffuse(pxr::GfVec4f(0.3f, 0.3f, 0.4f, 1.0f));
        back.SetAmbient(pxr::GfVec4f(0.02f));
        back.SetSpecular(pxr::GfVec4f(0.3f));
        back.SetSpotDirection(pxr::GfVec3f(0, 0, 1));
        lights.push_back(back);
    }

    // Fallback material
    pxr::GlfSimpleMaterial material;
    material.SetAmbient(pxr::GfVec4f(.1f, .1f, .1f, 1.f));
    material.SetSpecular(pxr::GfVec4f(.6f, .6f, .6f, .6f));
    material.SetShininess(16.0);

    // Ambient light
    pxr::GfVec4f ambient(pxr::GfVec4f(.1f, .1f, .1f, 1.f));

    // Update renderer
    m_renderer->SetLightingState(lights, material, ambient);
}