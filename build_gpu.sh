#!/usr/bin/env bash
for arg in "$@"; do declare $arg='1'; done

bindir="$PWD/bin"
mkdir -p $bindir


if [ -v game ]; then
  name="game"
elif [ -v quick ]; then
  name="quick"
else
  echo "Error: Please specify one of the follwing builds: { game, quick }"
  exit 1
fi

defines="-DGPU_=1"
out="$bindir/$name.spv"

if [ -v loop ]; then
  echo "build looping not implemented yet"
  exit 1
else
  glslangValidator -S comp -DGPU_=1 -V $name.c --glsl-version 460 --target-env spirv1.5 -o $out
fi

