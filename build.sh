for arg in "$@"; do declare $arg='1'; done

if [ -v cpu ]; then
  bash build_cpu.sh "$@"
elif [ -v gpu ]; then
  bash build_gpu.sh "$@"
else
  echo "Error: Please specify the build target as one of: { cpu, gpu }"
  exit 1
fi
