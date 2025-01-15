#include "GW2Viewer.h"
#include "DatFile.h"

static GLuint framebuffer = 0, texture = 0, depthbuffer = 0;
static int fb_width = 0, fb_height = 0;
static int find_number = 0;
static int temp_number = 0;
// Read the decompressed data buffer once
static std::vector<uint8_t> decompressed_data;
static std::vector<uint8_t> compressed_data;

static int image_width = 0, image_height = 0, image_channels = 0;
static unsigned char* image_data = nullptr;
static GLuint texture_id = 0;


GLuint shaderProgram;
GLuint VAO, VBO;
static std::vector<uint32_t> found_results;
std::chrono::high_resolution_clock::time_point lastFrameTime = std::chrono::high_resolution_clock::now();
float frameTime = 0.0f;
int frameCount = 0;
float fps = 0.0f;


class Application {
public:
	Application(int width, int height, const char* title)
		: window_width(width),
		window_height(height),
		window_title(title),
		clear_color(0.45f, 0.55f, 0.60f, 1.00f),
		status_message_timer(0.0f),
		status_message(""),
		last_selected_item_decompressed(-1),
		last_selected_item(-1),
		selected_item(-1),
		last_x(0), last_y(0), camera_zoom(45.0f), camera_angle_x(0.0f), camera_angle_y(0.0f) {

		initGLFW();
		createWindow();
		initGLAD();
		setupShaders();
		setupCube();
		initImGui();
		loadFile();  // Load the DAT file once when the application starts
	}

	~Application() {
		cleanup();
	}

	void run() {

		while (!glfwWindowShouldClose(context_window.get())) {
			glfwPollEvents();
			if (glfwGetWindowAttrib(context_window.get(), GLFW_ICONIFIED) != 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			renderFrame();
		}
	}

private:
	int window_width, window_height;
	const char* window_title;
	ImVec4 clear_color;
	std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> context_window{ nullptr, glfwDestroyWindow };
	std::unique_ptr<DatFile> dat_file; // Pointer to a DatFile object
	float status_message_timer;
	std::string status_message;
	int last_selected_item_decompressed;
	int last_selected_item;
	int selected_item;

	// Camera control variables
	float camera_zoom;
	float camera_angle_x, camera_angle_y;
	double last_x, last_y;

	void loadFile() {
		//std::string file_path = "Local.dat";
		std::string file_path = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Guild Wars 2\\Gw2.dat";

		try {
			dat_file = std::make_unique<DatFile>(file_path);
			std::cout << "Loaded DAT file: " << file_path << '\n';
		}
		catch (const std::exception& e) {
			std::cerr << "Failed to load DAT file: " << e.what() << '\n';
		}
	}

	void initGLFW() {
		glfwSetErrorCallback([](int error, const char* description) {
			std::cerr << "GLFW Error " << error << ": " << description << '\n';
			});

		if (!glfwInit()) {
			throw std::runtime_error("Failed to initialize GLFW");
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	}

	void initGLAD() {
		// Load OpenGL with Glad
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			throw std::runtime_error("Failed to initialize GLAD");

		}
	}

	void createWindow() {
		context_window.reset(glfwCreateWindow(window_width, window_height, window_title, nullptr, nullptr));
		if (!context_window) {
			throw std::runtime_error("Failed to create GLFW window");
		}

		glfwMakeContextCurrent(context_window.get());
		glfwSwapInterval(1); // Enable vsync
		// Set the user pointer to the Application instance
		glfwSetWindowUserPointer(context_window.get(), this);

		// Set mouse position callback
		glfwSetCursorPosCallback(context_window.get(), [](GLFWwindow* window, double xpos, double ypos) {
			Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
				double delta_x = xpos - app->last_x;
				double delta_y = ypos - app->last_y;

				app->camera_angle_x += delta_y * 0.1f;
				app->camera_angle_y += delta_x * 0.1f;
			}
			app->last_x = xpos;
			app->last_y = ypos;
			});

		// Set scroll callback
		glfwSetScrollCallback(context_window.get(), [](GLFWwindow* window, double xoffset, double yoffset) {
			Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
			app->camera_zoom -= (float)yoffset * 2.0f;
			if (app->camera_zoom < 10.0f) app->camera_zoom = 10.0f;
			if (app->camera_zoom > 45.0f) app->camera_zoom = 45.0f;
			});
	}

	void initImGui() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		//io.IniFilename = nullptr; // Disable .ini file saving

		ImGui::StyleColorsDark();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOpenGL(context_window.get(), true);
		ImGui_ImplOpenGL3_Init("#version 130");
	}

	void renderFrame() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		renderUI();

		ImGui::Render();

		int display_w, display_h;
		glfwGetFramebufferSize(context_window.get(), &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_context);
		}

		glfwSwapBuffers(context_window.get());
	}

	void renderUI() {
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// Set up a full-screen window with docking
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::Begin("DockSpace Demo", nullptr, window_flags);
		ImGui::PopStyleVar(2);

		// Dockspace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		else {
			ImGui::Text("Docking is not enabled! Enable it in ImGuiIO.");
		}
		RenderMenuBar();

		ImGui::End(); // End the main window

		// Panels
		renderLeftPanel();
		renderMiddlePanel();
		renderRightPanel();
	}

	// Function to set a dark theme
	void SetDarkTheme() {
		ImGui::StyleColorsDark();
	}

	// Function to set a light theme
	void SetLightTheme() {
		ImGui::StyleColorsLight();
	}


	void RenderMenuBar() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Themes")) {
				if (ImGui::MenuItem("Dark Theme")) {
					SetDarkTheme();
				}
				if (ImGui::MenuItem("Light Theme")) {
					SetLightTheme();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}



	void renderLeftPanel() {
		ImGui::Begin("MFT Data");

		//Bugged because its gonna refresh per frame each time want to find
		//// Fixed search bar at the top
		ImGui::Text("Search Bar:");
		ImGui::InputInt("##SearchBar", &find_number);
		ImGui::Separator();

		ImGui::Text("MFT Data List:");

		// Create a scrollable child window for the list
		ImVec2 child_size = ImVec2(0, 0); // Adjust height as needed
		ImGui::BeginChild("MFTList", child_size, true, ImGuiWindowFlags_HorizontalScrollbar);

		if (find_number > 0)
		{

			if (temp_number != find_number)
			{
				found_results = dat_file->findMftData(find_number, true);
				temp_number = find_number;
			}


			size_t total_items = found_results.size();
			// Use ImGuiListClipper for efficient rendering of large lists
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(total_items)); // Total number of items in the list

			while (clipper.Step()) {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
					if (ImGui::Selectable(("MFT Entry " + std::to_string(found_results[i])).c_str(), selected_item == found_results[i])) {
						selected_item = found_results[i];
					}
				}
			}

			clipper.End();
		}

		if (find_number == 0)
		{

			const auto& mft_data = dat_file->getMftData();
			size_t total_items = mft_data.size();

			// Use ImGuiListClipper for efficient rendering of large lists
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(total_items)); // Total number of items in the list

			while (clipper.Step()) {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
					if (ImGui::Selectable(("MFT Entry " + std::to_string(i)).c_str(), selected_item == i)) {
						selected_item = i;
					}
				}
			}

			clipper.End();

		}
		ImGui::EndChild();

		ImGui::End();
	}

	void renderCompressedTab() {
		if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
			const auto& selected_entry = dat_file->getMftData()[selected_item];

			// Read the compressed data buffer once
			if (selected_item != last_selected_item) {
				compressed_data = dat_file->readCompressedData(selected_entry);
				last_selected_item = selected_item;
			}

			// Display compressed data
			ImGui::Text("Compressed Data (Hex):");
			ImGui::BeginChild("Compressed Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(compressed_data.size() / 16 + 1));

			while (clipper.Step()) {
				for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
					uint64_t start_idx = static_cast<uint64_t>(line) * 16;
					std::string line_str;

					// Line number
					char line_number[16];
					snprintf(line_number, sizeof(line_number), "%08X: ", (unsigned int)start_idx);
					line_str += line_number;

					// Hexadecimal bytes
					for (uint64_t j = 0; j < 16; ++j) {
						if (start_idx + j < compressed_data.size()) {
							char hex_byte[4];
							snprintf(hex_byte, sizeof(hex_byte), "%02X ", compressed_data[start_idx + j]);
							line_str += hex_byte;
						}
						else {
							line_str += "   ";
						}
					}

					// ASCII representation
					line_str += " ";
					for (uint64_t j = 0; j < 16; ++j) {
						if (start_idx + j < compressed_data.size()) {
							char c = compressed_data[start_idx + j];
							line_str += (c >= 32 && c <= 126) ? c : '.';
						}
						else {
							line_str += " ";
						}
					}

					ImGui::TextUnformatted(line_str.c_str());
				}
			}

			clipper.End();
			ImGui::EndChild();
		}
	}

	void renderDecompressedTab() {
		if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
			const auto& selected_entry = dat_file->getMftData()[selected_item];

			// Read the decompressed data buffer once
			if (selected_item != last_selected_item_decompressed) {
				decompressed_data = dat_file->removeCrc32Data(selected_entry);
				dat_file->updateUncompressedSize(selected_item, decompressed_data.size());
				last_selected_item_decompressed = selected_item;
			}
			// Display decompressed data
			ImGui::Text("Decompressed Data (Hex):");
			ImGui::BeginChild("Decompressed Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(decompressed_data.size() / 16 + 1));

			while (clipper.Step()) {
				for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
					uint64_t start_idx = static_cast<uint64_t>(line) * 16;
					std::string line_str;

					// Line number
					char line_number[16];
					snprintf(line_number, sizeof(line_number), "%08X: ", (unsigned int)start_idx);
					line_str += line_number;

					// Hexadecimal bytes
					for (uint64_t j = 0; j < 16; ++j) {
						if (start_idx + j < decompressed_data.size()) {
							char hex_byte[4];
							snprintf(hex_byte, sizeof(hex_byte), "%02X ", decompressed_data[start_idx + j]);
							line_str += hex_byte;
						}
						else {
							line_str += "   ";
						}
					}

					// ASCII representation
					line_str += " ";
					for (uint64_t j = 0; j < 16; ++j) {
						if (start_idx + j < decompressed_data.size()) {
							char c = decompressed_data[start_idx + j];
							line_str += (c >= 32 && c <= 126) ? c : '.';
						}
						else {
							line_str += " ";
						}
					}

					ImGui::TextUnformatted(line_str.c_str());
				}
			}

			clipper.End();
			ImGui::EndChild();
		}
	}

	void createFramebuffer(GLuint& framebuffer, GLuint& texture, GLuint& depthbuffer, int width, int height) {
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		// Create texture
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		// Create depth buffer
		glGenRenderbuffers(1, &depthbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			throw std::runtime_error("Framebuffer not complete!");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}



	void setupShaders() {
		const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;

        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

		const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;

        void main() {
            FragColor = vec4(0.8, 0.5, 0.5, 1.0); // Red color
        }
    )";

		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
		glCompileShader(vertexShader);

		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
		glCompileShader(fragmentShader);

		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	}

	void setupCube() {
		float vertices[] = {
			// Positions
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f
		};

		unsigned int indices[] = {
			0, 1, 2, 2, 3, 0, // Back face
			4, 5, 6, 6, 7, 4, // Front face
			0, 1, 5, 5, 4, 0, // Bottom face
			2, 3, 7, 7, 6, 2, // Top face
			0, 3, 7, 7, 4, 0, // Left face
			1, 2, 6, 6, 5, 1  // Right face
		};

		GLuint EBO;
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void renderToFramebuffer(GLuint framebuffer, int width, int height, float camera_angle_x, float camera_angle_y, float camera_zoom) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(0, 0, width, height);

		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 projection = glm::perspective(glm::radians(camera_zoom), (float)width / (float)height, 0.1f, 100.0f);
		glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		view = glm::rotate(view, glm::radians(camera_angle_x), glm::vec3(1.0f, 0.0f, 0.0f));
		view = glm::rotate(view, glm::radians(camera_angle_y), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 model = glm::mat4(1.0f);

		glUseProgram(shaderProgram);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void renderPreviewTab() {
		if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
			const auto& selected_entry = dat_file->getMftData()[selected_item];

			// Display preview data
			ImGui::Text("Preview Data:");
			std::string file_type = "Image";



			if (selected_item != last_selected_item_decompressed) {
				// Decompress the data and update uncompressed size
				decompressed_data = dat_file->removeCrc32Data(selected_entry);
				dat_file->updateUncompressedSize(selected_item, decompressed_data.size());
				last_selected_item_decompressed = selected_item;
			}

			if (file_type == "Image")
			{

				// Free the previous image data if it exists
				if (image_data) {
					stbi_image_free(image_data);
					image_data = nullptr;
				}

				// Attempt to load the decompressed data as an image
				image_data = stbi_load_from_memory(
					decompressed_data.data(),
					static_cast<int>(decompressed_data.size()),
					&image_width,
					&image_height,
					&image_channels,
					0 // Keep original channels
				);

				if (image_data) {
					ImGui::Text("Image loaded successfully!");
					ImGui::Text("Dimensions: %dx%d, Channels: %d", image_width, image_height, image_channels);
				}
				else {
					ImGui::Text("Failed to load image. Unsupported format or corrupted data.");
				}

				// If the image is successfully loaded, display it
				if (image_data) {

					// Create an OpenGL texture for displaying the image
					if (texture_id == 0) {
						glGenTextures(1, &texture_id);
					}

					glBindTexture(GL_TEXTURE_2D, texture_id);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0,
						(image_channels == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, image_data);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

					// Display the texture in ImGui
					ImGui::Image((ImTextureID)texture_id,
						ImVec2(image_width, image_height));
				}
			}

			if (file_type == "Model3D")
			{

				// Reserve space for FPS and other info (e.g., 90px for FPS display)
				float textHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing();
				float fpsInfoHeight = textHeightWithSpacing * 5;

				// Begin child window for preview with adjusted height
				ImGui::BeginChild("Preview Scroll", ImVec2(0, -fpsInfoHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

				ImVec2 panel_size = ImGui::GetContentRegionAvail();
				int new_width = static_cast<int>(panel_size.x);
				int new_height = static_cast<int>(panel_size.y);

				if (new_width != fb_width || new_height != fb_height) {
					// Resize framebuffer if size changes
					fb_width = new_width;
					fb_height = new_height;

					if (framebuffer != 0) {
						glDeleteFramebuffers(1, &framebuffer);
						glDeleteTextures(1, &texture);
						glDeleteRenderbuffers(1, &depthbuffer);
					}

					createFramebuffer(framebuffer, texture, depthbuffer, fb_width, fb_height);
				}

				std::string fileType = "Model3D";
				if (fileType == "Model3D") {
					// Calculate FPS and frame time
					auto currentFrameTime = std::chrono::high_resolution_clock::now();
					std::chrono::duration<float> deltaTime = currentFrameTime - lastFrameTime;
					lastFrameTime = currentFrameTime;
					frameTime = deltaTime.count();
					frameCount++;

					if (frameCount >= 60) { // Update FPS every 60 frames
						fps = 1.0f / frameTime;
						frameCount = 0;
					}

					// Render to framebuffer
					renderToFramebuffer(framebuffer, fb_width, fb_height, camera_angle_x, camera_angle_y, camera_zoom);

					// Display framebuffer texture in ImGui
					ImGui::Image((ImTextureID)texture, ImVec2(fb_width, fb_height), ImVec2(0, 1), ImVec2(1, 0));

				}

				ImGui::EndChild();

				// Calculate model statistics
				int polygonCount = 12; // 6 faces * 2 triangles per face
				int edgeCount = 24; // 12 edges * 2 vertices per edge
				int vertexCount = 36; // 6 faces * 2 triangles * 3 vertices per triangle


				// Display FPS, frame time, and vertex count
				ImGui::Text("FPS: %.2f", fps);
				ImGui::Text("Frame Time: %.3f ms", frameTime * 1000.0f);

				// Display model statistics
				ImGui::Text("Polygon Count: %d", polygonCount);
				ImGui::Text("Edge Count: %d", edgeCount);
				ImGui::Text("Vertex Count: %d", vertexCount);
			}

		}
	}





	void renderMiddlePanel() {
		ImGui::Begin("Extracted Data");

		if (ImGui::BeginTabBar("MFT Data Tabs")) {
			if (ImGui::BeginTabItem("Compressed")) {
				renderCompressedTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Decompressed")) {
				renderDecompressedTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Preview")) {
				renderPreviewTab();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}



	void renderFileHeaderInformation() {
		const auto& header = dat_file->getHeader();
		const auto& mft_header = dat_file->getMftHeader();

		ImGui::Text("Filename: %s", dat_file->getFilename().c_str());
		ImGui::Text("File Size: %llu bytes", dat_file->getFileSize());
		ImGui::Text("Version: %d", header.version);
		ImGui::Text("Chunk Size: %u bytes", header.chunk_size);
		ImGui::Text("MFT Offset: %llu", header.mft_offset);
		ImGui::Text("MFT Size: %u bytes", header.mft_size);

		ImGui::Separator();
		ImGui::Text("MFT Header:");
		ImGui::Text("Identifier: %.*s", MFT_MAGIC_NUMBER, mft_header.identifier);
		ImGui::Text("Unknown Field: %llu", mft_header.unknown_field);
		ImGui::Text("Entry Count: %u", mft_header.mft_entry_size);
	}


	void renderSelectedMftEntryInformation() {
		const auto& selected_entry = dat_file->getMftData()[selected_item];

		ImGui::Separator();
		ImGui::Text("Selected MFT Entry:");
		ImGui::Text("Offset: %llu", selected_entry.offset);
		ImGui::Text("Size: %u bytes", selected_entry.size);
		ImGui::Text("Compression Flag: %u", selected_entry.compression_flag);
		ImGui::Text("Entry Flag: %u", selected_entry.entry_flag);
		ImGui::Text("Counter: %u", selected_entry.counter);
		ImGui::Text("CRC: %u", selected_entry.crc);
		ImGui::Text("Uncompressed Size: %u", selected_entry.uncompressed_size);

		if (ImGui::Button("Export Compressed Data")) {
			try {
				compressed_data = dat_file->readCompressedData(selected_entry);
				std::string filename = "compressed_" + std::to_string(selected_item) + ".bin";
				exportDataToFile(filename, compressed_data);
				status_message = "Compressed data exported to " + filename;
				status_message_timer = 3.0f; // Show the message for 3 seconds
			}
			catch (const std::exception& e) {
				status_message = std::string("Error: ") + e.what();
				status_message_timer = 3.0f;
			}
		}

		if (ImGui::Button("Export Decompressed Data")) {
			try {

				decompressed_data = dat_file->removeCrc32Data(selected_entry);
				std::string filename = "decompressed_" + std::to_string(selected_item) + ".bin";
				exportDataToFile(filename, decompressed_data);
				status_message = "Decompressed data exported to " + filename;
				status_message_timer = 5.0f;
			}
			catch (const std::exception& e) {
				status_message = std::string("Error: ") + e.what();
				status_message_timer = 5.0f;
			}
		}
	}

	void renderStatusMessage() {
		if (!status_message.empty() && status_message_timer > 0.0f) {
			ImGui::Separator();
			ImGui::Text("%s", status_message.c_str());
			status_message_timer -= ImGui::GetIO().DeltaTime; // Decrease the timer
			if (status_message_timer <= 0.0f) {
				status_message.clear(); // Clear the message when the timer expires
			}
		}
	}


	void renderRightPanel() {
		ImGui::Begin("File Information");
		ImGui::Text("DAT File Information:");
		ImGui::Separator();

		if (dat_file) {
			ImGui::PushTextWrapPos();

			// Render file header information
			renderFileHeaderInformation();

			// Render selected MFT entry information if an entry is selected
			if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
				renderSelectedMftEntryInformation();
			}
			else {
				ImGui::Text("No MFT entry selected.");
			}

			ImGui::PopTextWrapPos();
		}
		else {
			ImGui::Text("No DAT file loaded.");
		}

		// Render status message
		ImGui::PushTextWrapPos();
		renderStatusMessage();
		ImGui::PopTextWrapPos();

		ImGui::End();
	}


	void exportDataToFile(const std::string& filename, const std::vector<uint8_t>& data) {
		std::ofstream file(filename, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open file for writing: " + filename);
		}
		file.write(reinterpret_cast<const char*>(data.data()), data.size());
		file.close();
	}



	void cleanup() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwDestroyWindow(context_window.get());
		glfwTerminate();
	}
};

int main() {
	try {
		Application app(1280, 720, "Guild Wars 2 Viewer");
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Application error: " << e.what() << '\n';
		return 1;
	}

	return 0;
}
