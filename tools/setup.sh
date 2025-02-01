#!/bin/bash
REPO_ROOT="$(git rev-parse --show-toplevel)"

# Create virtual environment
python3 -m venv "${REPO_ROOT}/.venv"

# Activate virtual environment and install requirements
source "${REPO_ROOT}/.venv/bin/activate"
pip install -r "${REPO_ROOT}/requirements.txt"

# Make the pre-commit hook executable
chmod +x "${REPO_ROOT}/.git/hooks/pre-commit"
