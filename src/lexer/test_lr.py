import unittest

from atributos_lr import ErroAtributo, analisar
from lexer_v2 import Lexer
from parser_lr import ParserLR


class ParserLRTests(unittest.TestCase):
    def test_shift_reduce_parser_builds_ast_with_precedence(self):
        source = "int main() { return 1 + 2 * 3; }"
        tree = ParserLR(Lexer(source).analisar()).parse_program()
        self.assertEqual(
            tree.sexpr(),
            "Program(Function(int main() Block(Return(Binary(+,Lit(int,1),"
            "Binary(*,Lit(int,2),Lit(int,3)))))))",
        )

    def test_if_else_binds_to_nearest_if(self):
        source = "int main() { if (true) if (false) return 1; else return 2; }"
        tree = ParserLR(Lexer(source).analisar()).parse_program()
        outer = tree.declaracoes[0].corpo.comandos[0]
        self.assertIsNone(outer.senao)
        self.assertIsNotNone(outer.entao.senao)


class AttributeGrammarTests(unittest.TestCase):
    def test_environment_is_inherited_and_type_is_synthesized(self):
        result = analisar("x + 2 * y", {"x": "int", "y": "float"})
        self.assertEqual(result.tipo, "float")
        self.assertEqual(
            result.arvore.sexpr(),
            "Binary(+,Id(x),Binary(*,Lit(int,2),Id(y)))",
        )

    def test_reduction_trace_reports_synthesized_type(self):
        trace = []
        analisar("x + 1", {"x": "int"}, trace=trace.append)
        self.assertTrue(any("tipo=int" in event for event in trace))

    def test_undeclared_identifier_is_reported(self):
        with self.assertRaisesRegex(ErroAtributo, "não declarado"):
            analisar("missing + 1", {})

    def test_modulo_requires_integer_operands(self):
        with self.assertRaisesRegex(ErroAtributo, "exige operandos int"):
            analisar("1.0 % 2", {})


if __name__ == "__main__":
    unittest.main()
