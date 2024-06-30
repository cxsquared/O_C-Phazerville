Import("env")

import subprocess
import os

if os.name == 'nt':
    version = subprocess.check_output(["powershell.exe", "Get-Content .\src\OC_version.h | Select-Object -Index 1 | %{ $_ -replace '\"', ''}"], shell=True)
    git_rev = subprocess.check_output("git rev-parse --short HEAD", shell=True).decode().strip()
else:
    version = subprocess.check_output("tail -n1 'src/OC_version.h'|tr -d '\"'", shell=True).decode().strip()
    git_rev = subprocess.check_output("sh res/oc_build_tag.sh", shell=True).decode().strip()

extras = ""
env.Append(BUILD_FLAGS=[ f'-DOC_BUILD_TAG=\\"{git_rev}\\"' ])

build_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = build_flags.get("CPPDEFINES")
for item in defines:
    if item[0] == 'OC_VERSION_EXTRA':
        extras += item[1].strip('"')

if "VOR" in defines:
    extras += "+VOR"
if "FLIP_180" in defines:
    extras += "_flipped"

env.Replace(PROGNAME=f"o_C-phazerville-{version}{extras}-{git_rev}")
