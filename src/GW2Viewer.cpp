#include "GW2Viewer.h"
#include "DatFile.h"

static int selected_item = -1;  // Declare globally or as static to retain value across frames
static char search_query[256] = "";
static int last_selected_item_decompressed = -1;
static int last_selected_item = -1;
// Add a member variable to store the status message and a timer
std::string status_message;
float status_message_timer = 0.0f;


class Application {
public:
	Application(int width, int height, const char* title)
		: window_width(width), window_height(height), window_title(title), clear_color(0.45f, 0.55f, 0.60f, 1.00f) {
		initGLFW();
		createWindow();
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

	void loadFile() {
		// Error : Load large file because didnt yet implementing  virtual list
		std::string file_path = "Local.dat";
		//std::string file_path = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Guild Wars 2\\Gw2.dat";

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

	void createWindow() {
		context_window.reset(glfwCreateWindow(window_width, window_height, window_title, nullptr, nullptr));
		if (!context_window) {
			throw std::runtime_error("Failed to create GLFW window");
		}

		glfwMakeContextCurrent(context_window.get());
		glfwSwapInterval(1); // Enable vsync
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
		ImGui::Begin("Left Panel");

		//Bugged because its gonna refresh per frame each time want to find
		//// Fixed search bar at the top
		//ImGui::Text("Search Bar:");
		//ImGui::InputText("##SearchBar", search_query, sizeof(search_query));
		//ImGui::Separator();

		ImGui::Text("MFT Data List:");

		// Create a scrollable child window for the list
		ImVec2 child_size = ImVec2(0, 0); // Adjust height as needed
		ImGui::BeginChild("MFTList", child_size, true, ImGuiWindowFlags_HorizontalScrollbar);

		// Convert search query to lowercase for case-insensitive search
		std::string query = search_query;
		std::transform(query.begin(), query.end(), query.begin(), ::tolower);

		const auto& mft_data = dat_file->getMftData();
		size_t total_items = mft_data.size();

		// Use ImGuiListClipper for efficient rendering of large lists
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(total_items)); // Total number of items in the list

		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
				const auto& mft_entry = mft_data[i];

				// Convert the MFT entry information to lowercase for comparison
				std::string mft_entry_str = std::to_string(i); // Adjust this to relevant fields in the MFT entry
				std::transform(mft_entry_str.begin(), mft_entry_str.end(), mft_entry_str.begin(), ::tolower);

				// Check if the MFT entry matches the search query
				if (query.empty() || mft_entry_str.find(query) != std::string::npos) {
					if (ImGui::Selectable(("MFT Entry " + std::to_string(i)).c_str(), selected_item == i)) {
						selected_item = i;
					}
				}
			}
		}

		clipper.End();

		ImGui::EndChild();

		ImGui::End();
	}


	void renderMiddlePanel() {
		ImGui::Begin("Middle Panel");

		// Create a tab bar with 3 tabs: Compressed, Decompressed, Preview
		if (ImGui::BeginTabBar("MFT Data Tabs")) {

			// Compressed Data Tab
			if (ImGui::BeginTabItem("Compressed")) {
				if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
					const auto& selected_entry = dat_file->getMftData()[selected_item];

					// Read the compressed data buffer once, only if the selected item has changed
					static std::vector<uint8_t> compressed_data;
					if (selected_item != last_selected_item) {
						compressed_data = dat_file->readCompressedData(selected_entry);
						last_selected_item = selected_item;
					}

					// Display compressed data in hexadecimal format using ImGuiListClipper
					ImGui::Text("Compressed Data (Hex):");
					ImGui::BeginChild("Compressed Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

					ImGuiListClipper clipper;
					clipper.Begin(static_cast<int>(compressed_data.size() / 16 + 1)); // Calculate number of lines

					while (clipper.Step()) {
						for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
							uint64_t start_idx = line * 16;
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
				ImGui::EndTabItem();
			}

			// Decompressed Data Tab
			if (ImGui::BeginTabItem("Decompressed")) {
				if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
					const auto& selected_entry = dat_file->getMftData()[selected_item];

					// Read the decompressed data buffer once, only if the selected item has changed
					static std::vector<uint8_t> decompressed_data;
					if (selected_item != last_selected_item_decompressed) {
						//decompressed_data = dat_file->readDecompressedData(selected_entry);
						std::vector<uint8_t> decompressed_data = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

						last_selected_item_decompressed = selected_item;
					}

					// Display decompressed data in hexadecimal format using ImGuiListClipper
					ImGui::Text("Decompressed Data (Hex):");
					ImGui::BeginChild("Decompressed Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

					ImGuiListClipper clipper;
					clipper.Begin(static_cast<int>(decompressed_data.size() / 16 + 1)); // Calculate number of lines

					while (clipper.Step()) {
						for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
							uint64_t start_idx = line * 16;
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
				ImGui::EndTabItem();
			}

			// Preview Tab
			if (ImGui::BeginTabItem("Preview")) {
				if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
					const auto& selected_entry = dat_file->getMftData()[selected_item];

					// Preview data (e.g., as text, image, etc.)
					ImGui::Text("Preview Data:");
					ImGui::BeginChild("Preview Scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

					// Replace with actual preview data logic
					//std::vector<uint8_t> preview_data = dat_file->readPreviewData(selected_entry);
					std::vector<uint8_t> preview_data = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

					for (uint64_t i = 0; i < std::min(preview_data.size(), uint64_t(64)); ++i) {
						ImGui::Text("%c", preview_data[i]);
					}

					ImGui::EndChild();
				}
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}



	// Update the status message in the render loop
	void renderRightPanel() {
		ImGui::Begin("Right Panel");
		ImGui::Text("DAT File Information:");
		ImGui::Separator();

		if (dat_file) {
			ImGui::PushTextWrapPos();

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
			ImGui::PushTextWrapPos();

			if (selected_item >= 0 && selected_item < dat_file->getMftData().size()) {
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
						std::vector<uint8_t> compressed_data = dat_file->readCompressedData(selected_entry);
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
						std::vector<uint8_t> decompressed_data = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
						std::string filename = "decompressed_" + std::to_string(selected_item) + ".bin";
						exportDataToFile(filename, decompressed_data);
						status_message = "Decompressed data exported to " + filename;
						status_message_timer = 3.0f;
					}
					catch (const std::exception& e) {
						status_message = std::string("Error: ") + e.what();
						status_message_timer = 3.0f;
					}
				}
			}
			else {
				ImGui::Text("No MFT entry selected.");
			}

			ImGui::PopTextWrapPos();
		}
		else {
			ImGui::Text("No DAT file loaded.");
		}

		// Display the status message
		if (!status_message.empty() && status_message_timer > 0.0f) {
			ImGui::Separator();
			ImGui::Text("%s", status_message.c_str());
			status_message_timer -= ImGui::GetIO().DeltaTime; // Decrease the timer
			if (status_message_timer <= 0.0f) {
				status_message.clear(); // Clear the message when the timer expires
			}
		}

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
