# Commit Squashing Guide for DownPour Repository

## Problem
The DownPour repository has accumulated many redundant and uninformative commits that make the git history difficult to follow. Examples include:
- Multiple "refactor" commits
- Duplicate commits (e.g., "destroy the god object" appears twice)
- Uninformative commits (e.g., "🤨🤨🤨🤨🤨🤨🤨")
- Excessive merge commits

## Solution Overview
To clean up the commit history, you need to use interactive rebase to squash related commits into meaningful, consolidated commits.

## ⚠️ Important Warning
**Squashing commits rewrites git history and requires force pushing**. This should only be done:
1. By the repository owner or someone with admin permissions
2. After coordinating with all contributors to avoid conflicts
3. When everyone working on the repository is aware of the history rewrite

## Steps to Squash Commits

### Option 1: Squash Everything into One Commit (Simplest)

If you want to consolidate all changes into a single commit:

```bash
# 1. Ensure you're on master branch with latest changes
git checkout master
git pull origin master

# 2. Find the very first commit (or a stable base commit)
git log --oneline --reverse | head -1

# 3. Create a new orphan branch starting fresh
git checkout --orphan new-master

# 4. Add all current files
git add -A

# 5. Create a single comprehensive commit
git commit -m "DownPour: Complete rain simulation system with Vulkan rendering

This commit includes:
- Vulkan-based rendering engine
- Weather system with rain simulation
- Model loading and material management
- Entity-component system
- Windshield rain effects with wipers
- Camera system
- Comprehensive documentation"

# 6. Delete old master and rename new branch
git branch -D master
git branch -m master

# 7. Force push to replace remote master
git push -f origin master
```

### Option 2: Squash Recent Related Commits (Recommended)

To squash just the recent redundant commits while keeping older history:

```bash
# 1. Ensure you're on master with latest changes
git checkout master
git pull origin master

# 2. Identify where to start squashing (e.g., last 20 commits)
git log --oneline -20

# 3. Start interactive rebase from a base commit
# For example, if you want to squash the last 15 commits:
git rebase -i HEAD~15

# 4. In the editor that opens, you'll see something like:
#    pick abc1234 destroy the god object
#    pick def5678 refactor
#    pick ghi9012 Add Entity Design
#    ...
#
#    Change 'pick' to 'squash' (or 's') for commits you want to combine:
#    pick abc1234 destroy the god object
#    squash def5678 refactor
#    squash ghi9012 Add Entity Design
#    ...
#
#    The first commit stays as 'pick', and subsequent related commits
#    should be marked as 'squash' to combine them

# 5. Save and close the editor

# 6. In the next editor, combine/edit the commit messages
#    Delete redundant messages and create a clear, comprehensive message

# 7. Force push the rebased history
git push -f origin master
```

### Option 3: Squash by Feature/Topic

Group commits by logical features and squash each group:

```bash
# 1. Start interactive rebase
git rebase -i HEAD~30

# 2. Organize commits by topic, for example:
#    pick abc1234 Part 1: Remove dead renderer stub files
#    squash def5678 Part 2: Simplify Model.cpp material handling
#    squash ghi9012 Part 3: Implement RainSystem
#    squash jkl3456 Part 4: Implement WindshieldSurface
#    squash mno7890 Part 5: Update windshield_rain.frag shader
#    squash pqr1234 Part 6: Integrate WindshieldSurface
#    pick stu5678 Add comprehensive implementation docs
#    squash vwx9012 Address code review feedback
#    pick yza3456 create logging system for app
#    squash bcd7890 create python loader for materials
#    squash efg1234 Add Entity Design
#
#    This creates logical commits like:
#    - "Implement rain simulation system"
#    - "Add implementation documentation and review fixes"
#    - "Add entity system and asset loading"

# 3. Edit commit messages to be clear and descriptive
# 4. Force push
git push -f origin master
```

## Alternative: Keep History, Clean Up Going Forward

If you don't want to rewrite history, you can:

1. Accept the current messy history as-is
2. Create a tag marking the "before cleanup" point: `git tag before-cleanup`
3. Adopt strict commit message conventions going forward
4. Use squash merges for future pull requests

## Recommended Commit Message Convention

Going forward, use clear, descriptive commit messages:

```
<type>: <short description>

<optional longer description>

Examples:
- feat: Add rain particle physics simulation
- fix: Correct windshield wiper animation timing
- refactor: Simplify model material loading
- docs: Add architecture documentation
- test: Add unit tests for weather system
```

## After Squashing

Once history is rewritten, all contributors need to:

```bash
# Discard local master and get the new history
git checkout master
git fetch origin
git reset --hard origin/master
```

## Tools to Help

- **Git GUI tools**: GitKraken, SourceTree, or GitHub Desktop make interactive rebasing easier
- **Git commit message linter**: husky + commitlint to enforce good commit messages
- **Pre-commit hooks**: Automatically check commit message format

## Questions?

If you have questions about squashing commits, refer to:
- [Git rebase documentation](https://git-scm.com/docs/git-rebase)
- [Atlassian Git tutorial on rewriting history](https://www.atlassian.com/git/tutorials/rewriting-history)
