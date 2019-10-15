import glfw
from render import gl_render

def main():
    # Initialize the library
    if not glfw.init():
        return
    # Create a windowed mode window and its OpenGL context
    window = glfw.create_window(1000, 800, "Hello World", None, None)
    #window = glfw.create_window(1200, 1000, "Hello World", None, None)
    if not window:
        glfw.terminate()
        return

    # Make the window's context current
    glfw.make_context_current(window)

    # Loop until the user closes the window
    while not glfw.window_should_close(window):
        # Render here, e.g. using pyOpenGL
        gl_render(None,None)

        # Swap front and back buffers
        glfw.swap_buffers(window)

        # Poll for and process events
        #glfw.poll_events()

    glfw.terminate()

main()
