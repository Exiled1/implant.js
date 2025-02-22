import os
import shutil
import tempfile
import textwrap
import unittest

from .. import server

MODULES = {
    "simple.js": """
        hello world
        this is obviously valid javascript code
        but it doesnt matter
    """,

    "loadstuff.js": """
        // this is a random comment

        import { a, b, c } from "lib/mylib.js";
        
        ctx.output("hello");

        if (a()) {
            b();
        }
        c();

        ctx.output("noice");
    """,

    "lib/mylib.js": """
        function a() {
            ctx.output("abc");
        }
        
        function b() {
            ctx.output("def");
        }
        
        function c() {
            ctx.output("zyx");
        }
    """,

    "invalidload.js": """
        import * from "lib/asdf.js";
    """,
}

LOADSTUFF_OUTPUT = """
// this is a random comment

function a() {
ctx.output("abc");
}

function b() {
ctx.output("def");
}

function c() {
ctx.output("zyx");
}

ctx.output("hello");

if (a()) {
b();
}
c();

ctx.output("noice");
""".strip()

# cleanup the whitespace
for k in MODULES:
    MODULES[k] = textwrap.dedent(MODULES[k]).strip()

class ModloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod_path = tempfile.mkdtemp()
        os.environ[server.MOD_ENV_KEY] = cls.mod_path

        os.mkdir(os.path.join(cls.mod_path, "lib"))
        for p, c in MODULES.items():
            with open(os.path.join(cls.mod_path, p), "w") as f:
                f.write(c)

        cls.mods = server.load_modules()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.mod_path, ignore_errors=True)

    def test_modules_loaded(self):
        self.assertGreater(len(self.mods), 0)

    def test_basic_loading(self):
        self.assertIn("simple", self.mods)
        self.assertEqual(self.mods["simple"].code, MODULES["simple.js"])

    def test_packing(self):
        self.assertIn("loadstuff", self.mods)
        self.assertNotIn("mylib", self.mods)

        self.assertEqual(self.mods["loadstuff"].code, LOADSTUFF_OUTPUT)

        self.assertNotIn("invalidload", self.mods)

    def test_map(self):
        m = self.mods["loadstuff"]
        self.assertEqual(m.true_line_for_mod_line("mylib.js", 5), 7)
        self.assertEqual(m.true_line_for_mod_line("loadstuff", 5), 19)
