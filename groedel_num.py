#!/usr/bin/env python

import re
from typing import List, Dict, Tuple, Optional


class TMGodelEncoder:
    def __init__(
        self,
        transitions: List[str],
        start_state: Optional[str] = None,
        accept_state: Optional[str] = None,
        state_order: Optional[List[str]] = None,
        symbol_order: Optional[List[str]] = None,
        blank_symbols: Optional[List[str]] = None,
    ):
        """
        transitions: Liste von Strings, z.B.
            "q0,0->q1,1,R"

        start_state:
            Falls None, wird versucht automatisch zu raten.

        accept_state:
            Falls None, wird versucht automatisch zu raten
            (z.B. Zustand mit 'accept' oder 'acc' im Namen).

        state_order:
            Explizite Reihenfolge der Zustände.
            Falls gegeben, wird diese für die Nummerierung verwendet.
            Trotzdem gilt:
              q1 = Startzustand
              q2 = akzeptierender Zustand
              Rest danach.

        symbol_order:
            Explizite Reihenfolge der zusätzlichen Symbole.
            Die ersten drei sind fest:
              X1 = 0
              X2 = 1
              X3 = Blank
            Danach kommen weitere Symbole.

        blank_symbols:
            Welche Zeichen als Blank gelten sollen.
        """
        self.transitions_raw = transitions
        self.blank_symbols = blank_symbols or ["_", "B", "blank", "Blank", "□", "␣"]

        self.parsed_transitions = [self.parse_transition(t) for t in transitions]

        self.start_state = start_state or self.guess_start_state()
        self.accept_state = accept_state or self.guess_accept_state()

        self.state_map = self.build_state_map(state_order)
        self.symbol_map = self.build_symbol_map(symbol_order)
        self.direction_map = {"L": 1, "R": 2}

    def parse_transition(self, t: str) -> Tuple[str, str, str, str, str]:
        """
        Erwartetes Format:
            "q0,0->q1,1,R"
        oder mit Spaces:
            "q0, 0 -> q1, 1, R"
        """
        pattern = r'^\s*([^,\s]+)\s*,\s*([^,\s]+)\s*->\s*([^,\s]+)\s*,\s*([^,\s]+)\s*,\s*([LR])\s*$'
        match = re.match(pattern, t)
        if not match:
            raise ValueError(f"Ungültiges Transition-Format: {t}")
        src_state, read_sym, dst_state, write_sym, direction = match.groups()
        return src_state, read_sym, dst_state, write_sym, direction

    def guess_start_state(self) -> str:
        """
        Heuristik:
        - wenn ein Zustand 'start' enthält -> nimm ihn
        - sonst nimm Quelle der ersten Transition
        """
        states = self.collect_states()
        for s in states:
            if "start" in s.lower():
                return s
        return self.parsed_transitions[0][0]

    def guess_accept_state(self) -> str:
        """
        Heuristik:
        - Zustand mit 'accept', 'acc', 'halt' im Namen
        - sonst Fehler, da laut Kodierung q2 der akzeptierende Zustand sein soll
        """
        states = self.collect_states()
        for s in states:
            sl = s.lower()
            if "accept" in sl or sl == "acc" or "halt" in sl:
                return s
        raise ValueError(
            "Kein akzeptierender Zustand gefunden. "
            "Bitte accept_state explizit angeben."
        )

    def collect_states(self) -> List[str]:
        states = set()
        for src, _, dst, _, _ in self.parsed_transitions:
            states.add(src)
            states.add(dst)
        return sorted(states)

    def collect_symbols(self) -> List[str]:
        symbols = set()
        for _, read_sym, _, write_sym, _ in self.parsed_transitions:
            symbols.add(read_sym)
            symbols.add(write_sym)
        return sorted(symbols)

    def normalize_blank(self, sym: str) -> str:
        if sym in self.blank_symbols:
            return "_"
        return sym

    def build_state_map(self, state_order: Optional[List[str]]) -> Dict[str, int]:
        all_states = self.collect_states()

        if self.start_state not in all_states:
            raise ValueError(f"Startzustand '{self.start_state}' kommt in den Transitionen nicht vor.")
        if self.accept_state not in all_states:
            raise ValueError(f"Akzeptierender Zustand '{self.accept_state}' kommt in den Transitionen nicht vor.")
        if self.start_state == self.accept_state:
            raise ValueError("Startzustand und akzeptierender Zustand dürfen hier nicht gleich sein.")

        remaining = [s for s in all_states if s not in {self.start_state, self.accept_state}]

        if state_order:
            # Nur die restlichen Zustände in der gegebenen Reihenfolge übernehmen
            provided_remaining = [s for s in state_order if s not in {self.start_state, self.accept_state}]
            missing = [s for s in remaining if s not in provided_remaining]
            extras = [s for s in provided_remaining if s not in remaining]
            if missing:
                raise ValueError(f"Folgende Zustände fehlen in state_order: {missing}")
            if extras:
                raise ValueError(f"Folgende Zustände in state_order kommen nicht vor: {extras}")
            ordered_remaining = provided_remaining
        else:
            ordered_remaining = sorted(remaining)

        ordered_states = [self.start_state, self.accept_state] + ordered_remaining
        return {state: i + 1 for i, state in enumerate(ordered_states)}

    def build_symbol_map(self, symbol_order: Optional[List[str]]) -> Dict[str, int]:
        all_symbols = {self.normalize_blank(s) for s in self.collect_symbols()}

        # feste Kodierung:
        # X1 = 0, X2 = 1, X3 = Blank
        symbol_map = {"0": 1, "1": 2, "_": 3}

        additional = [s for s in all_symbols if s not in {"0", "1", "_"}]

        if symbol_order:
            normalized_order = [self.normalize_blank(s) for s in symbol_order]
            filtered = [s for s in normalized_order if s not in {"0", "1", "_"}]

            missing = [s for s in additional if s not in filtered]
            extras = [s for s in filtered if s not in additional]
            if missing:
                raise ValueError(f"Folgende Symbole fehlen in symbol_order: {missing}")
            if extras:
                raise ValueError(f"Folgende Symbole in symbol_order kommen nicht vor: {extras}")
            additional = filtered
        else:
            additional = sorted(additional)

        next_num = 4
        for sym in additional:
            symbol_map[sym] = next_num
            next_num += 1

        return symbol_map

    def encode_single_transition(self, transition: Tuple[str, str, str, str, str]) -> str:
        src, read_sym, dst, write_sym, direction = transition

        read_sym = self.normalize_blank(read_sym)
        write_sym = self.normalize_blank(write_sym)

        i = self.state_map[src]
        j = self.symbol_map[read_sym]
        k = self.state_map[dst]
        l = self.symbol_map[write_sym]
        m = self.direction_map[direction]

        return f'{"0"*i}1{"0"*j}1{"0"*k}1{"0"*l}1{"0"*m}'

    def encode_all(self) -> str:
        encoded = [self.encode_single_transition(t) for t in self.parsed_transitions]
        return "11".join(encoded)

    def numbering_tables(self) -> Dict[str, Dict[str, int]]:
        return {
            "states": self.state_map,
            "symbols": self.symbol_map,
            "directions": self.direction_map,
        }

    def pretty_print(self):
        print("Nummerierung der Zustände:")
        for state, num in sorted(self.state_map.items(), key=lambda x: x[1]):
            print(f"  {state} = q{num}")

        print("\nNummerierung der Symbole:")
        for sym, num in sorted(self.symbol_map.items(), key=lambda x: x[1]):
            shown = "Blank" if sym == "_" else sym
            print(f"  {shown} = X{num}")

        print("\nNummerierung der Richtungen:")
        for d, num in sorted(self.direction_map.items(), key=lambda x: x[1]):
            print(f"  {d} = D{num}")

        print("\nKodierung der einzelnen Transitionen:")
        for raw, parsed in zip(self.transitions_raw, self.parsed_transitions):
            print(f"  {raw}")
            print(f"    -> {self.encode_single_transition(parsed)}")

        print("\nGesamtkodierung:")
        print(self.encode_all())


if __name__ == "__main__":
    transitions = [
        "q0,0->q0,0,R",
        "q0,1->q0,0,R",
        "q0,_->q1,E,L",
        "q1,0->q1,0,L",
        "q1,_->q2,E,R",
        "q2,0->q3,Z,R",
        "q3,0->q3,0,R",
        "q3,E->q4,E,R",
        "q3,Y->q3,Y,R",
        "q4,0->q4,0,R",
        "q4,_->q5,0,L",
        "q5,0->q5,0,L",
        "q5,E->q8,E,L",
        "q8,0->q6,0,L",
        "q6,0->q6,0,L",
        "q6,X->q7,X,R",
        "q6,Z->q7,Z,R",
        "q7,0->q3,X,R",
        "q6,Y->q11,Y,L",
        "q11,0->q6,0,L",
        "q11,X->q12,X,R",
        "q11,E->q12,E,R",
        "q12,Y->q3,Z,R",
        "q8,Y->q14,Y,L",
        "q14,0->q6,0,L",
        "q14,X->q15,X,R",
        "q15,Y->q3,Z,R",
        "q8,X->q9,0,L",
        "q9,X->q9,0,L",
        "q9,Y->q10,X,R",
        "q9,Z->q10,X,R",
        "q10,0->q9,Y,L",
        "q9,E->q13,E,R",
        "q13,0->q3,X,R",
        "q8,Z->q16,Z,R",
        "q16,E->q16,_,L",
        "q16,Z->q16,_,L",
        "q16,X->q16,_,L",
        "q16,_->q17,_,R",
    ]

    encoder = TMGodelEncoder(
        transitions=transitions,
        start_state="q0",
        accept_state="q17"
    )

    encoder.pretty_print()

    # Generate bin file by using binary output in
    # echo "10...." | fold -w8 | while read -r byte; do
    #   printf "%02x" "$((2#$byte))"
    # done | xxd -r -p > datei.bi