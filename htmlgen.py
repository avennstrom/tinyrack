with open("src/index.html", "r", encoding="utf-8") as f:
    content = f.read()

with open("obj/color.vert", "r", encoding="utf-8") as f:
    content = content.replace("___COLOR_VERT___", f.read())
with open("obj/color.frag", "r", encoding="utf-8") as f:
    content = content.replace("___COLOR_FRAG___", f.read())
with open("obj/font.vert", "r", encoding="utf-8") as f:
    content = content.replace("___FONT_VERT___", f.read())
with open("obj/font.frag", "r", encoding="utf-8") as f:
    content = content.replace("___FONT_FRAG___", f.read())

with open("bin/index.html", "w", encoding="utf-8") as f:
    f.write(content)