"""Conservative optimization passes for the MiniC three-address IR."""

from __future__ import annotations

import re

_ASSIGNMENT = re.compile(r"^([A-Za-z_]\w*(?:#\w+)?) = (.+)$")
_BINARY = re.compile(r"^(\S+) (and|or|==|!=|<=|>=|&&|\|\||[+\-*/%<>]) (\S+)$")
_UNARY = re.compile(r"^(not|neg) (\S+)$")
_ATOM = re.compile(r"^(?:-?\d+(?:\.\d+)?|true|false|'.*'|\".*\"|[A-Za-z_]\w*(?:#\w+)?(?:\[[^\]]+\])?)$")
_IDENTIFIER = re.compile(r"[A-Za-z_]\w*(?:#\w+)?")
_LABEL = re.compile(r"^([A-Za-z_]\w*):$")
_BRANCH = re.compile(r"^ifFalse (\S+) goto (\S+)$")
_GOTO = re.compile(r"^goto (\S+)$")
_PURE_OPERATORS = {"+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "and", "or"}


def optimize(ir: str) -> str:
    """Optimize each function while preserving the surrounding IR format."""
    output: list[str] = []
    current: list[str] | None = None
    for raw_line in ir.splitlines():
        line = raw_line.strip()
        if line.startswith("func "):
            if current is not None:
                output.extend(_optimize_function(current))
            current = [line]
        elif current is not None:
            current.append(line)
            if line == "endfunc":
                output.extend(_optimize_function(current))
                current = None
        else:
            output.append(line)
    if current is not None:
        output.extend(_optimize_function(current))
    return "\n".join(output) + ("\n" if output else "")


def _optimize_function(function: list[str]) -> list[str]:
    if len(function) < 2:
        return function
    body = function[1:-1] if function[-1] == "endfunc" else function[1:]
    body = _move_loop_invariants(body)
    for _ in range(8):
        previous = body
        body = _remove_unreachable(body)
        body = _remove_redundant_jumps(body)
        body = _propagate_and_fold(body)
        body = _remove_unreachable(body)
        body = _remove_redundant_jumps(body)
        body = _eliminate_dead_assignments(body)
        if body == previous:
            break
    result = [function[0]]
    result.extend(
        line if _LABEL.fullmatch(line) else f"  {line}"
        for line in body
    )
    if function[-1] == "endfunc":
        result.append("endfunc")
    return result


def _literal(value: str) -> bool:
    if value in ("true", "false"):
        return True
    return bool(_ATOM.fullmatch(value)) and not _IDENTIFIER.fullmatch(value)


def _evaluate(left: str, operator: str, right: str) -> str | None:
    try:
        if operator in ("and", "or"):
            left_value = left == "true"
            right_value = right == "true"
            result = left_value and right_value if operator == "and" else left_value or right_value
            return "true" if result else "false"
        floating = "." in left or "." in right
        a = float(left) if floating else int(left)
        b = float(right) if floating else int(right)
        if operator == "/" and not floating:
            if b == 0:
                return None
            value = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                value = -value
        elif operator == "%" and not floating:
            if b == 0:
                return None
            quotient = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                quotient = -quotient
            value = a - quotient * b
        else:
            operations = {
                "+": lambda: a + b,
                "-": lambda: a - b,
                "*": lambda: a * b,
                "/": lambda: a / b,
                "%": lambda: a % b,
                "==": lambda: a == b,
                "!=": lambda: a != b,
                "<": lambda: a < b,
                ">": lambda: a > b,
                "<=": lambda: a <= b,
                ">=": lambda: a >= b,
            }
            value = operations[operator]()
    except (KeyError, ValueError, ZeroDivisionError, OverflowError):
        return None
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float) and value.is_integer() and "." not in left and "." not in right:
        return str(int(value))
    return str(value)


def _simplify_binary(left: str, operator: str, right: str) -> str | None:
    if _literal(left) and _literal(right):
        folded = _evaluate(left, operator, right)
        if folded is not None:
            return folded
    if operator == "+":
        if right == "0":
            return left
        if left == "0":
            return right
    elif operator == "-":
        if right == "0":
            return left
    elif operator == "*":
        if right == "1":
            return left
        if left == "1":
            return right
        if right == "0" or left == "0":
            return "0"
    elif operator == "/":
        if right == "1":
            return left
    return None


def _substitute(atom: str, values: dict[str, str]) -> str:
    seen: set[str] = set()
    current = atom
    while current in values and current not in seen:
        seen.add(current)
        current = values[current]
    return current


def _uses(expression: str) -> set[str]:
    return {
        name
        for name in _IDENTIFIER.findall(expression)
        if not _literal(name) and not name.startswith("L_")
    }


def _propagate_and_fold(body: list[str]) -> list[str]:
    output: list[str] = []
    values: dict[str, str] = {}
    common: dict[tuple[str, str, str], str] = {}
    dependencies: dict[tuple[str, str, str], set[str]] = {}

    def clear() -> None:
        values.clear()
        common.clear()
        dependencies.clear()

    for line in body:
        if _LABEL.fullmatch(line):
            clear()
            output.append(line)
            continue
        if line.startswith(("goto ", "ifFalse ")):
            match = _BRANCH.fullmatch(line)
            if match:
                condition = _substitute(match.group(1), values)
                if condition in ("true", "false"):
                    if condition == "false":
                        output.append(f"goto {match.group(2)}")
                    clear()
                    continue
                line = f"ifFalse {condition} goto {match.group(2)}"
            output.append(line)
            clear()
            continue
        assignment = _ASSIGNMENT.fullmatch(line)
        if assignment:
            target, expression = assignment.groups()
            if expression.startswith("call "):
                clear()
            binary = _BINARY.fullmatch(expression)
            unary = _UNARY.fullmatch(expression)
            signature: tuple[str, str, str] | None = None
            if binary:
                left, operator, right = binary.groups()
                left = _substitute(left, values)
                right = _substitute(right, values)
                simplified = _simplify_binary(left, operator, right)
                if simplified is not None:
                    expression = simplified
                else:
                    expression = f"{left} {operator} {right}"
                    if operator in _PURE_OPERATORS:
                        signature = (left, operator, right)
            elif unary:
                operator, operand = unary.groups()
                operand = _substitute(operand, values)
                expression = f"{operator} {operand}"
                if _literal(operand):
                    try:
                        if operator == "neg":
                            expression = str(-float(operand) if "." in operand else -int(operand))
                        elif operand in ("true", "false"):
                            expression = "false" if operand == "true" else "true"
                    except ValueError:
                        pass
            elif _ATOM.fullmatch(expression):
                expression = _substitute(expression, values)

            if signature is not None and signature in common:
                expression = common[signature]
            elif signature is not None:
                common[signature] = target
                dependencies[signature] = _uses(f"{signature[0]} {signature[1]} {signature[2]}")

            for key, names in list(dependencies.items()):
                if target in names:
                    common.pop(key, None)
                    dependencies.pop(key, None)
            values.pop(target, None)
            if expression != target:
                if _literal(expression):
                    values[target] = expression
                elif _IDENTIFIER.fullmatch(expression):
                    values[target] = _substitute(expression, values)
                else:
                    values.pop(target, None)
            output.append(f"{target} = {expression}")
            continue
        if line.startswith("print "):
            output.append(f"print {_substitute(line[6:], values)}")
            continue
        if line.startswith("return "):
            output.append(f"return {_substitute(line[7:], values)}")
            clear()
            continue
        if line.startswith(("param ", "read ", "store ")):
            clear()
        output.append(line)
    return output


def _label_references(body: list[str]) -> set[str]:
    refs: set[str] = set()
    for line in body:
        branch = _BRANCH.fullmatch(line)
        jump = _GOTO.fullmatch(line)
        if branch:
            refs.add(branch.group(2))
        if jump:
            refs.add(jump.group(1))
    return refs


def _label_reference_counts(body: list[str]) -> dict[str, int]:
    refs: dict[str, int] = {}
    for line in body:
        branch = _BRANCH.fullmatch(line)
        jump = _GOTO.fullmatch(line)
        target = branch.group(2) if branch else jump.group(1) if jump else None
        if target is not None:
            refs[target] = refs.get(target, 0) + 1
    return refs


def _remove_unreachable(body: list[str]) -> list[str]:
    referenced = _label_references(body)
    output: list[str] = []
    unreachable = False
    for line in body:
        label = _LABEL.fullmatch(line)
        if label:
            if unreachable and label.group(1) not in referenced:
                continue
            unreachable = False
            output.append(line)
            continue
        if unreachable:
            continue
        output.append(line)
        if line.startswith("return") or _GOTO.fullmatch(line):
            unreachable = True
    return output


def _remove_redundant_jumps(body: list[str]) -> list[str]:
    output: list[str] = []
    index = 0
    references = _label_reference_counts(body)
    while index < len(body):
        jump = _GOTO.fullmatch(body[index])
        if jump and index + 1 < len(body):
            label = _LABEL.fullmatch(body[index + 1])
            if label and label.group(1) == jump.group(1):
                if references.get(jump.group(1)) == 1:
                    index += 2
                    continue
        output.append(body[index])
        index += 1
    return output


def _eliminate_dead_assignments(body: list[str]) -> list[str]:
    output = body[:]
    changed = True
    while changed:
        changed = False
        used: set[str] = set()
        for line in output:
            assignment = _ASSIGNMENT.fullmatch(line)
            if assignment:
                used.update(_uses(assignment.group(2)))
            elif line.startswith("ifFalse "):
                branch = _BRANCH.fullmatch(line)
                if branch:
                    used.update(_uses(branch.group(1)))
            elif line.startswith(("print ", "return ", "param ", "read ", "store ")):
                used.update(_uses(line.split(" ", 1)[1]))
        reduced: list[str] = []
        for line in output:
            assignment = _ASSIGNMENT.fullmatch(line)
            if assignment:
                target, expression = assignment.groups()
                if target not in used and _ATOM.fullmatch(expression) or (
                    target not in used and _BINARY.fullmatch(expression)
                    and _BINARY.fullmatch(expression).group(2) in _PURE_OPERATORS
                ) or (target not in used and _UNARY.fullmatch(expression)):
                    changed = True
                    continue
            reduced.append(line)
        output = reduced
    return output


def _move_loop_invariants(body: list[str]) -> list[str]:
    output = body[:]
    index = 0
    while index < len(output):
        label = _LABEL.fullmatch(output[index])
        if not label:
            index += 1
            continue
        header = label.group(1)
        back_edge = next(
            (
                cursor
                for cursor in range(index + 1, len(output))
                if _GOTO.fullmatch(output[cursor])
                and _GOTO.fullmatch(output[cursor]).group(1) == header
            ),
            None,
        )
        if back_edge is None:
            index += 1
            continue
        loop = output[index + 1 : back_edge]
        writes: set[str] = set()
        safe = True
        for line in loop:
            assignment = _ASSIGNMENT.fullmatch(line)
            if assignment:
                writes.add(assignment.group(1))
            elif line.startswith(("read ", "store ", "param ", "ifFalse ")):
                if line.startswith(("read ", "store ", "param ")):
                    safe = False
        if safe:
            moves: list[str] = []
            for line in loop:
                assignment = _ASSIGNMENT.fullmatch(line)
                binary = _BINARY.fullmatch(assignment.group(2)) if assignment else None
                if not assignment or not binary:
                    continue
                target, expression = assignment.groups()
                _left, _operator, _right = binary.groups()
                operands = _uses(expression)
                if (
                    re.fullmatch(r"t\d+", target)
                    and _operator not in ("/", "%")
                    and not (operands & writes)
                ):
                    moves.append(line)
            if moves:
                for move in moves:
                    output.remove(move)
                output[index:index] = moves
                index += len(moves)
                back_edge -= len(moves)
        index = back_edge + 1
    return output
