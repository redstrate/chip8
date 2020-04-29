#define DOCTEST_CONFIG_COLORS_NONE
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "emu.hpp"

TEST_CASE("Test 0x1") {
    state.reset();

    process_opcode(0x1100);
    
    CHECK(state.PC == 0x100);
}

TEST_CASE("Test 0x3") {
    state.reset();
    
    // set v[1] to 1
    process_opcode(0x6101);
    
    // should skip if v[1] == 1
    process_opcode(0x3101);
    CHECK(state.PC == 0x206);
    
    // should not skip since v[1] != 2
    process_opcode(0x3102);
    CHECK(state.PC == 0x208);
}

TEST_CASE("Test 0x4") {
    state.reset();
    
    // set v[1] to 1
    process_opcode(0x6101);
    
    // should not skip since v[1] == 1
    process_opcode(0x4101);
    CHECK(state.PC == 0x204);
    
    // should skip since v[1] != 2
    process_opcode(0x4102);
    CHECK(state.PC == 0x208);
}

TEST_CASE("Test 0x6") {
    state.reset();
    
    process_opcode(0x6199);
    
    CHECK(state.v[1] == 0x99);
    CHECK(state.PC == 0x202);
}

TEST_CASE("Test 0x7") {
    state.reset();
    
    // add 1 + 1
    process_opcode(0x7101);
    process_opcode(0x7101);
    
    CHECK(state.v[1] == 0x02);
    CHECK(state.PC == 0x204);
}

TEST_CASE("Test 0x8XY0") {
    state.reset();
    
    // set v[1] to 3
    process_opcode(0x6103);
    
    // set v[2] = 4
    process_opcode(0x6204);
    
    // set v[1] to v[2]
    process_opcode(0x8120);
    
    CHECK(state.v[1] == 0x04);
    CHECK(state.PC == 0x206);
}

TEST_CASE("Test 0xA") {
    state.reset();
    
    process_opcode(0xABCD);
    
    CHECK(state.I == 0xBCD);
    CHECK(state.PC == 0x202);
}

// TODO: find a way to test this!!
TEST_CASE("Test 0xC") {
    state.reset();
    
    process_opcode(0xC122);
    
    CHECK(state.PC == 0x202);
}

TEST_CASE("Test 0xFX07") {
    state.reset();
    
    state.delay_timer = 5;
    
    process_opcode(0xF107);
    
    CHECK(state.v[1] == 0x5);
    CHECK(state.PC == 0x202);
}

TEST_CASE("Test 0xFX1E") {
    SUBCASE("") {
        state.reset();
        
        state.I = 5;
        
        // set v[1] to 5
        process_opcode(0x6105);
        
        process_opcode(0xF11E);
        
        CHECK(state.I == 10);
        CHECK(state.PC == 0x204);
    }
    
    SUBCASE("Overflow") {
        state.reset();
        
        state.I = 4095;
        
        // set v[1] to 5
        process_opcode(0x6125);
        
        process_opcode(0xF11E);
        
        CHECK(state.v[0xF] == 1);
        CHECK(state.PC == 0x204);
    }
}

TEST_CASE("Test 0x33") {
    SUBCASE("Zero") {
        state.reset();
        
        process_opcode(0xF133);
        
        CHECK(state.memory[state.I] == 0);
        CHECK(state.PC == 0x202);
    }
    
    SUBCASE("Larger than Zero") {
        state.reset();
        
        // store 0x65 == 101
        process_opcode(0x6165);
        
        process_opcode(0xF133);

        CHECK(state.memory[state.I] == 1);
        CHECK(state.memory[state.I + 1] == 0);
        CHECK(state.memory[state.I + 2] == 1);
        CHECK(state.PC == 0x204);
    }
}

TEST_CASE("Test 0x65") {
    state.reset();
    
    state.memory[state.I] = 0x1;
    state.memory[state.I + 1] = 0x2;
    state.memory[state.I + 2] = 0x3;

    process_opcode(0xF265);
    
    CHECK(state.v[0] == 0x1);
    CHECK(state.v[1] == 0x2);
    CHECK(state.v[2] == 0x3);
    CHECK(state.PC == 0x202);
}
