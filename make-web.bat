@echo off
if not exist bin mkdir bin
emcc -Os -o bin/tinyrack.html --shell-file src/emscripten_shell.html -Iraylib/include -Lraylib src/main.c src/modules.c src/timer.c src/parser.c src/platform.c src/platform_raylib.c -lraylib -s USE_GLFW=3 -sINITIAL_MEMORY=128MB -sASSERTIONS -D__DEFINED_timer_t