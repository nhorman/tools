#!/bin/sh

trap "rm -f *.dat *.cmds" EXIT

rm -f rculocktime*.dat
rm -f rwlocktime*.dat
rm -f *.svg
rm -f *.cmds

for i in 0 1 2 4 8
do
    for j in 16 32 64 
    do
        THREADCOUNT=$j
        export LOCK_WRITERS=$i
        let READERS=$j-$i
        if [ $LOCK_WRITERS -eq $THREADCOUNT ]
        then
            continue
        fi
        echo "$READERS readers, $i writers"
        RESULTS=$(../rwlocks --terse $THREADCOUNT)
        echo $READERS $LOCK_WRITERS $RESULTS >> ./rwlocktime_${LOCK_WRITERS}_writers.dat
        RESULTS=$(../rculocks --terse $THREADCOUNT)
        echo $READERS $LOCK_WRITERS $RESULTS >> ./rculocktime_${LOCK_WRITERS}_writers.dat
    done
done

cat > _gnuplot.cmds <<EOF

set terminal svg font "Verdana,12" enhanced 
set yrange [0:*]
set xlabel 'readers'
set ylabel 'time per read lock/unlock (usecs)'
set key out horizontal
set style line 1 lt 1 lw 3 pt 3 linecolor rgb "#ff0000"
set style line 2 lt 1 lw 3 pt 3 linecolor rgb "#ff8000"
set style line 3 lt 1 lw 3 pt 3 linecolor rgb "#ffff00"
set style line 4 lt 1 lw 3 pt 3 linecolor rgb "#3399ff"
set style line 5 lt 1 lw 3 pt 3 linecolor rgb "#3333ff"
set style line 6 lt 1 lw 3 pt 3 linecolor rgb "#9933ff"
set style line 7 lt 1 lw 3 pt 3 linecolor rgb "#ff33ff"
set style line 8 lt 1 lw 3 pt 3 linecolor rgb "#ff3399"
set style line 9 lt 1 lw 3 pt 3 linecolor rgb "#ff3333"
set style line 10 lt 1 lw 3 pt 3 linecolor rgb "#ff9933"

set output 'write_lock_time.svg'
set title "write lock time vs reader threads"
plot "rculocktime_0_writers.dat" using 1:3 w l ls 9 title "rcu(0writers)", \
"rwlocktime_0_writers.dat" using 1:3 w l ls 10 title "rw(0writers)", \
"rculocktime_1_writers.dat" using 1:3 w l ls 1 title "rcu(1writers)", \
"rwlocktime_1_writers.dat" using 1:3 w l ls 2 title "rw(1writers)", \
"rculocktime_2_writers.dat" using 1:3 w l ls 3 title "rcu(2writers)", \
"rwlocktime_2_writers.dat" using 1:3 w l ls 4 title "rw(2writers)", \
"rculocktime_4_writers.dat" using 1:3 w l ls 5 title "rcu(4writers)", \
"rwlocktime_4_writers.dat" using 1:3 w l ls 6 title"rw(4writers)", \
"rculocktime_8_writers.dat" using 1:3 w l ls 7 title "rcu(8writers)", \
"rwlocktime_8_writers.dat" using 1:3 w l ls 8 title "rw(8writers)",

set output 'read_lock_time.svg'
set title "read lock time vs reader threads"
plot "rculocktime_0_writers.dat" using 1:4 w l ls 9 title "rcu(0writers)", \
"rwlocktime_0_writers.dat" using 1:4 w l ls 10 title "rw(0writers)", \
"rculocktime_1_writers.dat" using 1:4 w l ls 1 title "rcu(1writers)", \
"rwlocktime_1_writers.dat" using 1:4 w l ls 2 title "rw(1writers)", \
"rculocktime_2_writers.dat" using 1:4 w l ls 3 title "rcu(2writers)", \
"rwlocktime_2_writers.dat" using 1:4 w l ls 4 title "rw(2writers)", \
"rculocktime_4_writers.dat" using 1:4 w l ls 5 title "rcu(4writers)", \
"rwlocktime_4_writers.dat" using 1:4 w l ls 6 title"rw(4writers)", \
"rculocktime_8_writers.dat" using 1:4 w l ls 7 title "rcu(8writers)", \
"rwlocktime_8_writers.dat" using 1:4 w l ls 8 title "rw(8writers)"

EOF

gnuplot ./_gnuplot.cmds
