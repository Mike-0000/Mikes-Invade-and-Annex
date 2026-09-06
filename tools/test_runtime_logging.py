"""Regression tests for the static logging-policy checker (not an engine test)."""
import unittest

from check_runtime_logging import debug_blocks, mask_source, print_calls, violations


class RuntimeLoggingPolicyTests(unittest.TestCase):
    def test_unguarded_routine_levels_and_bare_prints_fail(self):
        for statement in (
            'Print("tick", LogLevel.NORMAL);',
            'Print("tick", LogLevel.DEBUG);',
            'Print("tick");',
            'PrintFormat("tick %1", count);',
        ):
            with self.subTest(statement=statement):
                self.assertEqual(len(violations(statement)), 1)

    def test_production_severities_remain_available(self):
        for level in ("WARNING", "ERROR", "FATAL"):
            statement = f'Print("failed", LogLevel.{level});'
            self.assertEqual(violations(statement), [])
            self.assertEqual(len(violations(
                'if (IA_Log.IsDebugEnabled()) { ' + statement + ' }'
            )), 1)

    def test_nested_debug_blocks_and_conditional_bodies(self):
        source = '''
            if (IA_Log.IsDebugEnabled())
            {
                foreach (int entry : entries)
                {
                    if (entry > 0)
                        Print(string.Format("entry (%1)", entry), LogLevel.NORMAL);
                    else
                        Print("empty", LogLevel.NORMAL);
                }
            }
        '''
        self.assertEqual(violations(source), [])
        self.assertEqual(len(list(print_calls(source))), 2)
        self.assertEqual(len(debug_blocks(source)), 1)

    def test_scope_does_not_leak_past_debug_block(self):
        source = '''
            if (IA_Log.IsDebugEnabled()) { Print("debug", LogLevel.NORMAL); }
            Print("leaked", LogLevel.NORMAL);
        '''
        self.assertEqual(len(violations(source)), 1)
        self.assertTrue(source[violations(source)[0]:].startswith('Print("leaked"'))

    def test_comments_and_literals_do_not_create_fake_calls_or_guards(self):
        source = r'''
            // Print("comment", LogLevel.NORMAL);
            /* if (IA_Log.IsDebugEnabled()) { Print("comment"); } */
            string text = "Print(\"literal\"); if (IA_Log.IsDebugEnabled()) { }";
            Print("real braces { } and escaped \"quote\"", LogLevel.WARNING);
        '''
        self.assertEqual(violations(source), [])
        self.assertEqual(len(list(print_calls(source))), 1)
        self.assertEqual(mask_source(source).count('\n'), source.count('\n'))

    def test_multiline_format_and_level(self):
        source = '''
            if (IA_Log.IsDebugEnabled())
            {
                Print(string.Format("counts %1/%2", Count(), OtherCount()),
                    /* retain normal visibility when opted in */ LogLevel.NORMAL);
            }
        '''
        self.assertEqual(violations(source), [])

    def test_else_if_gate_is_recognized(self):
        source = '''
            if (!fired)
                Print("failed", LogLevel.WARNING);
            else if (IA_Log.IsDebugEnabled())
            {
                Print("fired", LogLevel.NORMAL);
            }
        '''
        self.assertEqual(violations(source), [])

    def test_unclosed_source_fails_instead_of_silently_passing(self):
        with self.assertRaises(ValueError):
            violations('if (IA_Log.IsDebugEnabled()) { Print("debug");')


if __name__ == '__main__':
    unittest.main()
