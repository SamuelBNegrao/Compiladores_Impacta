import io
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import mock_open, patch

from semantic import Lexer, ParserPos
from ir import GeradorIR
from minic import main


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


if __name__ == "__main__":
    unittest.main()
