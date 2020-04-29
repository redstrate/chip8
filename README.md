# chip8
A chip8 emulator that I implemented in C++. It only implements the SCHIP instruction set, and can play many modern roms found online.

This emulator also comes with a WIP compiler that takes a C-style language as input and can spit out valid CHIP-8 code:
```
var count = 3;
label(main);
count += 3;
draw_char(0, 5, count)
jump(main);
```

There's also a basic memory viewer and "debugger" included as well.
