#pragma once

// Standard Library Headers
#include <array>
#include <chrono>
#include <cstdio>

// Third-Party Library Headers
#include <GLFW/glfw3.h>

// FpsCounter Class
class FpsCounter
{
  public:
    FpsCounter() = default;

    /// Call once per frame; will update the window title every every `intervalSec` seconds.
    void tick(GLFWwindow *window, double intervalSec = 1.0) noexcept
    {
        using clock = std::chrono::steady_clock;
        ++m_frames;

        auto now = clock::now();
        std::chrono::duration<double> elapsed = now - m_lastTime;
        if (elapsed.count() < intervalSec)
        {
            return;
        }

        double fps = m_frames / elapsed.count();
        std::array<char, 128> buf;
        std::snprintf(buf.data(), buf.size(), "USD Viewer — %.1f FPS", fps);
        glfwSetWindowTitle(window, buf.data());

        // Reset for next interval
        m_frames = 0;
        m_lastTime = now;
    }

  private:
    typedef std::chrono::steady_clock::time_point time_point;

    time_point m_lastTime{std::chrono::steady_clock::now()};
    size_t m_frames{0};
};
