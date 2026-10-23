import io
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import mock_open, patch

from semantic import Lexer, ParserPos
from ir import GeradorIR
from minic import main
from optimizer import optimize


def generate(source):
    tokens = Lexer(source).analisar()
    program = ParserPos(tokens).parse_program()
    return GeradorIR().gerar(program)


class IRGenerationTests(unittest.TestCase):
    def test_arithmetic_expression_uses_temporaries_and_precedence(self):
        output = generate(
            "int main() { int a = 18; int b = 5; int c = 2; "
            "int r; r = a - b * c; print(r); return 0; }"
        )
        self.assertIn("t1 = b * c\n  t2 = a - t1\n  r = t2", output)

    def test_if_else_and_while_generate_control_flow(self):
        output = generate(
            "int main() { int i = 0; while (i < 4) { "
            "if (i == 2) { print(i); } i = i + 1; } return 0; }"
        )
        self.assertIn("L_test:\n  t1 = i < 4", output)
        self.assertIn("ifFalse", output)
        self.assertIn("goto L_test", output)
        self.assertIn("L_end_if:", output)

    def test_for_continue_targets_increment_block(self):
        output = generate(
            "int main() { int i = 0; "
            "for (; i < 3; i = i + 1) { "
            "if (i == 1) continue; print(i); } return 0; }"
        )
        self.assertIn("ifFalse t1 goto L_end", output)
        self.assertIn("goto L_increment", output)
        self.assertIn("L_increment:\n  t3 = i + 1", output)

    def test_array_access_uses_load_and_store(self):
        output = generate(
            "int main() { int v[3]; int i = 1; int x; "
            "v[i] = 12; x = v[i]; print(x); return 0; }"
        )
        self.assertIn("store v[t1], 12", output)
        self.assertIn("t3 = load v[t2]", output)

    def test_shadowed_variables_get_distinct_ir_names(self):
        output = generate(
            "int main() { int x = 5; { int x = 8; print(x); } "
            "print(x); return 0; }"
        )
        self.assertIn("x#outer = 5", output)
        self.assertIn("x#inner = 8", output)
        self.assertIn("print x#inner\n  print x#outer", output)


class IROptimizationTests(unittest.TestCase):
    def test_constant_folding_and_propagation(self):
        output = optimize(
            "func main\n"
            "  t1 = 2 + 3\n"
            "  a = t1\n"
            "  t2 = a * 4\n"
            "  b = t2\n"
            "  print b\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertIn("  print 20\n", output)
        self.assertNotIn("  a =", output)
        self.assertNotIn("  b =", output)

    def test_constant_branch_removes_unreachable_alternative(self):
        output = optimize(
            "func main\n"
            "  t1 = 3 < 5\n"
            "  ifFalse t1 goto L_else\n"
            "  r = 11\n"
            "  goto L_end\n"
            "L_else:\n"
            "  r = 22\n"
            "L_end:\n"
            "  print r\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertIn("  print 11\n", output)
        self.assertNotIn("  r = 22\n", output)

    def test_logical_constants_simplify_branches(self):
        output = optimize(
            "func main\n"
            "  t1 = true and false\n"
            "  ifFalse t1 goto L_else\n"
            "  print 1\n"
            "  goto L_end\n"
            "L_else:\n"
            "  print 2\n"
            "L_end:\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertIn("  print 2\n", output)
        self.assertNotIn("  print 1\n", output)

    def test_loop_invariant_expression_moves_before_loop(self):
        output = optimize(
            "func main\n"
            "  i = 0\n"
            "  x = 6\n"
            "  y = 7\n"
            "L_test:\n"
            "  t1 = i < 4\n"
            "  ifFalse t1 goto L_end\n"
            "  t2 = x * y\n"
            "  t3 = i + t2\n"
            "  i = t3\n"
            "  goto L_test\n"
            "L_end:\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertLess(output.index("  t2 = 42"), output.index("L_test:"))

    def test_integer_division_uses_truncation_toward_zero(self):
        output = optimize(
            "func main\n"
            "  t1 = -7 / 3\n"
            "  print t1\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertIn("  print -2\n", output)

    def test_common_subexpression_is_reused_until_an_operand_changes(self):
        output = optimize(
            "func main\n"
            "  read a\n"
            "  read b\n"
            "  t1 = a * b\n"
            "  t2 = t1 + 1\n"
            "  x = t2\n"
            "  t3 = a * b\n"
            "  t4 = t3 + 2\n"
            "  y = t4\n"
            "  print x\n"
            "  print y\n"
            "  return 0\n"
            "endfunc\n"
        )
        self.assertEqual(output.count(" = a * b\n"), 1)
        self.assertIn("  t4 = t1 + 2\n", output)


class CompilerCLITests(unittest.TestCase):
    def run_cli(self, source, *options):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with patch("builtins.open", mock_open(read_data=source)):
            with redirect_stdout(stdout), redirect_stderr(stderr):
                status = main(["fixture.c", *options])
        return status, stdout.getvalue(), stderr.getvalue()

    def test_default_mode_emits_ir(self):
        status, output, _errors = self.run_cli(
            "int main() { return 0; }"
        )
        self.assertEqual(status, 0)
        self.assertIn("func main\n", output)

    def test_check_mode_validates_without_emitting_ir(self):
        status, output, _errors = self.run_cli(
            "int main() { return 0; }", "--check"
        )
        self.assertEqual(status, 0)
        self.assertNotIn("func main", output)

    def test_lexical_error_uses_exit_code_2(self):
        status, _output, errors = self.run_cli(
            "int main() { @ return 0; }"
        )
        self.assertEqual(status, 2)
        self.assertIn("Erro léxico", errors)

    def test_syntax_error_uses_exit_code_3(self):
        status, _output, errors = self.run_cli(
            "int main() { return 0 }"
        )
        self.assertEqual(status, 3)
        self.assertIn("Erro sintático", errors)

    def test_semantic_error_uses_exit_code_4(self):
        status, _output, errors = self.run_cli(
            "int main() { break; return 0; }"
        )
        self.assertEqual(status, 4)
        self.assertIn("Erro semântico", errors)

    def test_o1_emits_optimized_ir(self):
        status, output, errors = self.run_cli(
            "int main() { int a = 2 + 3; int b = a * 4; "
            "print(b); return 0; }",
            "-O1",
        )
        self.assertEqual(status, 0, errors)
        self.assertIn("  print 20\n", output)

    def test_ir_option_can_be_combined_with_o1(self):
        status, output, errors = self.run_cli(
            "int main() { int a = 2 + 3; print(a); return 0; }", "--ir", "-O1"
        )
        self.assertEqual(status, 0, errors)
        self.assertIn("  print 5\n", output)

    def test_check_mode_rejects_ir_optimization_options(self):
        for option in ("-O0", "-O1", "--ir"):
            with self.subTest(option=option):
                status, _output, errors = self.run_cli(
                    "int main() { return 0; }", "--check", option
                )
                self.assertEqual(status, 1)
                self.assertIn("--check", errors)

    def test_ast_mode_rejects_optimization_options(self):
        status, _output, errors = self.run_cli(
            "int main() { return 0; }", "--ast", "-O1"
        )
        self.assertEqual(status, 1)
        self.assertIn("--ast", errors)


if __name__ == "__main__":
    unittest.main()
