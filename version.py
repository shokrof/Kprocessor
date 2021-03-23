import os
import urllib.request, json
import sys



def is_github_action():
    if "GITHUB_WORKFLOW" in dict(os.environ.items()):
        return True
    else:
        return False
    

def get_pypa_dev_latest():
    with urllib.request.urlopen("https://test.pypi.org/pypi/kProcessor/json") as url:
        data = json.loads(url.read().decode())
        return data["info"]["version"]


def increment_patch_version(patch_version):
    patch_version = patch_version.split('.')
    return f"{patch_version[0]}.{patch_version[1]}.{int(patch_version[2]) + 1}.{patch_version[3]}"


MAJOR = 1
MINOR = 1
PATCH = 0

dev_version = f"{MAJOR}.{MINOR}.{PATCH}.dev0"
release_version = f"{MAJOR}.{MINOR}.{PATCH}"

def get_version(release=False):
    version_tag = str()

    if release:
        version_tag = release_version
    else:
        # If it's running on github action, increment the dev patch number
        if is_github_action():
            test_pypa_latest_version = get_pypa_dev_latest()
            version_tag = increment_patch_version(test_pypa_latest_version)
        
        # Running on local machine
        else:
            version_tag = dev_version

    return version_tag


if len(sys.argv):
    if "master" in "".join(sys.argv):
        print(get_version(release=True))
    else:
        print(get_version())