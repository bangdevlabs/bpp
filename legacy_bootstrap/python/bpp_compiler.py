import re, json, sys, os

# --- TYPE SYSTEM ---
TY_UNKNOWN = 0
TY_BYTE = 1
TY_HALF = 2
TY_WORD = 3
TY_LONG = 4
TY_PTR = 5
TY_FLOAT = 6

TYPE_NAMES = {0: "unknown", 1: "byte", 2: "half", 3: "word", 4: "long", 5: "ptr", 6: "float"}
TYPE_FROM_KW = {"byte": TY_BYTE, "half": TY_HALF, "word": TY_WORD, "long": TY_LONG, "ptr": TY_PTR, "float": TY_FLOAT}
DECL_KEYWORDS = {"auto", "byte", "half", "word", "long", "ptr"}

# --- LEXER ---
TOKEN_SPEC = [
    ('COMMENT',  r'//.*|/\*[\s\S]*?\*/'), # Priority #1: Ignore comments
    ('IMPORT',   r'\bimport\b'),
    ('KEYWORD',  r'\b(auto|extrn|return|if|else|while|byte|half|word|long|ptr|int|char|void|struct|sizeof)\b'),
    ('HEX',      r'0x[0-9A-Fa-f]+'),
    ('FLOAT',    r'\d+\.\d+'),
    ('NUMBER',   r'\d+'),
    ('ID',       r'[a-zA-Z_][a-zA-Z0-9_]*'),
    ('STRING',   r'"[^"]*"'),
    ('CHAR',     r"'(?:\\[ntr\\']|.)'"),
    ('OP',       r'>>|<<|[+\-*/%=!<>|&^]=?|&&|\|\|'),
    ('PUNCT',    r'[(){}\[\];,.:@]'),
    ('SKIP',     r'[ \t\n\r]+'),
]

_CHAR_ESCAPES = {'n': 10, 't': 9, 'r': 13, '\\': 92, "'": 39}

def tokenize(code):
    tokens = []
    regex = '|'.join('(?P<%s>%s)' % p for p in TOKEN_SPEC)
    for mo in re.finditer(regex, code):
        kind = mo.lastgroup
        if kind == 'SKIP' or kind == 'COMMENT':
            continue
        if kind == 'CHAR':
            # Convert char literal to its integer value.
            inner = mo.group()[1:-1]  # strip surrounding quotes
            if inner.startswith('\\'):
                val = _CHAR_ESCAPES.get(inner[1], ord(inner[1]))
            else:
                val = ord(inner)
            tokens.append(('NUMBER', str(val)))
            continue
        tokens.append((kind, mo.group()))
    return tokens

# --- PARSER ---
class BPlusPlusParser:
    def __init__(self, tokens, source_file=None, imported=None, parent_structs=None):
        self.tokens = tokens
        self.pos = 0
        # USAMOS UM SET PARA GLOBALS: Evita o erro de redefinição no C-Shim
        self.globals_set = set()
        self.global_types = {}
        self.ir = {"extern_functions": {}, "functions": {}, "globals": []}
        self._source_file = source_file
        self._imported = imported if imported is not None else set()
        # Inherit struct definitions from the parent parser so that
        # imported modules can resolve struct field access (e.g. n.ntype).
        self.structs = dict(parent_structs) if parent_structs else {}
        self.var_structs = {}   # {var_name: struct_name} — tracks struct type per variable

    def peek_k(self, off=0):
        idx = self.pos + off
        return self.tokens[idx][0] if idx < len(self.tokens) else None

    def peek_v(self, off=0):
        idx = self.pos + off
        return self.tokens[idx][1] if idx < len(self.tokens) else ""

    def consume(self, expected=None):
        val = self.peek_v()
        self.pos += 1
        return val

    def parse(self):
        while self.pos < len(self.tokens):
            v, k = self.peek_v(), self.peek_k()
            if v == "import": self.parse_import()
            elif v == "struct": self.parse_struct()
            elif v in DECL_KEYWORDS: self.parse_global()
            elif k == "ID":
                # Lookahead: ID followed by '(' is always a function
                if self.peek_v(1) == "(":
                    self.parse_function()
                else:
                    self.parse_global_simple()
            else: self.pos += 1

        # Convert the set back to a list for final JSON
        self.ir["globals"] = list(self.globals_set)
        if self.structs:
            self.ir["structs"] = {name: {"fields": s["fields"], "size": s["size"]}
                                  for name, s in self.structs.items()}
        return self.ir

    def parse_import(self):
        self.consume() # import
        target = self.consume().strip('"')
        if self.peek_v() == ";":
            # File import: import "file.bpp";
            self.consume()
            self._import_file(target)
        elif self.peek_v() == "{":
            # FFI import: import "lib" { ret name(args); ... }
            self.consume()
            while self.peek_v() != "}":
                ret = self.consume()
                if self.peek_v() == "*": ret += self.consume()
                name = self.consume()
                self.consume() # (
                args = []
                while self.peek_v() != ")":
                    arg = self.consume()
                    if self.peek_v() == "*": arg += self.consume()
                    args.append(arg)
                    if self.peek_v() == ",": self.consume()
                self.consume() # )
                self.consume() # ;
                self.ir["extern_functions"][name] = {"lib": target, "ret": ret, "args": args}
            self.consume() # }

    # Standard library search paths (checked when import not found locally).
    _STB_DIRS = ["stb"]

    def _import_file(self, filename):
        # Resolve path relative to current source file
        if self._source_file:
            base = os.path.dirname(os.path.abspath(self._source_file))
            filepath = os.path.join(base, filename)
        else:
            filepath = filename

        # If not found locally, search standard library directories.
        if not os.path.exists(filepath):
            for stb_dir in self._STB_DIRS:
                candidate = os.path.join(os.path.dirname(os.path.abspath(
                    self._source_file or ".")), stb_dir, filename)
                if os.path.exists(candidate):
                    filepath = candidate
                    break

        abspath = os.path.abspath(filepath)
        if abspath in self._imported:
            return
        if not os.path.exists(filepath):
            print(f"IMPORT_ERROR: file not found: {filepath}", file=sys.stderr)
            return
        self._imported.add(abspath)

        with open(filepath, "r") as f:
            code = f.read()

        tokens = tokenize(code)
        sub = BPlusPlusParser(tokens, source_file=filepath, imported=self._imported,
                              parent_structs=self.structs)
        sub_ir = sub.parse()

        # Merge imported definitions into current IR
        self.ir["extern_functions"].update(sub_ir["extern_functions"])
        self.ir["functions"].update(sub_ir["functions"])
        for g in sub_ir["globals"]:
            self.globals_set.add(g)
        self.global_types.update(sub.global_types)
        self.structs.update(sub.structs)
        self.var_structs.update(sub.var_structs)

    def parse_struct(self):
        """Parse struct declaration: struct Name { field1, field2, field3: SubStruct }"""
        self.consume()  # 'struct'
        name = self.consume()
        self.consume()  # '{'
        fields = []
        offset = 0
        while self.peek_v() != "}":
            field_name = self.consume()
            field_struct = None
            field_size = 8  # default: one 64-bit word
            if self.peek_v() == ":":
                self.consume()
                type_name = self.consume()
                if type_name in self.structs:
                    field_struct = type_name
                    field_size = self.structs[type_name]["size"]
            fields.append({"name": field_name, "offset": offset,
                           "size": field_size, "struct_type": field_struct})
            offset += field_size
            if self.peek_v() == ",": self.consume()
        self.consume()  # '}'
        fields_map = {f["name"]: f for f in fields}
        self.structs[name] = {"fields": fields, "fields_map": fields_map, "size": offset}

    def _parse_field_chain(self, base_name):
        """Resolve var.field.field chain to a _field_ref pseudo-node with computed offset."""
        struct_name = self.var_structs.get(base_name)
        if not struct_name:
            return {"type": "var", "name": base_name}
        total_offset = 0
        current_struct = struct_name
        consumed_any = False
        while self.peek_v() == ".":
            if not current_struct:
                break
            struct_info = self.structs.get(current_struct)
            if not struct_info:
                break
            next_field = self.peek_v(1)
            if not next_field:
                break
            field_info = struct_info["fields_map"].get(next_field)
            if not field_info:
                break
            self.consume()  # .
            self.consume()  # field name
            consumed_any = True
            total_offset += field_info["offset"]
            current_struct = field_info.get("struct_type")
        if not consumed_any:
            return {"type": "var", "name": base_name}
        return {"type": "_field_ref", "base": base_name, "offset": total_offset}

    def _field_ref_to_ptr(self, ref):
        """Convert a _field_ref pseudo-node to the pointer expression (base + offset)."""
        return {"type": "binop", "op": "+",
                "left": {"type": "var", "name": ref["base"]},
                "right": {"type": "lit", "val": str(ref["offset"])}}

    def _resolve_field_ref(self, node):
        """Convert _field_ref to mem_load. Pass-through for other node types."""
        if isinstance(node, dict) and node.get("type") == "_field_ref":
            return {"type": "mem_load", "ptr": self._field_ref_to_ptr(node)}
        return node

    def parse_global(self):
        type_kw = self.consume() # auto/byte/half/word/long/ptr
        while True:
            name = self.consume()
            self.globals_set.add(name)
            if type_kw != "auto":
                self.global_types[name] = type_kw
            # Struct type annotation: auto head: Point
            if self.peek_v() == ":":
                self.consume()
                struct_name = self.consume()
                if struct_name in self.structs:
                    self.var_structs[name] = struct_name
            if self.peek_v() == ";": break
            if self.peek_v() == ",": self.consume()
            else: break
        self.consume()

    def parse_global_simple(self):
        name = self.consume()
        self.globals_set.add(name)
        if self.peek_v() == ";": self.consume()

    def parse_function(self):
        name = self.consume()
        self.consume()  # (
        # Save var_structs so function-local types don't leak to other functions
        saved_var_structs = dict(self.var_structs)
        params = []
        while self.peek_v() != ")":
            pname = self.consume()
            params.append(pname)
            # Struct type annotation on parameter: fn(p: Point)
            if self.peek_v() == ":":
                self.consume()
                struct_name = self.consume()
                if struct_name in self.structs:
                    self.var_structs[pname] = struct_name
            if self.peek_v() == ",": self.consume()
        self.consume(); self.consume()  # ) and {
        body = []
        while self.peek_v() != "}" and self.pos < len(self.tokens):
            body.append(self.parse_statement())
        self.consume()  # }
        self.ir["functions"][name] = {"params": params, "body": body}
        self.var_structs = saved_var_structs

    def parse_statement(self):
        v = self.peek_v()
        # Local struct definition inside a function body
        if v == "struct":
            self.parse_struct()
            return {"type": "nop"}
        if v in DECL_KEYWORDS:
            # Disambiguate: "ptr x;" is a typed declaration, "ptr = expr;" is an assignment.
            if v != "auto" and self.peek_v(1) == "=":
                pass  # Fall through to expression parsing below
            else:
                type_hint = v if v != "auto" else None
                self.consume(); decls = []
                while True:
                    var_name = self.consume()
                    decls.append(var_name)
                    # Struct type annotation: auto p: Point
                    if self.peek_v() == ":":
                        self.consume()
                        struct_name = self.consume()
                        if struct_name in self.structs:
                            self.var_structs[var_name] = struct_name
                    if self.peek_v() == ";": break
                    if self.peek_v() == ",": self.consume()
                self.consume()
                node = {"type": "decl", "vars": decls}
                if type_hint:
                    node["type_hint"] = type_hint
                return node

        # Dispatch hint: @seq, @par, @gpu before while.
        dispatch_hint = None
        if v == "@":
            self.consume()
            dispatch_hint = self.consume()
            v = self.peek_v()

        if v in ["if", "while"]:
            st = self.consume(); self.consume(); cond = self.parse_expr(); self.consume()
            body = []
            if self.peek_v() == "{":
                self.consume()
                while self.peek_v() != "}": body.append(self.parse_statement())
                self.consume()
            else: body.append(self.parse_statement())
            node = {"type": st, "cond": cond, "body": body}
            if dispatch_hint and st == "while":
                node["dispatch_hint"] = dispatch_hint
            # Handle else branch for if statements.
            if st == "if" and self.peek_v() == "else":
                self.consume()
                else_body = []
                if self.peek_v() == "{":
                    self.consume()
                    while self.peek_v() != "}": else_body.append(self.parse_statement())
                    self.consume()
                else: else_body.append(self.parse_statement())
                node["else_body"] = else_body
            return node

        if v == "return":
            self.consume(); e = self.parse_expr(); self.consume(); return {"type": "return", "val": e}

        e = self.parse_expr()
        if self.peek_v() == ";": self.consume()
        return e

    # Operator precedence table (higher number = tighter binding)
    PREC = {
        '||': 1, '&&': 2,
        '|': 3, '^': 4, '&': 5,
        '==': 6, '!=': 6,
        '<': 7, '<=': 7, '>': 7, '>=': 7,
        '<<': 8, '>>': 8,
        '+': 9, '-': 9, '*': 10, '/': 10, '%': 10,
    }

    def _op_prec(self):
        # Returns the precedence of the current token if it is a binary operator.
        if self.peek_k() != "OP" or self.peek_v() in [";", ")", ",", "}", "="]:
            return -1
        return self.PREC.get(self.peek_v(), -1)

    def _parse_unary(self):
        # Handles prefix unary operators: dereference (*) and negation (-).
        if self.peek_v() == "*":
            self.consume()
            result = self.parse_primary()
            return {"type": "mem_load", "ptr": self._resolve_field_ref(result)}
        if self.peek_v() == "-":
            self.consume()
            return {"type": "unary_op", "op": "-", "right": self._parse_unary()}
        return self._resolve_field_ref(self.parse_primary())

    def _parse_binops(self, left, min_prec):
        # Precedence climbing loop for binary operators.
        while self._op_prec() >= min_prec:
            op = self.consume()
            prec = self.PREC.get(op, 0)
            right = self._parse_unary()
            while self._op_prec() > prec:
                right = self._parse_binops(right, self._op_prec())
            left = {"type": "binop", "op": op, "left": left, "right": right}
        return left

    def parse_expr(self):
        # Handles dereference-assignment (mem_store), plain assignment, and expressions.
        if self.peek_v() == "*":
            self.consume()
            ptr = self._resolve_field_ref(self.parse_primary())
            if self.peek_v() == "=":
                self.consume()
                return {"type": "mem_store", "ptr": ptr, "val": self.parse_expr()}
            left = {"type": "mem_load", "ptr": ptr}
        elif self.peek_v() == "-":
            self.consume()
            left = {"type": "unary_op", "op": "-", "right": self._parse_unary()}
        else:
            left = self.parse_primary()
            # Field access: decide between mem_load (read) and mem_store (write)
            if isinstance(left, dict) and left.get("type") == "_field_ref":
                ptr_expr = self._field_ref_to_ptr(left)
                if self.peek_v() == "=":
                    self.consume()
                    return {"type": "mem_store", "ptr": ptr_expr, "val": self.parse_expr()}
                left = {"type": "mem_load", "ptr": ptr_expr}
            elif self.peek_v() == "=":
                self.consume()
                return {"type": "assign", "left": left, "right": self.parse_expr()}
        return self._parse_binops(left, 0)

    def parse_primary(self):
        k, v = self.peek_k(), self.peek_v()
        if k in ["NUMBER", "HEX", "FLOAT", "STRING"]: return {"type": "lit", "val": self.consume()}
        # sizeof(TypeName): resolved at compile time to the struct's byte size
        if k == "KEYWORD" and v == "sizeof":
            self.consume()   # sizeof
            self.consume()   # (
            type_name = self.consume()
            self.consume()   # )
            size = self.structs[type_name]["size"] if type_name in self.structs else 8
            return {"type": "lit", "val": str(size)}
        if k == "ID":
            name = self.consume()
            if self.peek_v() == "(":
                self.consume(); args = []
                while self.peek_v() != ")":
                    args.append(self.parse_expr())
                    if self.peek_v() == ",": self.consume()
                self.consume()
                return {"type": "call", "name": name, "args": args}
            # Field access chain: var.field.field → _field_ref pseudo-node
            if self.peek_v() == ".":
                return self._parse_field_chain(name)
            return {"type": "var", "name": name}
        if v == "(":
            self.consume(); e = self.parse_expr(); self.consume()
            return e
        self.pos += 1
        return {"type": "nop"}

# --- TYPE INFERENCE ---

def add_type(node, env):
    """Walk an AST node bottom-up and return its inferred type code."""
    if not isinstance(node, dict) or "type" not in node:
        return TY_UNKNOWN

    t = node["type"]

    if t == "lit":
        val = node["val"]
        if val.startswith('"'):
            return TY_PTR
        if "." in val:
            return TY_FLOAT
        n = int(val, 16) if val.startswith("0x") else int(val)
        if n <= 0xFF: return TY_BYTE
        if n <= 0xFFFF: return TY_HALF
        if n <= 0xFFFFFFFF: return TY_WORD
        return TY_LONG

    if t == "var":
        return env["vars"].get(node["name"], TY_UNKNOWN)

    if t == "binop":
        lt = add_type(node["left"], env)
        rt = add_type(node["right"], env)
        if lt == TY_PTR or rt == TY_PTR:
            return TY_PTR
        return max(lt, rt)

    if t == "unary_op":
        return add_type(node["right"], env)

    if t == "assign":
        rt = add_type(node["right"], env)
        if node["left"]["type"] == "var":
            name = node["left"]["name"]
            old = env["vars"].get(name, TY_UNKNOWN)
            # Promote to the largest type seen across all assignments
            env["vars"][name] = max(old, rt) if old != TY_UNKNOWN else rt
        return rt

    if t == "mem_load":
        add_type(node["ptr"], env)
        return TY_LONG

    if t == "mem_store":
        add_type(node["ptr"], env)
        add_type(node["val"], env)
        return TY_UNKNOWN

    if t == "call":
        for arg in node.get("args", []):
            add_type(arg, env)
        name = node["name"]
        if name == "malloc": return TY_PTR
        if name == "peek": return TY_BYTE
        if name == "poke": return TY_UNKNOWN
        return env["funcs"].get(name, TY_LONG)

    if t == "return":
        return add_type(node["val"], env)

    if t == "decl":
        hint = node.get("type_hint")
        if hint:
            ty = TYPE_FROM_KW[hint]
            for var in node["vars"]:
                env["vars"][var] = ty
        return TY_UNKNOWN

    if t in ("if", "while"):
        add_type(node["cond"], env)
        for stmt in node.get("body", []):
            add_type(stmt, env)
        for stmt in node.get("else_body", []):
            add_type(stmt, env)
        return TY_UNKNOWN

    return TY_UNKNOWN


def infer_types(ir):
    """Run two-pass type inference on the full IR. Returns per-function type info."""
    func_types = {}
    warnings = []

    # Seed extern function return types from FFI declarations
    ffi_map = {"void": TY_UNKNOWN, "int": TY_WORD, "char": TY_BYTE,
               "word": TY_PTR, "long": TY_LONG}
    for name, ext in ir["extern_functions"].items():
        ret = ext.get("ret", "int")
        if name == "malloc":
            func_types[name] = TY_PTR
        elif ret in TYPE_FROM_KW:
            func_types[name] = TYPE_FROM_KW[ret]
        elif ret in ffi_map:
            func_types[name] = ffi_map[ret]
        else:
            func_types[name] = TY_LONG

    # Pass 1: determine function return types from their bodies
    for name, func in ir["functions"].items():
        env = {"vars": {}, "funcs": func_types}
        for p in func["params"]:
            env["vars"][p] = TY_LONG
        ret = TY_UNKNOWN
        for stmt in func["body"]:
            ty = add_type(stmt, env)
            if isinstance(stmt, dict) and stmt.get("type") == "return":
                ret = max(ret, ty)
        func_types[name] = ret if ret != TY_UNKNOWN else TY_LONG

    # Pass 2: re-run with complete function type knowledge
    result = {}
    for name, func in ir["functions"].items():
        env = {"vars": {}, "funcs": func_types}
        for p in func["params"]:
            env["vars"][p] = TY_LONG
        # Apply global type hints
        for gname, gtype in ir.get("_global_types", {}).items():
            if gtype in TYPE_FROM_KW:
                env["vars"][gname] = TYPE_FROM_KW[gtype]
        ret = TY_UNKNOWN
        for stmt in func["body"]:
            ty = add_type(stmt, env)
            if isinstance(stmt, dict) and stmt.get("type") == "return":
                ret = max(ret, ty)
        final_ret = ret if ret != TY_UNKNOWN else func_types[name]

        for var, ty in env["vars"].items():
            if ty == TY_UNKNOWN:
                warnings.append(f"{name}(): '{var}' type unknown — defaults to long")

        result[name] = {
            "return": TYPE_NAMES[final_ret],
            "vars": {k: TYPE_NAMES[v] for k, v in env["vars"].items()}
        }

    for w in warnings:
        print(f"  warning: {w}", file=sys.stderr)

    return result


# --- DISPATCH ANALYSIS ---

# Extern functions known to have no observable side effects
PURE_EXTERNS = {"malloc", "peek", "rand"}


def _contains_var(node, name):
    """Check if an expression tree references a specific variable."""
    if not isinstance(node, dict): return False
    if node.get("type") == "var" and node.get("name") == name: return True
    for key in ("left", "right", "val", "ptr"):
        if key in node and _contains_var(node[key], name): return True
    return False


def _find_loop_var(body):
    """Find the induction variable (incremented at end of loop body)."""
    for stmt in reversed(body):
        if not isinstance(stmt, dict): continue
        if stmt["type"] == "assign":
            left = stmt.get("left", {})
            right = stmt.get("right", {})
            # Pattern: i = i + N
            if left.get("type") == "var" and right.get("type") == "binop":
                if right["op"] == "+" and right.get("left", {}).get("type") == "var":
                    if right["left"]["name"] == left["name"]:
                        return left["name"]
    return None


def _find_strided_vars(body, loop_var):
    """Find variables assigned as base + (loop_var * stride)."""
    if not loop_var: return set()
    strided = set()
    for stmt in body:
        if not isinstance(stmt, dict) or stmt["type"] != "assign": continue
        left = stmt.get("left", {})
        right = stmt.get("right", {})
        if left.get("type") != "var": continue
        # Pattern: var = base + (loop_var * stride)
        if right.get("type") == "binop" and right.get("op") == "+":
            r = right.get("right", {})
            if isinstance(r, dict) and r.get("type") == "binop" and r.get("op") == "*":
                if _contains_var(r, loop_var):
                    strided.add(left["name"])
    return strided


def _node_side_effects(node, safe_vars, impure_externs):
    """Return list of side effect descriptions found in an AST node."""
    if not isinstance(node, dict): return []
    effects = []
    t = node["type"]

    if t == "call":
        name = node["name"]
        if name in impure_externs:
            effects.append(name)
        if name == "poke":
            effects.append("poke")

    if t == "assign" and node.get("left", {}).get("type") == "var":
        if node["left"]["name"] not in safe_vars:
            effects.append(f"global_write:{node['left']['name']}")

    if t == "mem_store":
        # Safe if pointer base is a strided variable
        ptr = node.get("ptr", {})
        if ptr.get("type") == "binop" and ptr.get("op") == "+":
            base = ptr.get("left", {})
            if base.get("type") == "var" and base["name"] in safe_vars:
                pass  # Strided store — safe for parallelism
            else:
                effects.append("mem_store")
        else:
            effects.append("mem_store")

    # Recurse into children
    for key in ("left", "right", "val", "ptr", "cond"):
        if key in node:
            effects.extend(_node_side_effects(node[key], safe_vars, impure_externs))
    for key in ("body", "else_body", "args"):
        if key in node:
            for child in node[key]:
                effects.extend(_node_side_effects(child, safe_vars, impure_externs))
    return effects


def _classify_loop(while_node, locals_set, impure_externs):
    """Classify a while loop for dispatch: parallel, gpu_candidate, split_candidate, or sequential."""
    # If the programmer provided a dispatch hint, use it directly.
    hint = while_node.get("dispatch_hint")
    if hint:
        hint_map = {"seq": "sequential", "par": "parallel", "gpu": "gpu_candidate"}
        if hint in hint_map:
            return {"dispatch": hint_map[hint], "reason": f"programmer hint @{hint}"}
    body = while_node.get("body", [])
    loop_var = _find_loop_var(body)
    strided = _find_strided_vars(body, loop_var)
    has_stride = len(strided) > 0

    # Variables that are safe to read/write within the loop without causing cross-iteration conflicts
    safe_vars = locals_set | strided | ({loop_var} if loop_var else set())

    # Collect side effects per statement, separating pure from impure
    pure_count = 0
    impure_count = 0
    all_effects = []
    for stmt in body:
        fx = _node_side_effects(stmt, safe_vars, impure_externs)
        if fx:
            impure_count += 1
            all_effects.extend(fx)
        else:
            pure_count += 1

    # Deduplicate effect names
    unique_effects = list(dict.fromkeys(all_effects))

    if not all_effects and has_stride:
        return {"dispatch": "gpu_candidate", "loop_var": loop_var,
                "strided_vars": sorted(strided),
                "reason": "strided memory access, no side effects — can run on GPU"}
    elif not all_effects:
        return {"dispatch": "parallel", "loop_var": loop_var,
                "reason": "no cross-iteration dependencies"}
    elif has_stride and pure_count > 0:
        return {"dispatch": "split_candidate", "loop_var": loop_var,
                "strided_vars": sorted(strided),
                "effects": unique_effects,
                "reason": f"{pure_count} pure + {impure_count} impure stmts — split update from draw"}
    else:
        return {"dispatch": "sequential",
                "effects": unique_effects,
                "reason": "side effects throughout"}


def _analyze_loops(stmts, locals_set, impure_externs):
    """Recursively find and classify all loops in a statement list."""
    loops = []
    for stmt in stmts:
        if not isinstance(stmt, dict): continue
        if stmt["type"] == "while":
            info = _classify_loop(stmt, locals_set, impure_externs)
            inner = _analyze_loops(stmt.get("body", []), locals_set, impure_externs)
            if inner:
                info["inner"] = inner
            loops.append(info)
        elif stmt["type"] == "if":
            loops.extend(_analyze_loops(stmt.get("body", []), locals_set, impure_externs))
            loops.extend(_analyze_loops(stmt.get("else_body", []), locals_set, impure_externs))
    return loops


def analyze_dispatch(ir):
    """Analyze every function for purity and loop dispatch opportunities."""
    impure_externs = set(ir["extern_functions"].keys()) - PURE_EXTERNS
    result = {}
    for fname, func in ir["functions"].items():
        locals_set = set(func["params"])
        for stmt in func["body"]:
            if isinstance(stmt, dict) and stmt["type"] == "decl":
                locals_set.update(stmt["vars"])
        # Function-level purity (conservative: mem_store = side effect)
        all_fx = []
        for stmt in func["body"]:
            all_fx.extend(_node_side_effects(stmt, locals_set, impure_externs))
        pure = len(all_fx) == 0
        loops = _analyze_loops(func["body"], locals_set, impure_externs)
        result[fname] = {"pure": pure, "loops": loops}
    return result


# --- C EMITTER ---

# Map FFI type names to C type strings for prototypes and casts.
FFI_TO_C = {
    "void": "void", "int": "int", "char": "char", "char*": "const char*",
    "long": "long long", "word": "uint32_t", "byte": "uint8_t",
    "half": "uint16_t", "ptr": "void*", "float": "float", "double": "double",
}

# FFI arg types that represent pointers (need mem_base + offset conversion).
FFI_PTR_TYPES = {"word", "char*", "void*", "ptr"}

# FFI arg types that are plain value casts.
FFI_VAL_CAST = {
    "void": "", "int": "(int)", "char": "(char)", "long": "(long long)",
    "byte": "(uint8_t)", "half": "(uint16_t)", "float": "(float)", "double": "(double)",
}


def _expr_is_float(node, float_vars, float_funcs=set()):
    """Return True if an expression produces a float value."""
    if not isinstance(node, dict):
        return False
    t = node.get("type")
    if t == "lit":
        val = node.get("val", "")
        return "." in val and not val.startswith('"')
    if t == "var":
        return node.get("name") in float_vars
    if t == "binop":
        return _expr_is_float(node["left"], float_vars, float_funcs) or _expr_is_float(node["right"], float_vars, float_funcs)
    if t == "unary_op":
        return _expr_is_float(node.get("right"), float_vars, float_funcs)
    if t == "call":
        return node.get("name", "") in float_funcs
    return False


def _infer_float_vars(body, float_funcs=set()):
    """Infer which local variables hold float values via fixed-point iteration."""
    float_vars = set()
    def scan(stmts):
        for stmt in stmts:
            if not isinstance(stmt, dict):
                continue
            t = stmt.get("type")
            if t == "assign":
                left = stmt.get("left")
                right = stmt.get("right")
                if left and left.get("type") == "var" and _expr_is_float(right, float_vars, float_funcs):
                    name = left["name"]
                    if name not in float_vars:
                        float_vars.add(name)
                        nonlocal changed_outer
                        changed_outer = True
            elif t == "if":
                scan(stmt.get("body", []))
                scan(stmt.get("else_body", []))
            elif t == "while":
                scan(stmt.get("body", []))
    # Fixed-point iteration: propagate float types until stable.
    changed_outer = True
    while changed_outer:
        changed_outer = False
        scan(body)
    return float_vars


INT_ONLY_OPS = {"%", "&", "|", "^", "<<", ">>"}

def _uses_int_ops(body, var_name):
    """Check if a variable is used with integer-only operators anywhere in the function body."""
    def check_expr(node):
        """Recursively check if var_name appears as operand of an int-only op."""
        if not isinstance(node, dict):
            return False
        if node.get("type") == "binop" and node.get("op") in INT_ONLY_OPS:
            left = node.get("left", {})
            right = node.get("right", {})
            if left.get("type") == "var" and left.get("name") == var_name:
                return True
            if right.get("type") == "var" and right.get("name") == var_name:
                return True
        # Recurse into sub-expressions.
        for key in ("left", "right", "val", "ptr", "cond"):
            if key in node and isinstance(node[key], dict):
                if check_expr(node[key]):
                    return True
        for key in ("args",):
            if key in node and isinstance(node[key], list):
                for item in node[key]:
                    if isinstance(item, dict) and check_expr(item):
                        return True
        return False

    for stmt in body:
        if not isinstance(stmt, dict):
            continue
        if check_expr(stmt):
            return True
        for key in ("body", "else_body"):
            if key in stmt and isinstance(stmt[key], list):
                if _uses_int_ops(stmt[key], var_name):
                    return True
    return False


def _collect_float_call_params(ir, all_fv, float_funcs):
    """Find function parameters that receive float arguments at call sites,
    excluding parameters used with integer-only operators in the callee."""
    float_param_idx = {}
    def scan(stmts, caller_fv):
        for stmt in stmts:
            if not isinstance(stmt, dict):
                continue
            if stmt.get("type") == "call":
                fname = stmt.get("name", "")
                if fname in ir["functions"]:
                    for i, arg in enumerate(stmt.get("args", [])):
                        if _expr_is_float(arg, caller_fv, float_funcs):
                            float_param_idx.setdefault(fname, set()).add(i)
            for key in ("left", "right", "val", "ptr", "cond"):
                if key in stmt and isinstance(stmt[key], dict):
                    scan([stmt[key]], caller_fv)
            for key in ("body", "else_body", "args"):
                if key in stmt and isinstance(stmt[key], list):
                    scan(stmt[key], caller_fv)
    for caller_name, func in ir["functions"].items():
        scan(func["body"], all_fv.get(caller_name, set()))
    return float_param_idx


def _infer_all_floats(ir):
    """Infer float variables and return types across all functions with cross-function propagation."""
    float_funcs = set()
    all_fv = {name: set() for name in ir["functions"]}
    changed = True
    while changed:
        changed = False
        # Local inference within each function body.
        for name, func in ir["functions"].items():
            fv = _infer_float_vars(func["body"], float_funcs)
            for v in fv:
                if v not in all_fv[name]:
                    all_fv[name].add(v)
                    changed = True
        # Cross-function: propagate float args to callee params.
        fp_idx = _collect_float_call_params(ir, all_fv, float_funcs)
        for fname, indices in fp_idx.items():
            if fname not in ir["functions"]:
                continue
            params = ir["functions"][fname]["params"]
            body = ir["functions"][fname]["body"]
            for i in indices:
                if i < len(params):
                    pname = params[i]
                    # Skip if the param is used with integer-only ops like %.
                    if pname not in all_fv[fname] and not _uses_int_ops(body, pname):
                        all_fv[fname].add(pname)
                        changed = True
        # Determine return types.
        for name, func in ir["functions"].items():
            for stmt in func["body"]:
                if isinstance(stmt, dict) and stmt.get("type") == "return":
                    if _expr_is_float(stmt.get("val"), all_fv[name], float_funcs):
                        if name not in float_funcs:
                            float_funcs.add(name)
                            changed = True
    result = {}
    for name in ir["functions"]:
        ret = "double" if name in float_funcs else "long long"
        result[name] = (all_fv[name], ret)
    return result


def emit_c(ir, type_info=None):
    """Generate C source code from B++ IR."""
    out = []

    # Collect which libs are used and which extern functions are actually called.
    used_externs = _find_used_externs(ir)
    libs_used = set()
    for name in used_externs:
        ext = ir["extern_functions"].get(name, {})
        libs_used.add(ext.get("lib", ""))

    # Standard includes
    out.append("#include <stdint.h>")
    out.append("#include <stdio.h>")
    out.append("#include <stdlib.h>")
    out.append("#include <string.h>")

    # Lib-specific includes
    for lib in sorted(libs_used):
        if lib == "libc":
            continue  # Already included above
        if lib:
            out.append(f"#include <{lib}.h>")

    out.append("")

    # Real pointer model — malloc returns real addresses, no mem_base.
    out.append("#include <stdlib.h>")
    out.append("static uint8_t _bpp_scratch[16];")
    out.append("")

    # Built-in I/O runtime — raw ARM64 syscalls, no libc dependency.
    out.append("// B++ I/O runtime — raw syscalls.")
    out.append("static long long bpp_sys_write(long long fd, const void* buf, long long len) {")
    out.append('  register long long x0 __asm__("x0") = fd;')
    out.append('  register long long x1 __asm__("x1") = (long long)buf;')
    out.append('  register long long x2 __asm__("x2") = len;')
    out.append('  register long long x16 __asm__("x16") = 4;')
    out.append('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");')
    out.append("  return x0;")
    out.append("}")
    out.append("static long long bpp_sys_read(long long fd, void* buf, long long len) {")
    out.append('  register long long x0 __asm__("x0") = fd;')
    out.append('  register long long x1 __asm__("x1") = (long long)buf;')
    out.append('  register long long x2 __asm__("x2") = len;')
    out.append('  register long long x16 __asm__("x16") = 3;')
    out.append('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");')
    out.append("  return x0;")
    out.append("}")
    out.append("static long long bpp_sys_open(const char* path, long long flags) {")
    out.append('  register long long x0 __asm__("x0") = (long long)path;')
    out.append('  register long long x1 __asm__("x1") = flags;')
    out.append('  register long long x2 __asm__("x2") = 0;')
    out.append('  register long long x16 __asm__("x16") = 5;')
    out.append('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");')
    out.append("  return x0;")
    out.append("}")
    out.append("static long long bpp_sys_close(long long fd) {")
    out.append('  register long long x0 __asm__("x0") = fd;')
    out.append('  register long long x16 __asm__("x16") = 6;')
    out.append('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc" : "+r"(x0) : "r"(x16) : "memory", "cc");')
    out.append("  return x0;")
    out.append("}")
    out.append("static int _bpp_argc;")
    out.append("static char** _bpp_argv;")
    out.append("")

    # Globals
    for g in sorted(ir.get("globals", [])):
        out.append(f"long long {g} = 0;")
    if ir.get("globals"):
        out.append("")

    # Forward declarations for all B++ functions (with inferred float types).
    func_float_info = _infer_all_floats(ir)
    for name, func in ir["functions"].items():
        cname = "main_bpp" if name == "main" else name
        fv, ret = func_float_info[name]
        ptypes = []
        for p in func["params"]:
            pt = "double" if p in fv else "long long"
            ptypes.append(f"{pt} {p}")
        plist = ", ".join(ptypes) if ptypes else "void"
        out.append(f"{ret} {cname}({plist});")
    out.append("")

    # Extern function prototypes (only for functions actually called)
    for name in sorted(used_externs):
        ext = ir["extern_functions"].get(name)
        if not ext or ext.get("lib") == "libc":
            continue  # libc functions come from standard headers
        if name in ("malloc", "printf", "getchar", "putchar", "strncmp", "rand"):
            continue  # Standard C functions, already declared by headers
        ret_c = FFI_TO_C.get(ext["ret"], "int")
        args_c = []
        for a in ext["args"]:
            args_c.append(FFI_TO_C.get(a, "long long"))
        astr = ", ".join(args_c) if args_c else "void"
        out.append(f"// FFI: {name}")
        # Don't redeclare if the lib header already provides it
        # out.append(f"{ret_c} {name}({astr});")
    if used_externs:
        out.append("")

    # Function definitions
    for name, func in ir["functions"].items():
        cname = "main_bpp" if name == "main" else name
        float_vars, ret_type = func_float_info[name]
        ptypes = []
        for p in func["params"]:
            pt = "double" if p in float_vars else "long long"
            ptypes.append(f"{pt} {p}")
        plist = ", ".join(ptypes) if ptypes else "void"
        out.append(f"{ret_type} {cname}({plist}) {{")

        for stmt in func["body"]:
            if not isinstance(stmt, dict):
                continue
            if stmt.get("type") == "decl":
                # Split variables into float and int groups.
                int_vars = [v for v in stmt.get("vars", []) if v not in float_vars]
                flt_vars = [v for v in stmt.get("vars", []) if v in float_vars]
                if int_vars:
                    out.append(f"  long long {', '.join(int_vars)};")
                if flt_vars:
                    out.append(f"  double {', '.join(flt_vars)};")
            else:
                code = _emit_node(stmt, ir, 1)
                if code:
                    out.append(f"  {code};")

        out.append("  return 0;")
        out.append("}")
        out.append("")

    # Main entry point
    out.append("int main(int argc, char** argv) {")
    out.append("  _bpp_argc = argc;")
    out.append("  _bpp_argv = argv;")
    if "main" in ir["functions"]:
        out.append("  main_bpp();")
    out.append("  return 0;")
    out.append("}")

    return "\n".join(out) + "\n"


def _find_used_externs(ir):
    """Find all extern functions actually called in the program."""
    used = set()
    extern_names = set(ir["extern_functions"].keys())
    for func in ir["functions"].values():
        _collect_calls(func["body"], extern_names, used)
    return used


def _collect_calls(stmts, extern_names, used):
    """Recursively collect extern function calls from a statement list."""
    for stmt in stmts:
        if not isinstance(stmt, dict):
            continue
        if stmt.get("type") == "call" and stmt.get("name") in extern_names:
            used.add(stmt["name"])
        for key in ("left", "right", "val", "ptr", "cond"):
            if key in stmt and isinstance(stmt[key], dict):
                _collect_calls([stmt[key]], extern_names, used)
        for key in ("body", "else_body", "args"):
            if key in stmt and isinstance(stmt[key], list):
                _collect_calls(stmt[key], extern_names, used)


def _emit_node(node, ir, indent=0):
    """Translate an AST node to C code."""
    if not node or not isinstance(node, dict):
        return str(node) if node else ""
    t = node.get("type")
    pad = "  " * indent

    if t == "lit":
        val = node.get("val", "0")
        if val.startswith('"'):
            return f"(long long){val}"
        return str(val)

    if t == "var":
        return node.get("name", "")

    if t == "nop":
        return ""

    if t == "mem_load":
        ptr = _emit_node(node.get("ptr"), ir)
        return f"(*(long long*)((uintptr_t)({ptr})))"

    if t == "mem_store":
        ptr = _emit_node(node.get("ptr"), ir)
        val = _emit_node(node.get("val"), ir)
        return f"*(long long*)((uintptr_t)({ptr})) = {val}"

    if t == "unary_op":
        return f"({node['op']}{_emit_node(node['right'], ir)})"

    if t == "binop":
        left = _emit_node(node["left"], ir)
        right = _emit_node(node["right"], ir)
        op = node["op"]
        return f"({left} {op} {right})"

    if t == "assign":
        left = _emit_node(node.get("left"), ir)
        right = _emit_node(node.get("right"), ir)
        return f"{left} = {right}"

    if t == "return":
        return f"return {_emit_node(node.get('val'), ir)}"

    if t == "call":
        name = node.get("name")
        args = [_emit_node(a, ir) for a in node.get("args", [])]

        # B++ malloc maps to real malloc.
        if name == "malloc":
            return f"(long long)malloc({', '.join(args)})"

        # B++ peek/poke map to real pointer byte access.
        if name == "peek":
            return f"((long long)(*(uint8_t*)((uintptr_t)({args[0]}))))" if args else "0"
        if name == "poke":
            if len(args) >= 2:
                return f"(*(uint8_t*)((uintptr_t)({args[0]})) = (uint8_t)({args[1]}))"
            return "0"

        # Built-in I/O: putchar writes 1 byte to stdout via raw syscall.
        if name == "putchar":
            return f"(_bpp_scratch[0] = (uint8_t)({args[0]}), bpp_sys_write(1, _bpp_scratch, 1))" if args else "0"

        # Built-in I/O: getchar reads 1 byte from stdin via raw syscall.
        if name == "getchar":
            return "(_bpp_scratch[0] = 0, bpp_sys_read(0, _bpp_scratch, 1) > 0 ? (long long)_bpp_scratch[0] : -1LL)"

        # Built-in: str_peek reads a byte from a string pointer.
        if name == "str_peek":
            if len(args) >= 2:
                s, i = args[0], args[1]
                return f"(long long)((uint8_t*)((uintptr_t)({s})))[({i})]"
            return "0"

        # Built-in: sys_open opens a file descriptor via raw syscall.
        if name == "sys_open":
            return f"bpp_sys_open((const char*)((uintptr_t)({args[0]})), {args[1]})" if len(args) >= 2 else "0"

        # Built-in: sys_read reads bytes from fd into a buffer via raw syscall.
        if name == "sys_read":
            return f"bpp_sys_read({args[0]}, (void*)((uintptr_t)({args[1]})), {args[2]})" if len(args) >= 3 else "0"

        # Built-in: sys_write writes bytes from buffer to fd via raw syscall.
        if name == "sys_write":
            return f"bpp_sys_write({args[0]}, (void*)((uintptr_t)({args[1]})), {args[2]})" if len(args) >= 3 else "0"

        # Built-in: sys_close closes a file descriptor via raw syscall.
        if name == "sys_close":
            return f"bpp_sys_close({args[0]})" if args else "0"

        # Built-in: argv_get reads argv[i] as a pointer value.
        if name == "argv_get":
            return f"((long long)_bpp_argv[(int)({args[0]})])" if args else "0"

        # Built-in: argc_get returns the argument count.
        if name == "argc_get":
            return "((long long)_bpp_argc)"

        # FFI calls: apply casts from extern declarations.
        ext = ir["extern_functions"].get(name)
        if ext:
            cast_args = []
            ext_arg_types = ext.get("args", [])
            raw_args = node.get("args", [])
            for i, a in enumerate(args):
                if i < len(ext_arg_types):
                    atype = ext_arg_types[i]
                    # Special: Color arguments in raylib need GetColor()
                    if name in ("ClearBackground", "DrawRectangle") and i == len(args) - 1:
                        if atype == "int":
                            cast_args.append(f"GetColor((uint32_t){a})")
                            continue
                    # Pointer-type FFI args: cast to pointer.
                    if atype in FFI_PTR_TYPES:
                        cast_args.append(f"(const char*)((uintptr_t)({a}))")
                    else:
                        cast = FFI_VAL_CAST.get(atype, "")
                        cast_args.append(f"{cast}{a}" if cast else a)
                else:
                    cast_args.append(a)
            return f"{name}({', '.join(cast_args)})"

        return f"{name}({', '.join(args)})"

    if t == "if":
        cond = _emit_node(node.get("cond"), ir)
        body = "\n".join(f"{pad}  {_emit_node(s, ir, indent+1)};" for s in node.get("body", []) if isinstance(s, dict))
        result = f"if ({cond}) {{\n{body}\n{pad}}}"
        if node.get("else_body"):
            eb = "\n".join(f"{pad}  {_emit_node(s, ir, indent+1)};" for s in node["else_body"] if isinstance(s, dict))
            result += f" else {{\n{eb}\n{pad}}}"
        return result

    if t == "while":
        cond = _emit_node(node.get("cond"), ir)
        body = "\n".join(f"{pad}  {_emit_node(s, ir, indent+1)};" for s in node.get("body", []) if isinstance(s, dict))
        return f"while ({cond}) {{\n{body}\n{pad}}}"

    return ""


if __name__ == "__main__":
    try:
        show_types = "--types" in sys.argv
        show_dispatch = "--dispatch" in sys.argv
        do_emit = "--emit" in sys.argv
        args = [a for a in sys.argv[1:] if not a.startswith("--")]
        target = args[0] if args else "raylib_demo.bpp"
        with open(target, "r") as f: code = f.read()
        tokens = tokenize(code)
        parser = BPlusPlusParser(tokens, source_file=target)
        ir = parser.parse()

        if do_emit or show_types or show_dispatch:
            ir["_global_types"] = parser.global_types
            type_info = infer_types(ir)
            del ir["_global_types"]

        if do_emit:
            print(emit_c(ir, type_info))
        else:
            if show_types:
                ir["type_info"] = type_info
            if show_dispatch:
                ir["dispatch_info"] = analyze_dispatch(ir)
            print(json.dumps(ir, indent=2))
    except Exception as e:
        print(f"COMPILER_ERROR: {e}", file=sys.stderr); sys.exit(1)
