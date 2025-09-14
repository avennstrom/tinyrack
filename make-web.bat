@echo off
if not exist bin mkdir bin
copy asset\font.png bin
copy src\audio-processor.js bin
python fontgen.py
emcc -std=c23 -Os -o bin/tinyrack.html --shell-file src/emscripten_shell.html --js-library src/platform.js src/main.c src/modules.c src/timer.c src/parser.c src/platform.c src/platform_web.c -sINITIAL_MEMORY=128MB -D__DEFINED_timer_t -DPLATFORM_WEB -sFILESYSTEM=0 -sMINIMAL_RUNTIME=0 -sASSERTIONS=0 --closure 0