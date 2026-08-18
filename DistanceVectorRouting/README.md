# Distance Vector Routing with Weighted Links and Poisoned Reverse

A discrete-round simulator for the Distance Vector Routing (DVR) protocol, extended from a
hop-count baseline into a weighted-cost implementation with RIP-style **poisoned reverse**, plus a
link-failure harness that makes the **count-to-infinity** problem reproducible on demand.

The simulator models a network as a set of nodes, each owning one or more named IP interfaces.
Every round, each node builds a *neighbour-specific* distance vector and hands it to its neighbours;
receivers run the Bellman-Ford relaxation and rewrite their routing tables. Rounds repeat until no
table changes — that fixed point is convergence, and the round count is the headline measurement.

Built for CS3205 (Computer Networks), IIT Madras.

## What this extends

The provided starter code computed shortest paths by hop count only, over a fixed topology, with no
failure handling. Three extensions were implemented on top of it:

**1. Weighted links.** The input parser reads a per-link cost, `NetInterface` carries that cost, and
the relaxation step uses `new_cost = advertised_cost + link_cost` instead of `advertised_cost + 1`.
Shortest paths are therefore minimum-total-cost rather than minimum-hop.

**2. Poisoned reverse.** `sendMsg()` no longer broadcasts one table to everybody. It composes a
separate distance vector per neighbour: if a route to destination `D` currently uses neighbour `Y`
as its next hop, the copy sent to `Y` advertises `D` at infinity. `recvMsg()` gains the matching
rule — an update arriving *from the current next hop* is always accepted, even when it makes the
route worse. Without that rule a node would cling to a stale cheap cost and the poison would have no
effect.

**3. Link failure simulation.** The input format accepts an optional trailing line naming two nodes
whose link should be severed after initial convergence. The link cost is driven to infinity on both
endpoints and the algorithm is re-run, so reconvergence can be measured separately from the initial
convergence.

Infinity is `16`, matching RIPv1/v2 (RFC 2453). Beyond signalling unreachability, it is what bounds
the count-to-infinity loop: costs inflate one round at a time until they cross the ceiling and the
route is finally dropped.

## Build and run

Requires a C++11 compiler. `make` builds `rip`:

```bash
make
```

Or directly, without make:

```bash
g++ -std=c++11 -Iinclude src/main.cpp src/routing_algo.cpp -o rip
```

The simulator reads a topology on stdin, then prompts for a routing mode (`1` = poisoned reverse
on, `0` = off). To run non-interactively, append the choice to the input:

```bash
{ cat tests/input/test7.txt; echo 1; } | ./rip     # poisoned reverse
{ cat tests/input/test7.txt; echo 0; } | ./rip     # naive DVR, count-to-infinity
```

## Input format

```
4                             # number of nodes
A B C D                       # node labels
A 10.0.0.1 10.0.0.2 B 100     # <node> <own-iface-ip> <peer-iface-ip> <peer-node> <cost>
B 10.0.0.2 10.0.0.1 A 100
B 10.0.1.1 10.0.1.2 C 1
C 10.0.1.2 10.0.1.1 B 1
C 10.0.2.1 10.0.2.2 D 1
D 10.0.2.2 10.0.2.1 C 1
A 10.0.3.1 10.0.3.2 D 2
D 10.0.3.2 10.0.3.1 A 2
B C                           # optional: sever this link after convergence
EOE
```

Each physical link is declared twice, once from each endpoint, so that both directions get their own
interface entry. A line of two bare node labels (no dots, so unambiguous against an IP) is read as
the link-failure instruction. `EOE` ends the topology.

The cost column is **required** — see [Starter baseline](#starter-baseline) regarding the original
unweighted sample files.

## Output format

Each node prints its routing table sorted by cost, then by destination IP:

```
A:
10.0.0.1 | 10.0.0.1 | 10.0.0.1 | 0
10.0.0.21 | 10.0.0.21 | 10.0.0.1 | 2
10.0.1.23 | 10.0.0.21 | 10.0.0.1 | 2
10.0.1.3 | 10.0.0.21 | 10.0.0.1 | 7
```

Columns are `destination ip | next hop | outgoing interface | total cost`. The sort makes output
deterministic for a given input, which is what lets the test harness diff against reference files.

## Test harness

Eleven topologies live in `tests/input/`: chains, rings, a triangle, a chorded chain, and edge cases
— a two-node network, two inputs whose failure instruction names a non-adjacent pair, and one with
no failure instruction at all (all three exercise the "nothing to sever" path).
`scripts/run_tests.sh` runs each one in both modes and diffs against the reference outputs in
`tests/expected/`:

```bash
make test                        # or: ./scripts/run_tests.sh
./scripts/run_tests.sh --regen   # rewrite the reference outputs
```

## Results

Rounds to reconverge after the link failure, both modes, same topology:

| Topology | Shape | Link cut | Initial | Poisoned reverse | Naive DVR |
|---|---|---|---|---|---|
| test1  | 3-node chain          | B–C | 3 | 3 | 2 |
| test3  | 4-node ring           | A–B | 3 | 1 | 1 |
| test4  | 2-node link           | X–Y | 2 | 2 | 2 |
| test5  | 5-node, chord B–D     | C–D | 5 | 4 | 6 |
| test6  | 3-node triangle       | A–B | 2 | 2 | 3 |
| **test7** | **4-node ring, cost-skewed** | **B–C** | **3** | **2** | **10** |
| test9  | 4-node ring           | Q–R | 3 | 3 | 3 |
| test10 | 6-node ring           | C–D | 3 | 4 | 5 |

Poisoned reverse is not uniformly faster — on small or well-connected topologies both modes settle
in the same couple of rounds, and on test1 the naive run happens to finish sooner. It pays off
exactly where the failure strands a node behind a broken link and a neighbour still holds a stale
route back through it.

### Count-to-infinity, observed

`test7` is the clean demonstration. Four nodes in a ring — A–B, B–C, C–D, D–A — with the A–B link
deliberately priced at 100 while the rest cost 1, 1 and 2. After initial convergence every node
reaches B through the cheap side of the ring: C is B's direct neighbour at cost 1, D reaches B via C
at cost 2, and A reaches B via D at cost 4 rather than paying 100 for the direct link.

Cutting B–C removes the only cheap way in. The one surviving path is the 100-cost A–B link, which is
far past the infinity ceiling of 16, so B should simply become unreachable — and both modes do end
up agreeing that it is.

With poisoned reverse, they agree in **2 rounds**. D's route to B ran through C, so D advertises B to
C at infinity; A's route to B ran through D, so A advertises B to D at infinity. Nobody is holding
out a path that loops back through the node it is being offered to, so the bad news propagates in
one sweep.

With it disabled, D keeps advertising to C the cost-2 route to B *that runs through C*. C believes
it and installs a path to B via D; D then hears C's new cost and raises its own; A joins in from the
far side. The cost ratchets upward a link at a time around the ring, and the loop only breaks when
the accumulated cost crosses 16 and the route is finally discarded — **10 rounds**, a 5x slowdown to
reach an identical final state.

That is the whole argument for poisoned reverse in one topology. Both modes converge, and converge
to the same tables. The difference is how many rounds the network spends believing in a route that
does not exist — and in a real deployment, how long it spends black-holing traffic into it.

## Layout

```
├── src/
│   ├── main.cpp           topology parser, mode selection, failure driver
│   └── routing_algo.cpp   round loop, convergence detection, recvMsg relaxation
├── include/
│   └── node.h             Node/NetInterface/RoutingEntry, sendMsg + poisoned reverse
├── tests/
│   ├── input/             11 topologies
│   ├── expected/          reference output, both modes
│   └── starter-baseline/  original unweighted sample (see below)
├── scripts/run_tests.sh   diff-based regression runner
├── docs/                  assignment brief and submitted report
└── Makefile
```

## Starter baseline

`tests/starter-baseline/` holds the unweighted `sampleinput.txt` / `sampleoutput.txt` that shipped
with the assignment, kept for provenance. They are **not** runnable against this version: the
extended parser requires the cost column, and feeding it a four-field link line puts `cin` into a
fail state that never clears. They are excluded from the test harness for that reason.

## Notes

- `docs/report.pdf` is the report submitted for the course. It describes an earlier run of this code
  that used `999` as infinity over a different sample topology, so its round counts do not line up
  with the table above — those numbers come from the current source, where infinity is `16`.
- When a route's next hop sits behind a link that has been driven to infinity, the recorded cost can
  land slightly above 16 (e.g. `17`, `19`) rather than being clamped exactly at the ceiling. Any
  value at or above 16 means unreachable; real RIP clamps, this does not.
- The third column (`ip_interface`) is written when a routing entry is first created and is not
  rewritten when the entry later switches next hop, so it can name an interface that no longer
  corresponds to the current path. This is inherited from the starter code's `recvMsg`; the
  destination, next hop and cost columns — the ones the algorithm actually reasons about — are
  correct.
