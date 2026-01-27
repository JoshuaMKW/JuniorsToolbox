# Save this as gen_sjis.py and run it
def generate_cpp_switch():
    print("constexpr unsigned short unicode_to_sjis_lookup(char32_t cp) {")
    print("    if (cp < 0x80) return static_cast<unsigned short>(cp);")
    print("    switch (cp) {")
    
    # Iterate all valid Shift-JIS double-byte characters
    # Valid lead bytes: 0x81-0x9F, 0xE0-0xEF
    # Valid trail bytes: 0x40-0x7E, 0x80-0xFC
    for lead in list(range(0x81, 0xA0)) + list(range(0xE0, 0xF0)):
        for trail in list(range(0x40, 0x7F)) + list(range(0x80, 0xFD)):
            if trail == 0x7F: continue
            
            sjis_bytes = bytes([lead, trail])
            try:
                # Attempt to decode as Shift-JIS
                char = sjis_bytes.decode('shift_jis')
                # Get the Unicode codepoint
                codepoint = ord(char)
                
                # Print C++ case
                # 0x{lead:02X}{trail:02X} creates the combined 16-bit integer
                print(f"        case 0x{codepoint:X}: return 0x{lead:02X}{trail:02X};")
            except:
                pass # Not a valid mapping
                
    print("        default: return 0x3F; // '?'")
    print("    }")
    print("}")

if __name__ == "__main__":
    generate_cpp_switch()