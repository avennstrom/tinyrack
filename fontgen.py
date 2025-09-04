import json
import math

font = json.load(open('asset/font.json'))

f = open('src/font2.h', 'w')

f.write('#include <stdint.h>\n')
f.write('struct font_glyph {\n')
f.write('\tuint32_t atlas_left;\n')
f.write('\tuint32_t atlas_right;\n')
f.write('\tuint32_t atlas_top;\n')
f.write('\tuint32_t atlas_bottom;\n')
f.write('\tfloat plane_left;\n')
f.write('\tfloat plane_right;\n')
f.write('\tfloat plane_top;\n')
f.write('\tfloat plane_bottom;\n')
f.write('} g_font_glyphs[] = {\n')

glyphs = font['glyphs']
for glyph in glyphs:
    char = glyph['unicode']
    if 'atlasBounds' in glyph:
        atlas = glyph['atlasBounds']
        plane = glyph['planeBounds']

        f.write(f'\t[{char}] = {{')
        f.write(str(math.floor(atlas['left'])))
        f.write(',' + str(math.floor(atlas['right'])))
        f.write(',' + str(math.floor(atlas['bottom'])))
        f.write(',' + str(math.floor(atlas['top'])))
        f.write(',' + str(plane['left']) + 'f')
        f.write(',' + str(plane['right']) + 'f')
        f.write(',' + str(plane['bottom']) + 'f')
        f.write(',' + str(plane['top']) + 'f')
        f.write('},\n')

f.write('};\n')
f.close()