with open("spirited_away_icon.jpg", "rb") as f:
    bytes_content = f.read()
with open("spirited_away_icon.h", "w") as f:
    f.write("const unsigned char my_image_jpg[] = {\n")
    f.write(", ".join(hex(b) for b in bytes_content))
    f.write(f"\n}};\nconst unsigned int my_image_jpg_len = {len(bytes_content)};\n")