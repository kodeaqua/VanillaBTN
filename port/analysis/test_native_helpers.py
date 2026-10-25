"""Keep capture-free native helper copies token-equivalent to matching code."""

from pathlib import Path
import unittest

from source_audit import tokens


def function_body(source: str, name: str) -> list[str]:
    words = [token.value for token in tokens(source)]
    matches = []
    for index, word in enumerate(words):
        if word != name or words[index + 1:index + 2] != ["("]:
            continue
        cursor, depth = index + 1, 0
        while cursor < len(words):
            depth += (words[cursor] == "(") - (words[cursor] == ")")
            cursor += 1
            if depth == 0:
                break
        if words[cursor:cursor + 1] != ["{"]:
            continue
        start, depth = index - 1, 0  # Return type, excluding native `static`.
        while cursor < len(words):
            depth += (words[cursor] == "{") - (words[cursor] == "}")
            cursor += 1
            if depth == 0:
                break
        matches.append(words[start:cursor])
    if len(matches) != 1:
        raise AssertionError(f"Expected one definition of {name}, found {len(matches)}")
    return matches[0]


class NativeHelperTests(unittest.TestCase):
    def test_matching_helper_tokens_are_preserved(self):
        root = Path(__file__).resolve().parents[2]
        cases = [
            ("main/geometry/ground.c", "ground", ["ground_triangle_under", "ground_quad_under"]),
            ("overlay/11_GF_FARM/animals/animals.c", "animals",
             ["farm_kind_matches_flags", "farm_move_compare", "farm_heading_compare"]),
            ("overlay/11_GF_FARM/estate.c", "estate", ["estate_kind_value_for_flags"]),
            ("overlay/11_GF_FARM/menu/shop_dialogues.c", "shop", ["has_medal_price"]),
            ("overlay/11_GF_FARM/event/event_rules.c", "event_rules", ["winter_23_single_check"]),
        ]
        for original, native, names in cases:
            matching = (root / "src" / original).read_text(encoding="utf-8")
            adapter = (root / "port/compat/nested" / (native + ".inc")).read_text(encoding="utf-8")
            for name in names:
                with self.subTest(helper=name):
                    self.assertEqual(function_body(adapter, name), function_body(matching, name))


if __name__ == "__main__":
    unittest.main()
