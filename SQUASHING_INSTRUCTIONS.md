# How to Squash Commits in DownPour

Hi! This PR provides you with tools to clean up the commit history in your DownPour repository.

## What's the Problem?

Your repository has accumulated **65+ commits** with many that are:
- Duplicates (e.g., "Initial plan" appears 6 times!)
- Uninformative (e.g., "🤨🤨🤨🤨🤨🤨🤨", "refactor", "more add")
- Redundant (same message appearing multiple times)

This makes the git history hard to follow and doesn't tell a clear story of the project's evolution.

## What's Been Provided?

### 1. Analysis Script
Run this to see what commits need cleanup:
```bash
./scripts/squash_commits.sh --analyze
```

This shows you:
- Total commit count
- Duplicate commit messages
- Short/uninformative commits
- Merge commits

**Example output:**
```
[INFO] Finding duplicate commit messages...
      6 Initial plan
      2 🤨🤨🤨🤨🤨🤨🤨
      2 refactor WIP
      2 destroy the god object
      ...
```

### 2. Squashing Script
Interactive script to help you clean up commits:

**Option A: Squash everything into one commit** (simplest)
```bash
./scripts/squash_commits.sh --squash-all
```

**Option B: Squash just recent commits** (recommended)
```bash
./scripts/squash_commits.sh --squash-recent 15
```

The script will:
- ✅ Create a backup branch automatically
- ✅ Guide you through the process
- ✅ Show you what you're about to change
- ✅ Ask for confirmation before making changes

### 3. Comprehensive Guide
See **[docs/COMMIT_SQUASHING_GUIDE.md](docs/COMMIT_SQUASHING_GUIDE.md)** for:
- Detailed explanations of each approach
- Manual git commands if you prefer
- Best practices for commit messages
- How to handle things after squashing

## Quick Start (Recommended)

### Step 1: Analyze
```bash
cd /path/to/DownPour
./scripts/squash_commits.sh --analyze
```

### Step 2: Decide Your Approach

**If you want a clean slate:**
```bash
./scripts/squash_commits.sh --squash-all
```
This creates one commit with all your code. Perfect if you just want to start fresh.

**If you want to keep some history:**
```bash
./scripts/squash_commits.sh --squash-recent 20
```
This lets you interactively squash the last 20 commits while keeping older history.

### Step 3: Push Changes
After squashing, you'll need to force push:
```bash
git push -f origin master
```

⚠️ **Important:** Force pushing rewrites history. Make sure:
- No one else is currently working on the repository
- You've notified any collaborators
- You understand this can't be easily undone (though the script creates backups)

## Alternative: Keep Current History

If you don't want to rewrite history, that's fine! You can:
1. Leave the current commits as-is
2. Just commit more carefully going forward
3. Use the commit message conventions in the guide

## Example: Before and After

**Before:**
```
3d00748 Initial plan
ee59896 destroy the god object
ebe806f refactor
2101e99 Add Entity Design
4a89f8a create python loader for materials
04b1613 create logging system for app
6a5113a refactor WIP
6909a38 🤨🤨🤨🤨🤨🤨🤨
...
```

**After (squash-all):**
```
abc1234 DownPour: Complete rain simulation system with Vulkan rendering

This commit includes:
- Vulkan-based rendering engine
- Weather system with rain simulation
- Model loading and material management
...
```

**After (squash-recent with grouping):**
```
abc1234 Implement weather system with rain particles and windshield effects
def5678 Add entity system and material loading
ghi9012 Major refactor: Extract Vulkan subsystems
jkl3456 Setup 3D scene with camera and models
...
```

## Need Help?

- **Read the guide:** [docs/COMMIT_SQUASHING_GUIDE.md](docs/COMMIT_SQUASHING_GUIDE.md)
- **Run with --help:** `./scripts/squash_commits.sh --help`
- **Try analyze first:** `./scripts/squash_commits.sh --analyze`

The script is safe - it creates backups and asks for confirmation before making changes!
