from __future__ import with_statement
from cStringIO import StringIO

def camel_to_underscores(s, upper = False):
    res = ""
    for i in range(len(s)):
        c = s[i]
        if i > 0 and c.isupper():
            res = res + "_"
        if upper:
            res = res + c.upper()
        else:
            res = res + c.lower()
    return res

def underscores_to_camel(s):
    res = ""
    do_upper = True
    for i in range(len(s)):
        c = s[i]
        if c == "_":
            do_upper = True
        else:
            if do_upper:
                res = res + c.upper()
            else:
                res = res + c
            do_upper = False
    return res

proto_prefix = "Temp"

def set_prefix(prefix):
    global proto_prefix
    global proto_prefix_upper
    global proto_prefix_lower
    proto_prefix = prefix
    proto_prefix_upper = prefix.upper()
    proto_prefix_lower = prefix.lower()

def prefix_underscore_upper(*args):
    s = proto_prefix_upper
    for arg in args:
        s = s + "_" + arg
    return s

def prefix_underscore_lower(*args):
    s = proto_prefix_lower
    for arg in args:
        s = s + "_" + arg
    return s

def prefix_camel(*args):
    s = proto_prefix
    for arg in args:
        s = s + underscores_to_camel(arg)
    return s

def increment_identifier(idf):
    v = idf[-1:]
    if v.isdigit():
        return idf[:-1] + str(int(v) + 1)
    return idf + "2"

def sum_array(array):
    if len(array) == 0:
        return 0
    return " + ".join(array)

class CodeWriter:
    def __init__(self):
        self.out = StringIO()
        self.contents = [self.out]
        self.indentation = 0
        self.at_line_start = True
        self.indexes = ["i", "j", "k", "ii", "jj", "kk"]
        self.current_index = 0
        self.generated = {}
        self.vars = []
        self.has_error_check = False
        self.options = {}
        self.function_helper_writer = None

    def set_option(self, opt, value = True):
        self.options[opt] = value

    def has_option(self, opt):
        return self.options.has_key(opt)

    def set_is_generated(self, kind, name):
        if not self.generated.has_key(kind):
            v = {}
            self.generated[kind] = v
        else:
            v = self.generated[kind]
        v[name] = 1

    def is_generated(self, kind, name):
        if not self.generated.has_key(kind):
            return False
        v = self.generated[kind]
        return v.has_key(name)

    def getvalue(self):
        strs = map(lambda writer: writer.getvalue(), self.contents)
        return "".join(strs)

    def get_subwriter(self):
        writer = CodeWriter()
        self.contents.append(writer)
        self.out = StringIO()
        self.contents.append(self.out)
        writer.indentation = self.indentation
        writer.at_line_start = self.at_line_start
        writer.generated = self.generated
        writer.options = self.options
        writer.public_prefix = self.public_prefix

        return writer

    def write(self, s):
        # Ensure its a string
        s = str(s)

        if len(s) == 0:
            return

        if self.at_line_start:
            for i in range(self.indentation):
                self.out.write(" ")
            self.at_line_start = False
        self.out.write(s)
        return self

    def newline(self):
        self.out.write("\n")
        self.at_line_start = True
        return self

    def writeln(self, s):
        self.write(s)
        self.newline()
        return self

    def label(self, s):
        self.indentation = self.indentation - 1
        self.write(s + ":")
        self.indentation = self.indentation + 1
        self.newline()

    def statement(self, s):
        self.write(s)
        self.write(";")
        self.newline()
        return self

    def assign(self, var, val):
        self.write("%s = %s" % (var, val))
        self.write(";")
        self.newline()
        return self

    def increment(self, var, val):
        self.write("%s += %s" % (var, val))
        self.write(";")
        self.newline()
        return self

    def comment(self, str):
        self.write("/* " + str + " */")
        return self

    def todo(self, str):
        self.comment("TODO: *** %s ***" % str).newline()
        return self

    def error_check(self, check, label = "error"):
        self.has_error_check = True
        with self.block("if (SPICE_UNLIKELY(%s))" % check):
            if self.has_option("print_error"):
                self.statement('printf("%%s: Caught error - %s", __PRETTY_FUNCTION__)' % check)
            if self.has_option("assert_on_error"):
                self.statement("assert(0)")
            self.statement("goto %s" % label)

    def indent(self):
        self.indentation += 4

    def unindent(self):
        self.indentation -= 4
        if self.indentation < 0:
            self.indenttation = 0

    def begin_block(self, prefix= "", comment = ""):
        if len(prefix) > 0:
            self.write(prefix)
        if self.at_line_start:
            self.write("{")
        else:
            self.write(" {")
        if len(comment) > 0:
            self.write(" ")
            self.comment(comment)
        self.newline()
        self.indent()

    def end_block(self, semicolon=False, newline=True):
        self.unindent()
        if self.at_line_start:
            self.write("}")
        else:
            self.write(" }")
        if semicolon:
            self.write(";")
        if newline:
            self.newline()

    class Block:
        def __init__(self, writer, semicolon, newline):
            self.writer = writer
            self.semicolon = semicolon
            self.newline = newline

        def __enter__(self):
            return self.writer.get_subwriter()

        def __exit__(self, exc_type, exc_value, traceback):
            self.writer.end_block(self.semicolon, self.newline)

    class PartialBlock:
        def __init__(self, writer, scope, semicolon, newline):
            self.writer = writer
            self.scope = scope
            self.semicolon = semicolon
            self.newline = newline

        def __enter__(self):
            return self.scope

        def __exit__(self, exc_type, exc_value, traceback):
            self.writer.end_block(self.semicolon, self.newline)

    class NoBlock:
        def __init__(self, scope):
            self.scope = scope

        def __enter__(self):
            return self.scope

        def __exit__(self, exc_type, exc_value, traceback):
            pass

    def block(self, prefix= "", comment = "", semicolon=False, newline=True):
        self.begin_block(prefix, comment)
        return self.Block(self, semicolon, newline)

    def partial_block(self, scope, semicolon=False, newline=True):
        return self.PartialBlock(self, scope, semicolon, newline)

    def no_block(self, scope):
        return self.NoBlock(scope)

    def optional_block(self, scope):
        if scope != None:
            return self.NoBlock(scope)
        return self.block()

    def for_loop(self, index, limit):
        return self.block("for (%s = 0; %s < %s; %s++)" % (index, index, limit, index))

    def while_loop(self, expr):
        return self.block("while (%s)" % (expr))

    def if_block(self, check, elseif=False, newline=True):
        s = "if (%s)" % (check)
        if elseif:
            s = " else " + s
        self.begin_block(s, "")
        return self.Block(self, False, newline)

    def variable_defined(self, name):
        for n in self.vars:
            if n == name:
                return True
        return False

    def variable_def(self, ctype, *names):
        for n in names:
            # Strip away initialization
            i = n.find("=")
            if i != -1:
                n = n[0:i]
            self.vars.append(n.strip())
        # only add space for non-pointer types
        if ctype[-1] == "*":
            ctype = ctype[:-1].rstrip()
            self.writeln("%s *%s;"%(ctype, ", *".join(names)))
        else:
            self.writeln("%s %s;"%(ctype, ", ".join(names)))
        return self

    def function_helper(self):
        if self.function_helper_writer != None:
            writer = self.function_helper_writer.get_subwriter()
            self.function_helper_writer.newline()
        else:
            writer = self.get_subwriter()
        return writer

    def function(self, name, return_type, args, static = False):
        self.has_error_check = False
        self.function_helper_writer = self.get_subwriter()
        if static:
            self.write("static ")
        self.write(return_type)
        self.write(" %s(%s)"% (name, args)).newline()
        self.begin_block()
        self.function_variables_writer = self.get_subwriter()
        self.function_variables = {}
        return self.function_variables_writer

    def macro(self, name, args, define):
        self.write("#define %s(%s) %s" % (name, args, define)).newline()

    def ifdef(self, name):
        indentation = self.indentation
        self.indentation = 0;
        self.write("#ifdef %s" % (name)).newline()
        self.indentation = indentation

    def ifdef_else(self, name):
        indentation = self.indentation
        self.indentation = 0;
        self.write("#else /* %s */" % (name)).newline()
        self.indentation = indentation

    def endif(self, name):
        indentation = self.indentation
        self.indentation = 0;
        self.write("#endif /* %s */" % (name)).newline()
        self.indentation = indentation

    def add_function_variable(self, ctype, name):
        if self.function_variables.has_key(name):
            assert(self.function_variables[name] == ctype)
        else:
            self.function_variables[name] = ctype
            self.function_variables_writer.variable_def(ctype, name)

    def pop_index(self):
        index = self.indexes[self.current_index]
        self.current_index = self.current_index + 1
        self.add_function_variable("uint32_t", index)
        return index

    def push_index(self):
        self.current_index = self.current_index - 1

    class Index:
        def __init__(self, writer, val):
            self.writer = writer
            self.val = val

        def __enter__(self):
            return self.val

        def __exit__(self, exc_type, exc_value, traceback):
            self.writer.push_index()

    def index(self, no_block = False):
        if no_block:
            return self.no_block(None)
        val = self.pop_index()
        return self.Index(self, val)
