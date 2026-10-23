"""Small SLR(1) table builder and shift/reduce parser."""

from collections import defaultdict, deque
from dataclasses import dataclass


EOF = "$"


@dataclass(frozen=True)
class Production:
    lhs: str
    rhs: tuple[str, ...]
    action: str


class LRParseError(Exception):
    pass


class SLRParser:
    def __init__(self, start: str, productions: list[Production], reducers: dict):
        self.start = start
        self.productions = [Production("__START__", (start,), "accept")] + productions
        self.reducers = reducers
        self.nonterminals = {production.lhs for production in self.productions}
        self.terminals = {
            symbol
            for production in self.productions
            for symbol in production.rhs
            if symbol not in self.nonterminals
        }
        self.first, self.nullable = self._first_sets()
        self.follow = self._follow_sets()
        self.states, self.transitions = self._lr0_states()
        self.actions, self.gotos = self._parse_tables()

    def _first_sets(self):
        first = {symbol: {symbol} for symbol in self.terminals}
        first.update({symbol: set() for symbol in self.nonterminals})
        nullable = set()

        changed = True
        while changed:
            changed = False
            for production in self.productions:
                all_nullable = True
                for symbol in production.rhs:
                    before = len(first[production.lhs])
                    first[production.lhs].update(first[symbol])
                    changed |= len(first[production.lhs]) != before
                    if symbol not in nullable:
                        all_nullable = False
                        break
                if all_nullable and production.lhs not in nullable:
                    nullable.add(production.lhs)
                    changed = True
        return first, nullable

    def _first_of(self, symbols):
        result = set()
        for symbol in symbols:
            result.update(self.first[symbol])
            if symbol not in self.nullable:
                break
        return result

    def _follow_sets(self):
        follow = {symbol: set() for symbol in self.nonterminals}
        follow["__START__"].add(EOF)
        changed = True
        while changed:
            changed = False
            for production in self.productions:
                for index, symbol in enumerate(production.rhs):
                    if symbol not in self.nonterminals:
                        continue
                    before = len(follow[symbol])
                    suffix = production.rhs[index + 1 :]
                    follow[symbol].update(self._first_of(suffix))
                    if all(item in self.nullable for item in suffix):
                        follow[symbol].update(follow[production.lhs])
                    changed |= len(follow[symbol]) != before
        return follow

    def _closure(self, items):
        result = set(items)
        pending = list(items)
        by_lhs = defaultdict(list)
        for index, production in enumerate(self.productions):
            by_lhs[production.lhs].append(index)

        while pending:
            production_index, dot = pending.pop()
            production = self.productions[production_index]
            if dot == len(production.rhs):
                continue
            symbol = production.rhs[dot]
            if symbol not in self.nonterminals:
                continue
            for child in by_lhs[symbol]:
                item = (child, 0)
                if item not in result:
                    result.add(item)
                    pending.append(item)
        return frozenset(result)

    def _lr0_states(self):
        initial = self._closure({(0, 0)})
        states = [initial]
        state_ids = {initial: 0}
        transitions = {}
        pending = deque([0])

        while pending:
            state_id = pending.popleft()
            state = states[state_id]
            symbols = {
                self.productions[index].rhs[dot]
                for index, dot in state
                if dot < len(self.productions[index].rhs)
            }
            for symbol in symbols:
                moved = {
                    (index, dot + 1)
                    for index, dot in state
                    if dot < len(self.productions[index].rhs)
                    and self.productions[index].rhs[dot] == symbol
                }
                target = self._closure(moved)
                if target not in state_ids:
                    state_ids[target] = len(states)
                    states.append(target)
                    pending.append(state_ids[target])
                transitions[(state_id, symbol)] = state_ids[target]
        return states, transitions

    def _parse_tables(self):
        actions = {}
        gotos = {}
        for state_id, state in enumerate(self.states):
            for production_index, dot in state:
                production = self.productions[production_index]
                if dot < len(production.rhs):
                    symbol = production.rhs[dot]
                    target = self.transitions[(state_id, symbol)]
                    if symbol in self.terminals:
                        self._set_action(actions, state_id, symbol, ("shift", target))
                    else:
                        gotos[(state_id, symbol)] = target
                    continue

                if production_index == 0:
                    self._set_action(actions, state_id, EOF, ("accept",))
                else:
                    for terminal in self.follow[production.lhs]:
                        self._set_action(
                            actions, state_id, terminal, ("reduce", production_index)
                        )
        return actions, gotos

    def _set_action(self, actions, state, terminal, action):
        key = (state, terminal)
        previous = actions.get(key)
        if previous is None or previous == action:
            actions[key] = action
            return
        # ELSE binds to the closest unmatched IF.
        if terminal == "ELSE" and {previous[0], action[0]} == {"shift", "reduce"}:
            actions[key] = previous if previous[0] == "shift" else action
            return
        raise ValueError(
            f"Conflito SLR(1) no estado {state}, terminal {terminal}: "
            f"{previous} versus {action}"
        )

    def parse(self, tokens, trace=None):
        state_stack = [0]
        value_stack = []
        position = 0

        while True:
            token = tokens[position]
            terminal = EOF if token.tipo == "EOF" else token.tipo
            state = state_stack[-1]
            action = self.actions.get((state, terminal))
            if action is None:
                expected = sorted(
                    symbol
                    for (candidate, symbol), value in self.actions.items()
                    if candidate == state and value[0] != "reduce"
                )
                lexeme = token.lexema or "<fim de arquivo>"
                raise LRParseError(
                    f"esperado um de {', '.join(expected)}, encontrado "
                    f"'{lexeme}' ({token.tipo}) na linha {token.linha}, "
                    f"coluna {token.coluna}"
                )

            if action[0] == "shift":
                state_stack.append(action[1])
                value_stack.append(token)
                if trace is not None:
                    trace(f"shift {terminal} -> estado {action[1]}")
                position += 1
                continue

            if action[0] == "reduce":
                production = self.productions[action[1]]
                count = len(production.rhs)
                values = value_stack[-count:] if count else []
                if count:
                    del state_stack[-count:]
                    del value_stack[-count:]
                result = self.reducers[production.action](values)
                target = self.gotos[(state_stack[-1], production.lhs)]
                state_stack.append(target)
                value_stack.append(result)
                if trace is not None:
                    rhs = " ".join(production.rhs) or "ε"
                    trace(
                        f"reduce {production.lhs} -> {rhs}; "
                        f"goto estado {target}"
                        + (f"; tipo={result.tipo}" if hasattr(result, "tipo") else "")
                    )
                continue

            if trace is not None:
                trace("accept")
            return value_stack[-1]
