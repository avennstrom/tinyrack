@echo on

if not exist bin mkdir bin

rem copy src\index.html bin
copy src\audio-processor.js bin
copy asset\font.png bin

shader_minifier --format text --preserve-externals -o obj\color.vert .\src\shaders\color.vert
shader_minifier --format text --preserve-externals -o obj\color.frag .\src\shaders\color.frag
shader_minifier --format text --preserve-externals -o obj\font.vert .\src\shaders\font.vert
shader_minifier --format text --preserve-externals -o obj\font.frag .\src\shaders\font.frag
python htmlgen.py

python fontgen.py

set CFLAGS=-std=c23 -Os --target=wasm32 -nostdlib -DPLATFORM_WEB

clang %CFLAGS% -o obj/main.o -c src/main.c
clang %CFLAGS% -o obj/modules.o -c src/modules.c
clang %CFLAGS% -o obj/renderbuf.o -c src/renderbuf.c
clang %CFLAGS% -o obj/stdlib.o -c src/stdlib.c
clang %CFLAGS% -o obj/strbuf.o -c src/strbuf.c
clang %CFLAGS% -o obj/platform_web.o -c src/platform_web.c
clang %CFLAGS% -o obj/parser.o -c src/parser.c
clang %CFLAGS% -o obj/timer.o -c src/timer.c
clang %CFLAGS% -o obj/math.o -c src/math.c

wasm-ld @exports.txt -o bin/rack.wasm ^
    obj/main.o obj/modules.o obj/renderbuf.o ^
    obj/stdlib.o obj/strbuf.o obj/platform_web.o ^
    obj/parser.o obj/timer.o obj/math.o

wasm2wat bin/rack.wasm -o obj/rack.wat

wasm-objdump -s bin\rack.wasm > obj\rack-dump.txt