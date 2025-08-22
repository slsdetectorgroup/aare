# SPDX-License-Identifier: LGPL-3.0-or-other
# Copyright (C) 2021 Contributors to the Aare Package
"""
Script to update VERSION file with semantic versioning if provided as an argument, or with 0.0.0 if no argument is provided.
"""

import sys
import os
import re
from datetime import datetime

from packaging.version import Version, InvalidVersion


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def is_integer(value): 
    try:
        int(value)
    except ValueError:
        return False
    else:
        return True


def get_version():

    # Check at least one argument is passed
    if len(sys.argv) < 2:
        version =   datetime.today().strftime('%Y.%-m.%-d')
    else: 
        version = sys.argv[1]

    try:
        v = Version(version)  # normalize check if version follows PEP 440 specification
        
        version_normalized = version.replace("-", ".")

        version_normalized =  re.sub(r'0*(\d+)', lambda m : str(int(m.group(0))), version_normalized) #remove leading zeros 

        return version_normalized 

    except InvalidVersion as e:
        print(f"Invalid version {version}. Version format must follow semantic versioning format of python PEP 440 version identification specification.")
        sys.exit(1)
    

def write_version_to_file(version):
    version_file_path = os.path.join(SCRIPT_DIR, "VERSION")
    with open(version_file_path, "w") as version_file:
        version_file.write(version)
    print(f"Version {version} written to VERSION file.")

# Main script
if __name__ == "__main__":

    version = get_version()
    write_version_to_file(version)
