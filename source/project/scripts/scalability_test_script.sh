#!/bin/bash

#===============================================================================
# MPI Scalability Test
#
# 1. Sweep over total MPI ranks: 2, 4, 8, 16, 32  
# 2. Use a fixed epoch count: 100000  
# 3. Test grid sizes: 100×100, 200×200, 400×400, 800×800, 1600×1600, 3200×3200  
# 4. For each run, record:
#      • timestamp
#      • actual random seed used by rank 0
#      • total ranks
#      • rows, cols
#      • epochs (100000)
#      • stop generation (steady, all-dead, or total)
#      • elapsed wall‐clock time (s)
#===============================================================================

set -euo pipefail

#--------------------------------------
# Parameter definitions
#--------------------------------------
EPOCHS=(100000)
PROCS=(2 4 8 16 32)
ROWS=(100 200 400 800 1600 3200)
COLS=(100 200 400 800 1600 3200)

HOSTFILE="$HOME/mpi_hosts.txt"
BINARY="./game_of_life"
OUTPUT="results.csv"

#--------------------------------------
# CSV header
#--------------------------------------
echo "timestamp,seed,procs,rows,cols,epochs,stop,elapsed_s" > "$OUTPUT"

#--------------------------------------
# Run the sweep
#--------------------------------------
for np in "${PROCS[@]}"; do
  for max_epoch in "${EPOCHS[@]}"; do
    for r in "${ROWS[@]}"; do
      for c in "${COLS[@]}"; do

        ts=$(date +%Y-%m-%dT%H:%M:%S)
        TMP=$(mktemp)

        # Run MPI job under /usr/bin/time, tee stdout to TMP, capture last line as elapsed time
        elapsed=$(
          /usr/bin/time -f "%e" \
            mpirun --hostfile "$HOSTFILE" -np "$np" "$BINARY" "-n $r" "-m $c" "-e $max_epoch" 2>&1 \
            | tee "$TMP" \
            | tail -n1
        )

        # Extract the actual seed used by rank 0
        # Format printed by your main.c: "Using base seed: <user_seed> (rank 0 uses <seed>)"
        seed_line=$(grep '^Using base seed:' "$TMP" || true)
        seed=$(echo "$seed_line" | sed -E 's/.*rank 0 uses ([0-9]+).*/\1/')
        seed=${seed:-NA}

        # Determine stop generation
        if grep -q '^Reached steady state at generation' "$TMP"; then
          stop=$(grep '^Reached steady state at generation' "$TMP" \
                   | awk '{print $5}')
        elif grep -q '^All cells are dead at generation' "$TMP"; then
          stop=$(grep '^All cells are dead at generation' "$TMP" \
                   | awk '{print $6}' | tr -d ',')
        else
          stop=$max_epoch
        fi

        # Append results
        echo "$ts,$seed,$np,$r,$c,$max_epoch,$stop,$elapsed" >> "$OUTPUT"
        echo "[$ts] procs=$np grid=${r}x${c} EPOCHS=$max_epoch seed=$seed stop=$stop time=${elapsed}s"

        rm "$TMP"

      done
    done
  done
done

echo "All tests completed. Results saved in $OUTPUT"
