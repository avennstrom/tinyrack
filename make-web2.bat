@echo off
if not exist bin2 mkdir bin2
clang -std=c23 --target=wasm32 -c src/main.c -DPLATFORM_WEB