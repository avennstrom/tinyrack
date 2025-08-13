@echo off
if not exist bin mkdir bin
emcc -o bin/tinyrack.html --shell-file src/emscripten_shell.html -Iraylib/include -Lraylib src/main.c src/modules.c -lraylib -s USE_GLFW=3 -sINITIAL_MEMORY=128MB