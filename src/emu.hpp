#pragma once

#include <cstdint>

// chip-8 constants
constexpr int screen_width = 64;
constexpr int screen_height = 32;
constexpr int program_begin = 0x200;
constexpr int stack_size = 16;

inline int to_coord(int x, int y) {
    return (y * screen_width) + x;
}

struct EmulatorState {
    void reset() {
        for(int i = 0; i < 4096; i++)
            memory[i] = 0;
        
        PC = program_begin;
        
        for(int i = 0; i < 12; i++)
            stack[i] = 0;
        
        stack_pointer = 0;
        
        I = 0;
        
        for(int i = 0; i < 16; i++)
            v[i] = 0;
        
        delay_timer = 0;
        sound_timer = 0;
        
        for(int y = 0; y < screen_height; y++) {
            for(int x = 0; x < screen_width; x++) {
                pixels[to_coord(x, y)] = 0;
            }
        }
        
        draw_dirty = true;
    }
    
    uint8_t memory[4096] = {};
    uint16_t PC = program_begin;
    
    uint16_t stack[stack_size] = {};
    int stack_pointer = 0;
    
    uint16_t I = 0;
    
    uint8_t v[16] = {};
    
    uint8_t delay_timer = 0, sound_timer = 0;
    
    uint8_t pixels[screen_width * screen_height] = {};
    bool draw_dirty = false;
    
    bool keys[16] = {};
};

struct EmuOptions {
    bool enable_anti_flicker = true;
    bool emulate_original = false;
};

inline EmuOptions options;

inline bool pause_execution = false;

inline EmulatorState state;
inline EmulatorState stored_state;

void process_opcode(const uint16_t opcode);

void save_state();
void load_state();
