#!/data/data/com.termux/files/usr/bin/bash

# === Configuration ===
# Set your name and email here for git commits
GIT_USER_NAME="81-ton"
GIT_USER_EMAIL="cinarkocaktuz@gmail.com"

# ---------------------

# Check for git installed
if ! command -v git &> /dev/null; then
  echo "Git not found! Please install it first: pkg install git"
  exit 1
fi

# Set git user config globally if not set
if ! git config --global user.name &> /dev/null; then
  git config --global user.name "$GIT_USER_NAME"
fi

if ! git config --global user.email &> /dev/null; then
  git config --global user.email "$GIT_USER_EMAIL"
fi

# Check current directory and list files
echo "Uploading project from folder: $(pwd)"
echo "Files in folder:"
ls

# Ask for GitHub repo URL (HTTPS or SSH)
read -p "Enter your GitHub repository URL (HTTPS or SSH): " REPO_URL

# Initialize git repo if needed
if [ ! -d .git ]; then
  git init
  # Set default branch to main
  git branch -M main
fi

# Create or update README.md
if [ ! -f README.md ]; then
  echo "# $(basename $(pwd))" > README.md
  echo "" >> README.md
  echo "This is my project uploaded from Termux." >> README.md
fi

# Download C++ .gitignore if not exist
if [ ! -f .gitignore ]; then
  curl -s https://raw.githubusercontent.com/github/gitignore/main/C++.gitignore -o .gitignore
fi

# Add LICENSE file if missing (MIT License)
if [ ! -f LICENSE ]; then
  cat > LICENSE <<EOF
MIT License

Copyright (c) $(date +%Y) $GIT_USER_NAME

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
EOF
fi

# Stage files
git add .

# Commit with current date and time
commit_msg="Update from Termux on $(date '+%Y-%m-%d %H:%M:%S')"
git commit -m "$commit_msg" 2>/dev/null || echo "No changes to commit."

# Add remote if not set
if ! git remote | grep origin &> /dev/null; then
  git remote add origin "$REPO_URL"
fi

# Push to GitHub
echo "Pushing to $REPO_URL ..."
git push -u origin main

echo "✅ Upload finished."
