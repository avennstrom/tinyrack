@echo off
if not exist bin mkdir bin
emcc -Os -o bin/tinyrack.html --shell-file src/emscripten_shell.html -Iraylib/include -Lraylib src/main.c src/modules.c src/timer.c src/parser.c -lraylib -s USE_GLFW=3 -sINITIAL_MEMORY=128MB -sFILESYSTEM=0 -sMINIMAL_RUNTIME=0 -sASSERTIONS=0 --closure 0 -D__DEFINED_timer_t