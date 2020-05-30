#include "compiler.hpp"

#include <string>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>

#include "emu.hpp"

void set_bit(uint16_t& opcode, int digit, int value) {
    uint16_t mask = 0x0000;
    switch(digit) {
        case 0:
            mask = 0xF000;
            break;
        case 1:
            mask = 0x0F00;
            break;
        case 2:
            mask = 0x00F0;
            break;
        case 3:
            mask = 0x000F;
            break;
    }
    
    opcode = (opcode & ~mask) | (digit == 3 ? value : (value << (8 / digit)));
}

void set_nn(uint16_t& opcode, int value) {
    opcode = (opcode & ~0x00FF) | value;
}

void set_nnn(uint16_t& opcode, int value) {
    opcode = (opcode & ~0x0FFF) | value;
}

enum class RegisterOp {
    Assign,
    Add
};

std::vector<std::string> split(const std::string& s, const char delimiter) {
    std::vector<std::string> splits;
    
    std::istringstream ss(s);
    std::string split;
    while (std::getline(ss, split, delimiter))
        splits.push_back(split);
        
    return splits;
}

std::vector<uint16_t> opcodes;
int v_offset = 0;

bool is_number(const std::string& str) {
    return std::find_if(str.begin(), str.end(), [](unsigned char c) { return !std::isdigit(c); }) == str.end();
}

struct VariableData {
    int offset = program_begin;
    int default_value = 0;
};

std::map<std::string, VariableData> variable_data;

enum class VariableType {
    VRegister,
    Constant,
    Variable
};

VariableType determine_Type(std::string str) {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    
    if(str.find('v') != std::string::npos) {
        return VariableType::VRegister;
    } else if(!is_number(str)) {
        return VariableType::Variable;
    } else {
        return VariableType::Constant;
    }
}

int parse_v_index(std::string str) {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

    if(str.find('v') != std::string::npos) {
        auto left_side_index = str.substr(str.find('[') + 1, str.length() - 2);
        return std::stoi(left_side_index);
    } else if(!is_number(str)) {
        uint16_t opcode = 0xA000;
        set_nnn(opcode, variable_data[str].offset);
        
        opcodes.push_back(opcode);
        
        opcode = 0xF065;
        set_bit(opcode, 1, v_offset);
        
        opcodes.push_back(opcode);
        
        return v_offset++;
    } else {
        uint16_t opcode = 0x6000;
        set_bit(opcode, 1, v_offset);
        set_nn(opcode, std::stoi(str));
        
        opcodes.push_back(opcode);
        
        return v_offset++;
    }
}

std::map<std::string, int> get_arguments(std::vector<std::string> args, std::vector<std::string> arg_format) {
    struct OrderData {
        std::string arg, name;
        int v_offset;
        VariableType type;
    };
    
    std::vector<OrderData> datas;
    
    int i = 0;
    for(auto arg : args) {
        arg.erase(std::remove(arg.begin(), arg.end(), ' '), arg.end());

        OrderData data = {};
        data.name = arg_format[i];
        data.arg = arg;
        data.v_offset = parse_v_index(arg);
        data.type = determine_Type(arg);
        
        datas.push_back(data);
        i++;
    }
    
    std::sort(datas.begin(), datas.end(), [](const OrderData& a, const OrderData& b) {
        return a.type == VariableType::Variable && (a.v_offset > b.v_offset);
    });

    std::map<std::string, int> real_args;
    for(auto data : datas) {
        real_args[data.name] = parse_v_index(data.arg);
    }
    
    return real_args;
}

void compile(std::string code) {
    int current_offset = 0;
    int next_instruction = 0;
    
    std::map<std::string, int> function_map;
    bool needs_to_store_next_function = false;
    std::string next_function_to_store;
    
    code.erase(std::remove(code.begin(), code.end(), '\n'), code.end());
        
    while(true) {
        next_instruction = code.find_first_of(';', current_offset + 1);
        if(next_instruction == -1)
            break;
        
        auto instruction = code.substr(current_offset == 0 ? current_offset : current_offset + 1, current_offset == 0 ? (next_instruction - current_offset) : (next_instruction - current_offset) - 1);
                
        if(needs_to_store_next_function) {
            int offset = (int)opcodes.size() + variable_data.size() + program_begin;
            needs_to_store_next_function = false;
            function_map[next_function_to_store] = offset + 1;
        }
        
        // assignment
        if(instruction.find("var") != std::string::npos) {
            auto var_string = split(instruction, ' ');
            
            auto var_name = var_string[1];
            auto var_default = var_string[3];
            
            VariableData data = {};
            data.default_value = std::stoi(var_default);
            data.offset = program_begin + variable_data.size();
            
            variable_data[var_name] = data;
            
            uint16_t opcode = 0xA000;
            set_nnn(opcode, data.offset);
            
            opcodes.push_back(opcode);
            
            opcode = 0x7000;
            set_bit(opcode, 1, 0);
            set_nn(opcode, data.default_value);
            
            opcodes.push_back(opcode);
            
            opcode = 0xA000;
            set_nnn(opcode, data.offset);
            
            opcodes.push_back(opcode);
            
            opcode = 0xF055;
            set_bit(opcode, 1, 0);
            
            opcodes.push_back(opcode);
        } else if(instruction.find("+=") != std::string::npos) {
            auto left_side = instruction.substr(0, instruction.find_first_of(' '));
            auto right_side = instruction.substr(instruction.find("+=") + 2, instruction.length());
            
            int right_side_integer = std::stoi(right_side);
            
            auto left_side_type = determine_Type(left_side);
            auto left_side_integer = parse_v_index(left_side);
                            
            // 6XNN
            uint16_t opcode = 0x7000;
            set_bit(opcode, 1, left_side_integer);
            set_nn(opcode, right_side_integer);
            
            opcodes.push_back(opcode);
            
            // if it is a variable, update it in memory
            if(left_side_type == VariableType::Variable) {
                opcode = 0xA000;
                set_nnn(opcode, variable_data[left_side].offset);
                
                opcodes.push_back(opcode);
                
                opcode = 0xF055;
                set_bit(opcode, 1, left_side_integer);
                
                opcodes.push_back(opcode);
            }
        } else if(instruction.find('=') != std::string::npos) {
            auto left_side = instruction.substr(0, instruction.find_first_of(' '));
            auto right_side = instruction.substr(instruction.find('=') + 2, instruction.length());
            
            int right_side_integer = std::stoi(right_side);
            
            // this is a v register
            if(left_side.find('[') != std::string::npos) {
                auto left_side_integer = parse_v_index(left_side);
                
                // 6XNN
                uint16_t opcode = 0x6000;
                set_bit(opcode, 1, left_side_integer);
                set_nn(opcode, right_side_integer);
                
                opcodes.push_back(opcode);
            }
        } else if(instruction.find('(') != std::string::npos) {
            // function
            auto function_name = instruction.substr(0, instruction.find_first_of('('));
            
            auto arguments_string = instruction.substr(instruction.find_first_of('(') + 1, instruction.length() - instruction.find_first_of('(') - 2);
            auto arguments = split(arguments_string, ',');
            
            if(function_name == "draw_char") {
                auto args = get_arguments(arguments, {"x", "y", "n"});
                
                auto x_index = args["x"];
                auto y_index = args["y"];
                auto c_index = args["n"];
                
                // FX29
                uint16_t opcode = 0xF029;
                set_bit(opcode, 1, c_index);
                
                opcodes.push_back(opcode);
                
                // DXYN
                opcode = 0xD000;
                set_bit(opcode, 1, x_index);
                set_bit(opcode, 2, y_index);
                set_bit(opcode, 3, 5);
                opcodes.push_back(opcode);
            } else if(function_name == "label") {
                needs_to_store_next_function = true;
                next_function_to_store = arguments[0];
            } else if(function_name == "jump") {
                // 1NNN
                uint16_t opcode = 0x1000;
                set_nnn(opcode, function_map[arguments[0]]);
                                
                opcodes.push_back(opcode);
            }
        }
        
        v_offset = 0;
        current_offset = next_instruction;
    }
    
    std::cout << "Finished compilation!" << std::endl;
}

void load_compiled_rom() {
    std::vector<uint8_t> compiled_opcodes;
    for(auto& opcode : opcodes) {
        compiled_opcodes.push_back(opcode >> 8); // hi
        compiled_opcodes.push_back(opcode); // low
    }
    
    memcpy(state.memory + program_begin, compiled_opcodes.data(), compiled_opcodes.size());
}
