# clean up app.py
with open("app.py", "r", encoding="utf-8") as f:
    content = f.read()

# Find the FIRST occurrence of "if __name__ == \"__main__\":"
marker = "if __name__ == \"__main__\":"
pos = content.find(marker)

if pos != -1:
    # Check if main() is after it
    main_call = "    main()"
    next_pos = content.find(main_call, pos)
    if next_pos != -1:
        # Keep until end of main() call
        end_pos = next_pos + len(main_call)
        new_content = content[:end_pos]
        
        with open("app.py", "w", encoding="utf-8") as f:
            f.write(new_content)
        print("Truncated app.py successfully (First Occurrence).")
    else:
        print("Couldn't find main() call after if name block.")
else:
    print("Couldn't find if name block.")
