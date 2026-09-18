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

if [ -v release ]; then
  opt="gpu_release.opt"
else
  opt="gpu_dev.opt"
fi

defines="-DGPU_=1"
out="$bindir/$name.spv"

if [ -v loop ]; then
  while : ; do
    inotifywait -e close_write ./$name.c &>/dev/null
    glslangValidator -S comp $defines -V $name.c \
    --glsl-version 460 \
    --target-env spirv1.5 \
    -o $out.bloat &&
      spirv-opt -o $out.tmp $out.bloat --skip-validation -Oconfig=$opt &&
        mv $out.tmp $out && echo "$name: ok"
  done
else
  glslangValidator -S comp -DGPU_=1 -V $name.c \
  --glsl-version 460 \
  --target-env spirv1.5 -o $out.bloat &&
    spirv-opt -o $out.tmp $out.bloat --skip-validation -Oconfig=$opt &&
      mv $out.tmp $out && echo "$name: ok"
fi

