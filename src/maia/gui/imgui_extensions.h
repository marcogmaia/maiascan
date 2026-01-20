// Copyright (c) Maia

#pragma once

namespace maia {

// Manages the lifecycle of the GUI and Windowing system (GLFW/ImGui).
//
// This class uses RAII to initialize the window and ImGui context on
// construction and clean them up on destruction. It provides methods to handle
// the main render loop and window events.
class GuiSystem {
 public:
  // Initializes the windowing system and ImGui context.
  //
  // Sets up GLFW, creates a window, initializes OpenGL loader, and configures
  // ImGui style and backends. Check IsValid() after construction to ensure
  // success.
  GuiSystem();

  // Shuts down the windowing system and ImGui context.
  ~GuiSystem();

  // Non-copyable and non-movable to prevent multiple ownership of the window
  // handle.
  GuiSystem(const GuiSystem&) = delete;
  GuiSystem& operator=(const GuiSystem&) = delete;
  GuiSystem(GuiSystem&&) = delete;
  GuiSystem& operator=(GuiSystem&&) = delete;

  // Starts a new ImGui frame.
  //
  // Should be called at the beginning of the render loop iteration.
  void BeginFrame();

  // Ends the current ImGui frame and renders draw data.
  //
  // Should be called after all ImGui commands are issued and before
  // SwapBuffers.
  void EndFrame();

  // Window management

  // Checks if the window close flag has been set.
  // Returns true if the window should close, false otherwise.
  bool WindowShouldClose() const;

  // Processes pending events.
  void PollEvents();

  // Swaps the front and back buffers.
  void SwapBuffers();

  // Clears the window background with the specified color.
  // r: Red component (0.0 - 1.0)
  // g: Green component (0.0 - 1.0)
  // b: Blue component (0.0 - 1.0)
  // a: Alpha component (0.0 - 1.0)
  void ClearWindow(float r, float g, float b, float a = 1.0f);

  // Gets the raw window handle.
  // Returns void* pointer to the underlying window handle (GLFWwindow*).
  void* window_handle() const {
    return window_handle_;
  }

  // Checks if the system was initialized successfully.
  // Returns true if initialized, false otherwise.
  bool IsValid() const {
    return window_handle_ != nullptr;
  }

 private:
  void* window_handle_{nullptr};
};

}  // namespace maia
