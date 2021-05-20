import argparse
import os
import shutil
import json

parser = argparse.ArgumentParser(description='Convert velocity fields from 0 -> 127 to floating point 0.0 -> 1.0')
parser.add_argument('file', type=str, help='The file to process')
parser.add_argument('--no-backup', action='store_true', help='Do not create a .bak file')
parser.add_argument('--verbose', action='store_true', help='Verbose mode')
args = parser.parse_args()

assert os.path.exists(args.file), f"Can't file {args.file}"

if not args.no_backup:
    backup_filename = args.file + ".bak"
    shutil.copyfile(args.file, backup_filename)

# with open (args.file, "r") as file:
#     file_lines = file.readlines()

# regex = re.compile(r"\"velocity\"\\w*:\\w*(\\d+)", re.IGNORECASE)
# for line in file_lines:
#     print(regex.findall(line))

def item_generator(json_input, lookup_key):
    if isinstance(json_input, dict):
        for k, v in json_input.items():
            if k == lookup_key:
                yield v
            else:
                yield from item_generator(v, lookup_key)
    elif isinstance(json_input, list):
        for item in json_input:
            yield from item_generator(item, lookup_key)

with open(args.file, "r") as file:
    data = json.load(file)

for notes in item_generator(data, "notes"):
    for note in notes:
        note["velocity"] = float(note["velocity"] / 127.0);

with open(args.file, "w") as file:
    json.dump(data, file, indent="\t")
