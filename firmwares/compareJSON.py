import json

# Load the two JSON files
with open("x2745_DPP-PHA_2023091800_parameters.json") as f1:
    old_params = json.load(f1)

with open("x2745_DPP-PHA_2025012205_parameters.json") as f2:
    new_params = json.load(f2)

# Index by parameter name for easy lookup
old_dict = {param["name"]: param for param in old_params}
new_dict = {param["name"]: param for param in new_params}

# 1. Find new parameter names
new_names = set(new_dict.keys()) - set(old_dict.keys())
print("🆕 New parameters in new version:")
for name in sorted(new_names):
    print(f"  - {name}")

# 2. Compare allowed values for common parameters
print("\n🔍 Parameters with changed allowed values:")
for name in sorted(set(old_dict.keys()) & set(new_dict.keys())):
    old_vals = {v["value"]: v["description"] for v in old_dict[name].get("allowed_values", [])}
    new_vals = {v["value"]: v["description"] for v in new_dict[name].get("allowed_values", [])}
    
    if old_vals != new_vals:
        print(f"  - {name}")
        removed = set(old_vals) - set(new_vals)
        added = set(new_vals) - set(old_vals)
        changed = {k for k in old_vals if k in new_vals and old_vals[k] != new_vals[k]}
        
        if added:
            print(f"     ➕ Added: {sorted(added)}")
        if removed:
            print(f"     ➖ Removed: {sorted(removed)}")
        if changed:
            print(f"     🔄 Modified descriptions: {sorted(changed)}")
