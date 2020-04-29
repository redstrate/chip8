#include <iostream>
#include <iomanip>
#include <cstdio>
#include <climits>
#include <cmath>
#include <SDL.h>
#include <map>
#include <filesystem>
#include <vector>
#include <array>

#include "emu.hpp"
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "compiler.hpp"

constexpr std::array<uint8_t, 80> chip8_fontset = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

const std::map<SDL_Scancode, int> scancodes = {
    {SDL_SCANCODE_0, 0},
    {SDL_SCANCODE_1, 1},
    {SDL_SCANCODE_2, 2},
    {SDL_SCANCODE_3, 3},
    {SDL_SCANCODE_4, 4},
    {SDL_SCANCODE_5, 5},
    {SDL_SCANCODE_6, 6},
    
    {SDL_SCANCODE_KP_0, 0},
    {SDL_SCANCODE_KP_1, 1},
    {SDL_SCANCODE_KP_2, 2},
    {SDL_SCANCODE_KP_3, 3},
    {SDL_SCANCODE_KP_4, 4},
    {SDL_SCANCODE_KP_5, 5},
    {SDL_SCANCODE_KP_6, 6}
};

bool is_rom_open = false;

void load_rom(const char* path) {
    state.reset();
    
    memcpy(state.memory, chip8_fontset.data(), chip8_fontset.size());

    FILE* file = fopen(path, "rb");
    
    fseek(file, 0L, SEEK_END);
    auto sz = ftell(file);
    fseek(file, 0L, SEEK_SET);
    
    fread(state.memory + program_begin, sz, 1, file);
    
    is_rom_open = true;
}

std::string get_short_debug_string(uint16_t opcode) {
    if(opcode == 0)
        return "invalid opcode";
    
    const uint16_t id = opcode >> 12;
    const uint8_t nn = opcode & 0x00ff;
    
    switch(id) {
        case 0x0:
        {
            if(nn == 0xE0) {
                return "clear screen";
            }
        }
            break;
        case 0x2:
        {
            return "call subroutine";
        }
            break;
        case 0x6:
        {
            return "v[x] = nn";
        }
            break;
        case 0xD:
            return "display sprite";
    }
    
    return "";
}

GLuint quad_vao = 0;
GLuint pixel_program = 0;
GLuint pixels_texture = 0;

void setup_gfx() {
    // create quad for pixel rendering
    constexpr std::array vertices = {
        -0.5f,  0.5f, 0.0f, 0.0f, 0.0f,
        0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f
    };
    
    constexpr std::array elements = {
        0, 1, 2,
        2, 3, 0
    };
    
    glGenVertexArrays(1, &quad_vao);
    glBindVertexArray(quad_vao);
    
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    GLuint ebo = 0;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(uint32_t), elements.data(), GL_STATIC_DRAW);
    
    constexpr std::string_view vertex_glsl =
        "#version 330 core\n"
        "layout (location = 0) in vec3 in_position;\n"
        "layout (location = 1) in vec2 in_uv;\n"
        "out vec2 uv;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(in_position.xyz, 1.0);\n"
        "   uv = in_uv;\n"
        "}\n";
    const char* vertex_src = vertex_glsl.data();
    
    constexpr std::string_view fragment_glsl =
        "#version 330 core\n"
        "in vec2 uv;\n"
        "out vec4 out_color;\n"
        "uniform sampler2D pixel_texture;\n"
        "void main()\n"
        "{\n"
        "    out_color.rgb = vec3(1) * (texture(pixel_texture, uv).r * 255);\n"
        "}\n";
    const char* fragment_src = fragment_glsl.data();
    
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    glCompileShader(vertex_shader);
    
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
    glCompileShader(fragment_shader);
    
    pixel_program = glCreateProgram();
    
    glAttachShader(pixel_program, vertex_shader);
    glAttachShader(pixel_program, fragment_shader);
    glLinkProgram(pixel_program);
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    glGenTextures(1, &pixels_texture);
    glBindTexture(GL_TEXTURE_2D, pixels_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, screen_width, screen_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    SDL_Window* window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    
    gladLoadGL();
    setup_gfx();
    
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    
    std::vector<std::string> rom_paths;
    for(auto& p: std::filesystem::directory_iterator("roms/"))
        rom_paths.push_back(p.path());
    
    bool running = true;
    while(running) {
        SDL_Event event = {};
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if(event.type == SDL_QUIT)
                running = false;
            
            if(event.type == SDL_KEYDOWN) {
                if(scancodes.count(event.key.keysym.scancode))
                    state.keys[scancodes.at(event.key.keysym.scancode)] = 1;
            }
            
            if(event.type == SDL_KEYUP) {
                if(scancodes.count(event.key.keysym.scancode))
                    state.keys[scancodes.at(event.key.keysym.scancode)] = 0;
            }
        }
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        
        if(ImGui::BeginMainMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::BeginMenu("Open ROM...")) {
                    for(auto& rom : rom_paths) {
                        if(ImGui::Button(rom.c_str()))
                            load_rom(rom.c_str());
                    }
                    
                    ImGui::EndMenu();
                }
                
                if(ImGui::MenuItem("Compile & Run Program")) {
                    state.reset();
                    
                    memcpy(state.memory, chip8_fontset.data(), chip8_fontset.size());
                    
                    compile();
                    load_compiled_rom();
                    
                    is_rom_open = true;
                }
                
                ImGui::EndMenu();
            }
            
            if(ImGui::BeginMenu("Options")) {
                ImGui::MenuItem("Enable Anti-flicker", nullptr, &options.enable_anti_flicker);
                ImGui::MenuItem("Emulate Original CHIP-8", nullptr, &options.emulate_original);

                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
        
        if(state.delay_timer > 0)
            state.delay_timer--;
        
        const uint16_t opcode = (state.memory[state.PC] << 8) | state.memory[state.PC + 1];
        if(is_rom_open && !pause_execution)
            process_opcode(opcode);
            
        if(ImGui::Begin("Memory")) {
            for(int i = 0; i < 16; i++)
                ImGui::Text("V[%i] = %i", i, state.v[i]);
            
            for(int i = program_begin; i < 4096; i++)
                ImGui::Text("mem[%i] = %i", i, state.memory[i]);
        }
            
        ImGui::End();
        
        if(ImGui::Begin("Debugger")) {
            if(ImGui::Button(pause_execution ? "Play" : "Pause"))
                pause_execution = !pause_execution;
            
            ImGui::SameLine();
            
            if(ImGui::Button("Step"))
                process_opcode(opcode);
            
            static bool enable_auto_scroll = true;
            ImGui::Checkbox("Enable auto scroll", &enable_auto_scroll);
            
            ImGui::BeginChild("progam_edit", ImVec2(-1, -1), true);
            
            for(int i = program_begin; i < 4096; i += 2) {
                std::string s;
                s.reserve(50);
                
                uint16_t opcode = (state.memory[i] << 8) | state.memory[i + 1];
                auto debug_string = get_short_debug_string(opcode);

                sprintf(s.data(), "[0x%02X] 0x%04X ; %s", i, opcode, debug_string.c_str());
                
                ImGui::Selectable(s.c_str(), state.PC == i);
                
                if(state.PC == i && enable_auto_scroll && !pause_execution && is_rom_open) {
                    ImGui::SetScrollHereY();
                }
            }
            
            ImGui::EndChild();
        }
        ImGui::End();
        
        if(state.draw_dirty) {
            glBindTexture(GL_TEXTURE_2D, pixels_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, screen_width, screen_height, 0, GL_RED, GL_UNSIGNED_BYTE, state.pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            state.draw_dirty = false;
        }
                
        ImGui::Render();
        
        auto& io = ImGui::GetIO();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(pixel_program);
        glBindVertexArray(quad_vao);
        glBindTexture(GL_TEXTURE_2D, pixels_texture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SDL_GL_SwapWindow(window);
    }

    return 0;
}
