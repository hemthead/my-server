#!/bin/sh

# compile binary
test=$(gcc -Wall -Werror src/main.c -o ./out/bin)
if [ $PIPESTATUS -eq 1 ]; then
  printf "\r\nmake.sh: encountered compile error, exiting early\r\n"
  exit
fi

# handle arguments
for arg; do
  case $arg in
    run) # run the binary after compiling, passing on arguments
      runargs="$@"
      #runargs=$(echo "$runargs" | cut -c 5-)
      runargs=${runargs#*run} # everything after "run" is an argument for the binary
      printf "make.sh: running \"./out/bin$runargs\":\r\n\r\n" # run binary
      ./out/bin ${runargs}
      break # stop reading arguments after running
      ;;
    *)
      printf "make.sh: hmm... what's that? I couldn't hear you (argument: $arg)\r\n"
      ;;
  esac
done
