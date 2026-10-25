#!/bin/bash
set -e
cd "$(dirname "$0")"
if ! command -v python3 >/dev/null; then
  echo 'Python 3 is required. Install Python 3, then run this file again.'
  read -r -p 'Press Return to close.'
  exit 1
fi
python3 -m venv .flash-env
.flash-env/bin/python -m pip install 'esptool==5.1.0'
.flash-env/bin/python flash.py "$@"
read -r -p 'Press Return to close.'
