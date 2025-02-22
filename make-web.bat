@echo off
if not exist bin mkdir bin
emcc -o bin/tinyrack.html -Iraylib/include -Lraylib src/main.c -lraylib -s USE_GLFW=3 -sINITIAL_MEMORY=128MB -s ASYNCIFY