#include "emu.hpp"

#include <cstdio>
#include <iostream>
#include <array>

typedef void (*cpu_func)(const uint16_t opcode);

void null_func(const uint16_t opcode) {
    printf("unimplemented: %.4X\n", opcode);
}

template<typename T, size_t Size>
void safe_call(const std::array<T, Size>& array, const int index, const uint16_t opcode) {
    if(index < Size) {
        array[index](opcode);
    } else {
        null_func(opcode);
    }
}

// 0x00E0
void op_e0(const uint16_t opcode) {
    for(int y = 0; y < screen_height; y++) {
        for(int x = 0; x < screen_width; x++) {
            state.pixels[to_coord(x, y)] = 0;
        }
    }
    
    state.draw_dirty = true;
    
    state.PC += 2;
}

// 0x00EE
void op_ee(const uint16_t opcode) {
    state.stack_pointer--;
    state.PC = state.stack[state.stack_pointer];
    state.PC += 2;
}

constexpr std::array op0_func = {
    op_e0, // e0
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    null_func,
    op_ee // ee
};

void operation0(const uint16_t opcode) {
    safe_call(op0_func, opcode & 0x000f, opcode);
}

// 1NNN
void operation1(const uint16_t opcode) {
    const uint16_t nnn = opcode & 0x0fff;

    state.PC = nnn;
}

// 2NNN
void operation2(const uint16_t opcode) {
    const uint16_t nnn = opcode & 0x0fff;

    state.stack[state.stack_pointer] = state.PC;
    state.stack_pointer++;
    state.PC = nnn;
}

// 3XNN
void operation3(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t nn = opcode & 0x00ff;

    if(state.v[x] == nn)
        state.PC += 4;
    else
        state.PC += 2;
}

// 4XNN
void operation4(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t nn = opcode & 0x00ff;
    
    if(state.v[x] != nn)
        state.PC += 4;
    else
        state.PC += 2;
}

// 6XNN
void operation6(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t nn = opcode & 0x00ff;
    
    state.v[x] = nn;
    
    state.PC += 2;
}

// &XNN
void operation7(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t nn = opcode & 0x00ff;
    
    state.v[x] += nn;
    
    state.PC += 2;
}

// 8XY0
void op8_func0(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    state.v[x] = state.v[y];
    state.PC += 2;
}

// 8XY1
void op8_func2(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    state.v[x] = state.v[x] & state.v[y];
    state.PC += 2;
}

// 8XY3
void op8_func3(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    state.v[x] = state.v[x] ^ state.v[y];
    state.PC += 2;
}

// 8XY4
void op8_func4(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    // TODO: implement tests
    if(state.v[y] < (0xFF - state.v[x]))
        state.v[0xF] = 1;
    else
        state.v[0xF] = 0;
    
    state.v[x] += state.v[y];
    state.PC += 2;
}

// 8XY5
void op8_func5(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    // TODO: implement tests
    if(state.v[y] > state.v[x])
        state.v[0xF] = 0;
    else
        state.v[0xF] = 1;
    
    state.v[x] -= state.v[y];
    state.PC += 2;
}

// 8XY6
void op8_func6(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    
    state.v[0xF] = (state.v[x] & 1);
    state.v[x] >>= 1;
    
    state.PC += 2;
}

const std::array op8_func = {
    op8_func0,
    null_func,
    op8_func2,
    op8_func3,
    op8_func4,
    op8_func5,
    op8_func6
};

void operation8(const uint16_t opcode) {
    safe_call(op8_func, opcode & 0x000f, opcode);
}

// 9XY0
void operation9(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    
    if(state.v[x] != state.v[y])
        state.PC += 4;
    else
        state.PC += 2;
}

// ANNN
void operationA(const uint16_t opcode) {
    const uint16_t nnn = opcode & 0x0fff;

    state.I = nnn;
    state.PC += 2;
}

// CXNN
void operationC(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t nn = opcode & 0x00ff;

    srand(time(nullptr));
    state.v[x] = (rand() % 0xFF) & nn;
    state.PC += 2;
}

// DXYN
void operationD(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    const uint8_t y = (opcode & 0x00f0) >> 4;
    const uint8_t n = opcode & 0x000f;

    const uint8_t x_pos = state.v[x];
    const uint8_t y_pos = state.v[y];
    const uint8_t height = n;
    
    state.v[0xF] = 0;
    state.draw_dirty = true;

    for(int y = 0; y < height; y++) {
        const uint8_t pixel = state.memory[state.I + y];
        
        for(int x = 0; x < 8; x++) {
            const int final_x = (x_pos + x) % screen_width;
            const int final_y = (y_pos + y) % screen_height;
            
            if((pixel & (0x80 >> x)) != 0) {
                if(state.pixels[to_coord(final_x, final_y)] == 1) {
                    state.v[0xF] = 1;
                    
                    if(options.enable_anti_flicker)
                        state.draw_dirty = false; // anti-flicker mechanism
                }
                
                state.pixels[to_coord(final_x, final_y)] ^= 1;
            }
        }
    }
    
    state.PC += 2;
}

// EX9E & EXA1
void operationE(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    switch(opcode & 0x00ff) {
        case 0x9E:
        {
            if(state.keys[state.v[x]] == 1)
                state.PC += 4;
            else
                state.PC += 2;
        }
            break;
        case 0xA1:
        {
            if(state.keys[state.v[x]] == 0)
                state.PC += 4;
            else
                state.PC += 2;
        }
            break;
    }
}

// FX07
void opF_func7(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    state.v[x] = state.delay_timer;
    
    state.PC += 2;
}

// FX0A
void opF_funcA(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    for(int i = 0; i < 16; i++) {
        if(state.keys[i] != 0) {
            state.v[x] = i;
            state.PC += 2;
        }
    }
}

// FX55/FX65 & FX15
void opF_func5(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    switch((opcode & 0x00F0) >> 4) {
        case 0x6:
        {
            for(int i = 0; i <= x; i++)
                state.v[i] = state.memory[state.I + i];
            
            if(options.emulate_original)
                state.I += x + 1;
            
            state.PC += 2;
        }
            break;
        case 0x5:
        {
            for(int i = 0; i <= x; i++)
                state.memory[state.I + i] = state.v[i];
            
            if(options.emulate_original)
                state.I += x + 1;
            
            state.PC += 2;
        }
            break;
        case 0x1:
        {
            state.delay_timer = state.v[x];
            
            state.PC += 2;
        }
            break;
    }
}

// FX18
void opF_func8(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    state.sound_timer = state.v[x];
    
    state.PC += 2;
}

// FX29
void opF_func9(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    state.I = state.v[x] * 0x5;
    
    state.PC += 2;
}

// FX1E
void opF_funcE(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;

    if((state.I + state.v[x]) > 0xFFF)
        state.v[15] = 1;
    else
        state.v[15] = 0;
    
    state.I += state.v[x];
    
    state.PC += 2;
}

// FX33
void opF_func3(const uint16_t opcode) {
    const uint8_t x = (opcode & 0x0f00) >> 8;
    
    int decimal_rep = state.v[x];
    
    state.memory[state.I] = (decimal_rep % 1000) / 100;
    state.memory[state.I + 1] = (decimal_rep % 100) / 10;
    state.memory[state.I + 2] = (decimal_rep % 10);
    
    state.PC += 2;
}

const std::array opF_func = {
    null_func,
    null_func,
    null_func,
    opF_func3,
    null_func,
    opF_func5,
    null_func,
    opF_func7,
    opF_func8,
    opF_func9,
    opF_funcA,
    null_func,
    null_func,
    null_func,
    opF_funcE
};

void operationF(const uint16_t opcode) {
    safe_call(opF_func, opcode & 0x000F, opcode);
}

constexpr std::array cpu_opcode = {
    operation0, // 0x0,
    operation1, // 0x1
    operation2, // 0x2
    operation3, // 0x3
    operation4, // 0x4
    null_func, // 0x5
    operation6, // 0x6
    operation7, // 0x7
    operation8, // 0x8
    operation9, // 0x9
    operationA, // 0xA
    null_func, // 0xB
    operationC, // 0xC
    operationD, // 0xD
    operationE, // 0xE
    operationF, // 0xF
};

void process_opcode(const uint16_t opcode) {
    safe_call(cpu_opcode, opcode >> 12, opcode);
}

void save_state() {
    stored_state = state;
}

void load_state() {
    state = stored_state;
}
