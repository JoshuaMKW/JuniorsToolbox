import json
from pathlib import Path


def restructure_wizard(data):
    # Iterate over the top-level keys (e.g., "Strollin' Stu")
    for main_key, main_content in data.items():

        # Check if "Wizard" exists in this object
        if "Rendering" in main_content:
            rendering_section = main_content["Rendering"]

            # Iterate over each object inside Rendering (e.g., "Default")
            for render_group_name, render_group_content in rendering_section.items():

                # Create the new structure
                orig = rendering_section[render_group_name]
                if "Object" in orig:
                    orig["Model"] = orig["Object"]
                    del orig["Object"]

                # Replace the old content with the new structure
                rendering_section[render_group_name] = orig

    return data


# Run the function

root = Path("./Templates/Vanilla")
file_list = [f for f in root.rglob("**/*") if f.is_file() and f.suffix.endswith("json")]
for file in file_list:
    print(file)
    with open(file, "r", encoding="utf-8-sig") as f:
        json_data = json.load(f)
    new_data = restructure_wizard(json_data)
    with open(file, "w", encoding="utf-8-sig") as f:
        json.dump(new_data, f, indent=4, ensure_ascii=False)
