#!/usr/bin/env bash
# Check recent commit messages (subject lines) against the project's commit format.

SINCE=${1:-HEAD~20}

echo "Checking commit messages since: $SINCE"

commits=$(git rev-list --max-count=100 $SINCE 2>/dev/null)
if [ -z "$commits" ]; then
  # Try interpreting as a count
  if [[ "$SINCE" =~ ^[0-9]+$ ]]; then
    commits=$(git rev-list --max-count=$SINCE HEAD)
  fi
fi

# If repository has no commits (no HEAD), exit early
if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
  echo "Repository has no commits (no HEAD). Nothing to check." >&2
  exit 0
fi

if [ -z "$commits" ]; then
  echo "No commits found for: $SINCE"
  exit 0
fi

bad=()
for c in $commits; do
  msg=$(git log -n 1 --pretty=format:%s $c)
  echo "$msg" > /tmp/altina_commit_msg.txt
  if ! ./.githooks/commit-msg /tmp/altina_commit_msg.txt >/dev/null 2>&1; then
    bad+=("$c: $msg")
  fi
  rm -f /tmp/altina_commit_msg.txt
done

if [ ${#bad[@]} -eq 0 ]; then
  echo "All commit messages OK"
  exit 0
fi

echo "Found ${#bad[@]} commits with invalid messages:" >&2
for b in "${bad[@]}"; do
  echo "$b" >&2
done

exit 1
