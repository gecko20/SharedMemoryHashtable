#!gawk -f

# Reservoir sampling from https://stackoverflow.com/a/692401

# Waterman's Algorithm R for random sampling
# by way of Knuth's The Art of Computer Programming, volume 2

BEGIN {
    if (!n) {
        print "Usage: random_sampling.awk -v n=[size]"
        exit
    }
    t = n
    srand()
}

NR <= n {
    pool[NR] = $0
    places[NR] = NR
    next
}

NR > n {
    t++
    M = int(rand()*t) + 1
    if (M <= n) {
        READ_NEXT_RECORD(M)
    }
}

END {
    if (NR < n) {
        print "random_sampling.awk: Not enough records for sample" \
              > "/dev/stderr"
        exit
    }
    # gawk needs a numeric sort function
    # since it doesn't have one, zero-pad and sort alphabetically
    pad = length(NR)
    for (i in pool) {
        new_index = sprintf("%0" pad "d", i)
        newpool[new_index] = pool[i]
    }
    x = asorti(newpool, ordered)
    for (i = 1; i <= x; i++)
        print newpool[ordered[i]]
}

function READ_NEXT_RECORD(idx) {
    rec = places[idx]
    delete pool[rec]
    pool[NR] = $0
    places[idx] = NR
}

