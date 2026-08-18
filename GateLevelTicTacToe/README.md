# Gate-Level Tic-Tac-Toe and Sequential Logic in Verilog

A structural Verilog project that builds up from primitive logic gates to a complete,
playable Tic-Tac-Toe game engine. Nothing here uses behavioural shortcuts for the
combinational logic — the decoders, win-detection network and move arbitration are all
written with `and` / `or` / `not` gate primitives, and the only behavioural constructs are
the clocked `always` blocks that define state elements.

Built with Icarus Verilog. Every module ships with a self-checking testbench.

## Design Hierarchy

```
TBox            3x3 game engine: turn arbitration, move validation, win/draw detection
 ├── TCell            single write-once cell (valid + symbol), synchronous reset
 └── row_col_decoder  (row, col) -> 9-bit one-hot cell select
      └── decoder     2-to-4 one-hot decoder, built from gate primitives

RIPPLE_COUNTER  4-bit asynchronous counter
 └── D_FF_ED         edge-triggered D flip-flop
      └── D_a             level-sensitive D latch

D_FF_MS         master-slave D flip-flop
 └── D_a              level-sensitive D latch (x2, on opposite clock phases)
```

## Components

### `D_a` — Level-Sensitive D Latch
The foundational storage element. Transparent while `en` is high, holds its value when
`en` is low, with an active-low asynchronous `rstn`. Both flip-flop styles below are
composed from this single primitive.

### `D_FF_MS` — Master-Slave D Flip-Flop
Two `D_a` latches chained on opposite clock phases. The master is transparent while
`CLK` is high and the slave while `CLK` is low, so data advances one stage per half
cycle and the output updates on the **falling** edge. This structure is what makes the
flip-flop immune to the transparency problem a single latch has.

### `D_FF_ED` — Edge-Triggered D Flip-Flop
Uses a gate-delay pulse generator rather than a second latch:

```verilog
not (nc, CLK);          // nc = ~CLK, but delayed by one gate
and (inp, CLK, nc);     // narrow enable pulse on the rising edge
D_a uut (D, inp, RESET, Q);
```

In steady state `CLK & ~CLK` is zero, so the latch is closed. On a rising clock edge the
inverter's propagation delay means `nc` is briefly still `1` while `CLK` has already gone
to `1`, opening the latch for a sliver of time. That transient is exactly the edge-trigger.
It is a nice demonstration that "edge-triggered" is a *timing* property built out of
level-sensitive parts, not a primitive.

### `RIPPLE_COUNTER` — 4-Bit Asynchronous Counter
Four `D_FF_ED` stages wired in toggle configuration (`D = ~Q`), where each stage is
clocked by the **previous stage's output** rather than a common clock. The counter runs
the full `0000 → 1111` sequence and wraps, with an asynchronous reset. Being a ripple
design, each stage's transition is delayed by the one before it — the classic
area-versus-propagation-delay tradeoff against a synchronous counter.

### `decoder` / `row_col_decoder` — Cell Addressing
`decoder` is a 2-to-4 one-hot decoder built from gate primitives. `row_col_decoder`
instantiates two of them and ANDs the row and column lines pairwise to produce a 9-bit
one-hot select — the standard row/column crossbar used to address a 2D array with only
`O(√n)` decode logic.

### `TCell` — Write-Once Game Cell
Holds `valid` (is the cell occupied) and `symbol` (X or O). Once written it latches its
value and ignores further `set` pulses until reset, which is what enforces "you cannot
play on an occupied square" in hardware rather than in control logic. Reset is
synchronous.

### `TBox` — Game Engine
The top-level module, and where the interesting logic lives:

- **Move decoding** — `row_col_decoder` converts the `(row, col)` input to a one-hot
  cell select.
- **Move validation** — a move is accepted only if the target cell is empty and the board
  is not locked; `new_move` is ORed across all nine cells to produce a single
  `move_valid` signal.
- **Turn arbitration** — a `toggle` register flips the active player, but *only when a
  move was actually valid*. This is the subtle part: a player who clicks an occupied
  square does not forfeit their turn.
- **Win detection** — a purely combinational network of 16 AND gates covering the eight
  winning lines for each player, checking `symbol` and `valid` together so that three
  empty cells never register as a win for O.
- **Draw and lock** — a draw is `all_cells_filled & ~x_wins & ~o_wins`. Once
  `game_state` leaves `00`, `lock_board` gates every subsequent `set`, freezing the
  final position.

`game_state` encodes: `00` game on, `01` X won, `10` O won, `11` draw.

## Building and Running

Requires [Icarus Verilog](https://steveicarus.github.io/iverilog/).

```bash
make test          # build and run all five testbenches
make ms            # master-slave D flip-flop
make ed            # edge-triggered D flip-flop
make counter       # 4-bit ripple counter
make tcell         # single game cell
make tbox          # full game engine
make clean
```

Without `make`, any testbench can be run directly:

```bash
iverilog -I src -o tbox.vvp tb/TBox_tb.v
vvp tbox.vvp
```

The `-I src` flag matters — the testbenches use `` `include `` and rely on `src/` being on
the include path.

## Verification

All five testbenches are self-checking and currently pass:

| Testbench | Module under test | Result |
|---|---|---|
| `Q1a_tb.v` | `D_FF_MS` | 0 errors across directed D transitions and rapid toggling |
| `Q1b_tb.v` | `D_FF_ED` | 0 errors across directed D transitions and rapid toggling |
| `Q2_tb.v` | `RIPPLE_COUNTER` | full `0000`–`1111` sweep, wrap, and mid-count reset |
| `TCell_tb.v` | `TCell` | 7/7 — write-once behaviour, reset precedence, rapid toggling |
| `TBox_tb.v` | `TBox` | 3/3 — initial state, an O win, an X win |

`TBox_tb.v` renders the board to stdout after each move:

```
Board State: X won
X X X
_ O _
_ O _
-------------
```

The flip-flop testbenches also dump VCD waveforms (`$dumpfile`), viewable in GTKWave.

## Notes

The `RIPPLE_COUNTER` in the original lab submission clocked its fourth stage from its own
output instead of the third stage, which capped the count at `0111`. That is corrected
here, so the counter is a true 4-bit design.

Course assignment specification: [`docs/Lab9.pdf`](docs/Lab9.pdf).
