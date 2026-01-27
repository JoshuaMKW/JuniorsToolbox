import json
from pathlib import Path


def restructure_wizard(data):
    # Iterate over the top-level keys (e.g., "Strollin' Stu")
    for main_key, main_content in data.items():

        # Check if "Wizard" exists in this object
        if "Wizard" in main_content:
            wizard_section = main_content["Wizard"]

            # Iterate over each object inside Wizard (e.g., "Default")
            for wiz_obj_name, wiz_obj_content in wizard_section.items():

                # Create the new structure
                new_structure = {
                    "Name": wiz_obj_name,  # Add sibling attribute matching the key name
                    "Dependencies": {
                        "Managers": {},
                        "TablesBin": {},
                        "AssetPaths": [],
                    },
                    "Members": wiz_obj_content,  # Move all original fields into "Members"
                }

                # Replace the old content with the new structure
                wizard_section[wiz_obj_name] = new_structure

    return data


# Run the function

root = Path("./Templates")
file_list = [f for f in root.rglob("**/*") if f.is_file() and f.suffix.endswith("json")]
for file in file_list:
    with open(file, "r", encoding="utf-8-sig") as f:
        json_data = json.load(f)
    new_data = restructure_wizard(json_data)
    with open(file, "w", encoding="utf-8-sig") as f:
        json.dump(new_data, f, indent=4, ensure_ascii=False)
