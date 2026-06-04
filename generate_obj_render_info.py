import json
from pathlib import Path

def create_render_info(model: str, textures: dict[str], animations: list[str]) -> dict:
    new_render_info = {
        "Animations": animations,
        "Model": model,
        "Textures": textures
    }
    return new_render_info

def collect_render_data_from_wizard(wizard: dict) -> dict | None:
    if "Members" not in wizard:
        return None

    members = wizard["Members"]
    if "Object" not in members:
        return None

    return { members["Object"] : create_render_info("", {}, []) }

def collect_render_datas(data) -> dict:
    render_datas: dict = {}

    # Iterate over the top-level keys (e.g., "Strollin' Stu")
    for main_key, main_content in data.items():

        # Check if "Wizard" exists in this object
        if "Wizard" in main_content:
            wizard_section = main_content["Wizard"]

            # Iterate over each object inside Rendering (e.g., "Default")
            for wizard_group_name, wizard_group_content in wizard_section.items():
                render_data = collect_render_data_from_wizard(wizard_group_content)
                if render_data is None:
                    continue

                render_datas.update(render_data)

    return render_datas


# Run the function

all_render_data: dict = {}

root = Path("./Templates/Vanilla")
file_list = [f for f in root.rglob("**/*") if f.is_file() and f.suffix.endswith("json")]
for file in file_list:
    print(file)
    with open(file, "r", encoding="utf-8-sig") as f:
        json_data = json.load(f)
    render_datas = collect_render_datas(json_data)
    all_render_data.update(render_datas)

with open("./Templates/object_info_map.json", "r", encoding="utf-8-sig") as f:
    all_render_data.update(json.load(f))
    
sorted_dict = {k: v for k, v in sorted(all_render_data.items())}
print(sorted_dict)

with open("./Templates/object_info_map.json", "w", encoding="utf-8-sig") as f:
    json.dump(sorted_dict, f, indent=4, ensure_ascii=False)