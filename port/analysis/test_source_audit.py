"""Regression fixtures for the lexical audit's scope and token handling."""

from pathlib import Path
import tempfile
import unittest

from source_audit import audit, functions, tokens


class SourceAuditTests(unittest.TestCase):
    def test_comments_literals_and_initializer_are_not_functions(self):
        source = '''
/* int fake(void) { } */
char *label = "int fake2(void) { }";
void (*table[])(void) = {0};
static int helper(int value) { if (value) { return 1; } return 0; }
int public_function(void) { return helper(0); }
'''
        found = functions(source)
        self.assertEqual([(item["name"], item["static"]) for item in found],
                         [("helper", True), ("public_function", False)])

    def test_prototype_and_macro_are_not_definitions(self):
        source = '#define MACRO(x) { x; }\nint declared(int);\nvoid actual(void) {}'
        self.assertEqual([item["name"] for item in functions(source)], ["actual"])

    def test_token_lines_survive_multiline_comments(self):
        found = tokens('/* one\n two */\nname("not_a_call()");')
        self.assertEqual(found[0].value, "name")
        self.assertEqual(found[0].line, 3)
        self.assertNotIn("not_a_call", [token.value for token in found])

    def test_continued_macro_body_is_skipped(self):
        source = '#define BODY(x) \\\n    helper(x) \\\n    { return 0; }\nint actual(void) { return 0; }'
        self.assertEqual([item["name"] for item in functions(source)], ["actual"])

    def test_operators_and_numbers_are_preserved(self):
        found = [token.value for token in tokens('value + (0x9000);')]
        self.assertEqual(found, ["value", "+", "(", "0x9000", ")", ";"])

    def test_collision_candidates_exclude_static_functions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for region in ("one", "two"):
                folder = root / "src" / "overlay" / region
                folder.mkdir(parents=True)
                (folder / "unit.c").write_text(
                    'static int local(void) { return 0; }\n'
                    'int shared(void) { return ExternalApi(); }\n', encoding="utf-8")
                (folder / "CMakeLists.txt").write_text(
                    f'psx_overlay({region} FILE {region}.bin REGION low)', encoding="utf-8")
            result = audit(root)
            self.assertEqual(set(result["function_collision_candidates"]), {"shared"})
            self.assertEqual(set(result["external_call_candidates"]), {"ExternalApi"})
            self.assertEqual({item["name"] for item in result["overlays"]}, {"one", "two"})
            self.assertTrue(all(not item["file"].startswith(directory)
                                for item in result["functions"]))


if __name__ == "__main__":
    unittest.main()
