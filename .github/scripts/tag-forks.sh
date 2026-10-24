#!/bin/bash
# Tags the source trees of a release in the forks: cegcc-TAG on the
# submodule commits (gcc, binutils, mingw, w32api) of SRC, a checkout of
# TAG, so the commits a release builds from stay reachable whatever
# happens to the fork branches. A tag already on the same commit is left
# as it is; on another commit it is an error.
#
#   tag-forks.sh TAG SRC
#
# gh needs a token with contents: write on the four forks.
set -eu

TAG=$1 SRC=$2
NAME=cegcc-$TAG

# Up to v16.2.0-ce.1 the 16.2 trees were pinned in gcc16/Dockerfile and
# the submodules are the 9.3 trees; those tags are tagged in the forks.
if [ "${TAG#v16.}" != "$TAG" ] && [ -f "$SRC/gcc16/Dockerfile" ]; then
  echo "$TAG: trees pinned in gcc16/Dockerfile, not tagged here"
  exit 0
fi

for p in gcc binutils mingw w32api; do
  sha=$(git -C "$SRC" ls-tree HEAD "$p" | awk '$2 == "commit" {print $3}')
  url=$(git config -f "$SRC/.gitmodules" "submodule.$p.url" || true)
  if [ -z "$sha" ] || [ -z "$url" ]; then
    echo "tag-forks.sh: no submodule $p at $TAG" >&2
    exit 1
  fi
  repo=${url#https://github.com/}
  repo=${repo%.git}
  have=$(gh api "repos/$repo/git/ref/tags/$NAME" --jq .object.sha 2>/dev/null || true)
  if [ -z "$have" ]; then
    gh api "repos/$repo/git/refs" -f ref="refs/tags/$NAME" -f sha="$sha" > /dev/null
    echo "$repo: $NAME on ${sha:0:10}"
  elif [ "$have" = "$sha" ]; then
    echo "$repo: $NAME already on ${sha:0:10}"
  else
    echo "tag-forks.sh: $repo has $NAME on ${have:0:10}, not ${sha:0:10}" >&2
    exit 1
  fi
done
