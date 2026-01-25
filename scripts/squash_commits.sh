#!/bin/bash
# Commit Squashing Helper Script for DownPour Repository
# 
# This script helps squash redundant commits in the repository.
# Run with --help for usage information.

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Commit Squashing Helper Script
===============================

Usage: $0 [OPTIONS]

Options:
    --squash-all        Squash all commits into a single commit
    --squash-recent N   Squash the last N commits
    --analyze          Analyze commit history and show recommendations
    --help             Show this help message

Examples:
    $0 --analyze                    # Analyze current commit history
    $0 --squash-recent 15          # Squash last 15 commits
    $0 --squash-all                # Squash all commits into one

⚠️  WARNING: This script rewrites git history and requires force push!
    - Coordinate with all contributors before running
    - Make sure everyone is aware of the history rewrite
    - Have a backup of your repository

EOF
}

# Function to check if in git repository
check_git_repo() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository"
        exit 1
    fi
}

# Function to check for uncommitted changes
check_clean_working_tree() {
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        print_error "You have uncommitted changes. Please commit or stash them first."
        git status --short
        exit 1
    fi
}

# Function to analyze commit history
analyze_commits() {
    print_info "Analyzing commit history..."
    echo ""
    
    # Count total commits
    total_commits=$(git rev-list --count HEAD)
    print_info "Total commits: $total_commits"
    echo ""
    
    # Find redundant commit messages
    print_info "Finding duplicate commit messages..."
    git log --pretty=format:"%s" | sort | uniq -c | sort -rn | head -20
    echo ""
    
    # Show commits with uninformative messages
    print_info "Commits with potentially uninformative messages (short or symbols only)..."
    git log --oneline | grep -E '^[a-f0-9]+ .{1,15}$' | head -20
    echo ""
    
    # Show recent merge commits
    print_info "Recent merge commits:"
    git log --oneline --merges | head -10
    echo ""
    
    # Recommendations
    echo ""
    print_warning "RECOMMENDATIONS:"
    echo "1. Consider squashing duplicate commits (shown above)"
    echo "2. Review short/uninformative commits and combine them with related changes"
    echo "3. Squash sequential refactor commits into meaningful groups"
    echo ""
    echo "To squash commits, run:"
    echo "  $0 --squash-recent <number>    # Squash recent N commits"
    echo "  $0 --squash-all               # Squash all commits into one"
    echo ""
}

# Function to squash all commits
squash_all_commits() {
    print_warning "This will squash ALL commits into a single commit!"
    print_warning "This is a DESTRUCTIVE operation that rewrites history."
    echo ""
    read -p "Are you sure you want to continue? (type 'yes' to confirm): " confirmation
    
    if [ "$confirmation" != "yes" ]; then
        print_info "Operation cancelled"
        exit 0
    fi
    
    print_info "Creating backup branch..."
    backup_branch="backup-before-squash-$(date +%Y%m%d-%H%M%S)"
    git branch "$backup_branch"
    print_info "Created backup branch: $backup_branch"
    
    print_info "Squashing commits..."
    
    # Get current branch
    current_branch=$(git rev-parse --abbrev-ref HEAD)
    
    # Create new orphan branch
    git checkout --orphan temp-squash
    
    # Add all files
    git add -A
    
    # Get project name (repo name)
    repo_name=$(basename "$(git rev-parse --show-toplevel)")
    
    # Create comprehensive commit message
    read -p "Enter commit message title [DownPour: Complete rain simulation system]: " commit_title
    commit_title=${commit_title:-"DownPour: Complete rain simulation system"}
    
    cat > /tmp/commit_msg << EOF
$commit_title

This commit consolidates the entire project history into a single commit.
Previous history is preserved in branch: $backup_branch

Project includes:
- Vulkan-based rendering engine
- Weather system with rain simulation
- Model loading and material management
- Entity-component system
- Windshield rain effects with wipers
- Camera system
- Comprehensive documentation
EOF
    
    git commit -F /tmp/commit_msg
    
    # Replace old branch
    git branch -D "$current_branch"
    git branch -m "$current_branch"
    
    print_info "Commits squashed successfully!"
    print_warning "To push changes, you need to force push:"
    echo "    git push -f origin $current_branch"
    echo ""
    print_info "Backup branch created: $backup_branch"
}

# Function to squash recent N commits
squash_recent_commits() {
    num_commits=$1
    
    if [ -z "$num_commits" ] || ! [[ "$num_commits" =~ ^[0-9]+$ ]]; then
        print_error "Invalid number of commits: $num_commits"
        exit 1
    fi
    
    total_commits=$(git rev-list --count HEAD)
    if [ "$num_commits" -ge "$total_commits" ]; then
        print_error "Cannot squash $num_commits commits (total: $total_commits)"
        print_info "Use --squash-all to squash all commits"
        exit 1
    fi
    
    print_info "Will squash the last $num_commits commits"
    print_info "Showing commits that will be squashed:"
    echo ""
    git log --oneline -"$num_commits"
    echo ""
    
    print_warning "This is a DESTRUCTIVE operation that rewrites history."
    read -p "Continue? (type 'yes' to confirm): " confirmation
    
    if [ "$confirmation" != "yes" ]; then
        print_info "Operation cancelled"
        exit 0
    fi
    
    print_info "Creating backup branch..."
    backup_branch="backup-before-squash-$(date +%Y%m%d-%H%M%S)"
    git branch "$backup_branch"
    print_info "Created backup branch: $backup_branch"
    
    print_info "Starting interactive rebase..."
    print_info "In the editor:"
    echo "  1. The first commit will be 'pick'"
    echo "  2. Change subsequent commits from 'pick' to 'squash' (or 's')"
    echo "  3. Save and close"
    echo "  4. Edit the combined commit message in the next editor"
    echo ""
    read -p "Press Enter to continue..."
    
    # Start interactive rebase
    git rebase -i HEAD~"$num_commits"
    
    if [ $? -eq 0 ]; then
        print_info "Rebase completed successfully!"
        current_branch=$(git rev-parse --abbrev-ref HEAD)
        print_warning "To push changes, you need to force push:"
        echo "    git push -f origin $current_branch"
        echo ""
        print_info "Backup branch created: $backup_branch"
    else
        print_error "Rebase failed. You can abort with: git rebase --abort"
        exit 1
    fi
}

# Main script
main() {
    check_git_repo
    
    case "$1" in
        --help)
            show_usage
            ;;
        --analyze)
            analyze_commits
            ;;
        --squash-all)
            check_clean_working_tree
            squash_all_commits
            ;;
        --squash-recent)
            check_clean_working_tree
            if [ -z "$2" ]; then
                print_error "Missing number of commits to squash"
                show_usage
                exit 1
            fi
            squash_recent_commits "$2"
            ;;
        *)
            print_error "Unknown option: $1"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Run main function if script is executed (not sourced)
if [ "${BASH_SOURCE[0]}" == "${0}" ]; then
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    main "$@"
fi
