
for arg in "$@"; do declare $arg='1'; done

bindir="$PWD/bin"
mkdir -p $bindir

defines="-DCPU_=1"

if [ -v game ]; then
  name="game"
elif [ -v quick ]; then
  name="quick"
else
  echo "Error: Please specify one of the follwing builds: { game, quick }"
  exit 1
fi

if ! [ -v release ]; then defines="-DDEV_=1" && opt="-O0 -g";
else defines="$defines" && opt="-O3"; fi;

if [ -v lnx ]; then defines="$defines -DCPU_=1 -DLNX_=1"; fi
if [ -v cleanup ]; then defines="$defines -DCLEANUP=1"; fi

warn_flags="-Wno-cpp"
stdver="c99"
libs="-ldl"
flags="--std=$stdver $opt $warn_flags $libs"
out="$bindir/$name"

if [ -v show_cmd ]; then
  echo "build cmd: $CC $defines $flags ./$name.c -o $out"
fi

if [ -v loop ]; then
  while : ; do
    inotifywait -e close_write ./$name.c &>/dev/null
    $CC $defines $flags ./$name.c -o $out &&
      echo "$name: ok" && if [ -v run ]; then $out; fi || continue
  done
else
  $CC $defines $flags ./$name.c -o $out &&
    if [ -v run ]; then $out; fi
fi
