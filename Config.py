
def standard_header(a): return "<%s>" % a

def normal_header(a):
    return "\"%s\"" % a

def include_all(include_list):
    res = ""

    for i in include_list:
        res += "#include %s\n" %i

    return res

class Config:
    def __init__(self):
        self.output_path = ""
        self.include_path = "include/"
        self.src_path = "src/"
        self.template_path = "template/"
        self.parser_path = "parser/"

        self.flex_headerfile = "%sflex_lexer.h" % self.output_path
        self.flex_cpp_file = "%sflex_lexer.cpp" % self.output_path
        self.api_prefix = "ff_"
        self.token_prefix = "SQL_"

        self.bison_output_path = "%sbison.y" % self.parser_path
        self.flex_output_path = "%sflex.l" % self.parser_path
        self.bison_cpp_file = "%sbison_parser.cpp" % self.output_path
        self.bison_headerfile = "%sbison_parser.h" % self.output_path


        self.bison_parser_typedef_template_path = "%sparser_typedef_template.h" % self.template_path
        self.bison_parser_typedef_output_path = "%sparser_typedef.h" % self.parser_path

        # Flex
        self.flex_include_files = [standard_header("stdio.h"), standard_header("sstream"), standard_header("string"), standard_header("cstring"), normal_header(self.bison_headerfile), normal_header("parser_utils.h")]

        self.flex_declarations = []
        self.flex_declarations.append("static thread_local std::stringstream strbuf;")
        #self.flex_declarations.append("char* substr(const char* source, int from, int to);")

        self.flex_option_list = ["reentrant", "bison-bridge", "never-interactive", "batch", "noyywrap", "warn", "bison-locations"]
        #self.flex_option_list.append("case-insensitive")
        self.flex_option_list.append("header-file=\"%s\"" % self.flex_headerfile)
        self.flex_option_list.append("outfile=\"%s\"" % self.flex_cpp_file)
        self.flex_option_list.append("prefix=\"%s\"" % self.api_prefix)

        self.flex_extra_option = []

        self.flex_extra_option.append("%s COMMENT")
        self.flex_extra_option.append("%x singlequotedstring")
        # Bison
        self.bison_top_input_type = "Program"
        #self.ast_definition_path = "ast.h"

        self.bison_include_files = [standard_header("stdio.h"), standard_header("string.h"), normal_header(self.bison_headerfile), normal_header(self.flex_headerfile)]

        self.bison_declarations = []

        self.bison_declarations.append("int yyerror(YYLTYPE* llocp, %s * result, yyscan_t scanner, const char *msg) { return 0; }" % self.bison_top_input_type)

        self.bison_code_require_include_files = [normal_header("../include/ast.h"), normal_header("parser_typedef.h")]

        self.bison_code_require_defines = dict()
        self.bison_code_require_defines["api.pure"] = "full"
        self.bison_code_require_defines["api.prefix"] = "{%s}" % self.api_prefix
        self.bison_code_require_defines["api.token.prefix"] = "{%s}" % self.token_prefix
        self.bison_code_require_defines["parse.error"] = "verbose"

        self.bison_code_require_extra_defines = []
        self.bison_code_require_extra_defines.append('%locations')
        self.bison_code_require_extra_defines.append("""
%initial-action {
    // Initialize
    @$.first_column = 0;
    @$.last_column = 0;
    @$.first_line = 0;
    @$.last_line = 0;
    @$.total_column = 0;
    @$.string_length = 0;
};""")

        self.bison_code_require_lex_params = dict()
        self.bison_code_require_lex_params["scanner"] = "yyscan_t"

        self.bison_code_require_parse_params = dict()
        self.bison_code_require_parse_params["result"] = "%s*" % self.bison_top_input_type
        self.bison_code_require_parse_params["scanner"] = "yyscan_t"

        self.bison_data_type_prefix = "FF"

        ## header
        self.define_header_template_path = "%sdefine_header_template.h" % (self.template_path)
        self.define_header_output_path = "%sdefine.h" % self.include_path

        self.ast_header_template_path = "%sast_header_template.h" % (self.template_path)

        self.ast_src_template_path = "%sast_src_template.cpp" % (self.template_path)

        self.ast_src_output_path = "%s/ast.cpp" % (self.src_path)
        self.ast_header_output_path = "%s/ast.h" % (self.include_path)

        self.mutate_header_template_path = "%smutate_header_template.h" % (self.template_path)
        self.mutate_src_template_path = "%smutate_src_template.cpp" % (self.template_path)

        self.mutate_src_output_path = "%s/mutate.cpp" % (self.src_path)
        self.mutate_header_output_path = "%s/mutate.h" % (self.include_path)
        ## Others

        self.parser_utils_header_template_path =  "%sparser_utils_header_template.h" % (self.template_path)
        self.parser_utils_header_output_path =  "%sparser_utils.h" % (self.parser_path)
        self.utils_header_template_path =  "%sutils_header_template.h" % (self.template_path)
        self.utils_src_template_path =  "%sutils_src_template.cpp" % (self.template_path)

        self.utils_src_output_path = "%s/utils.cpp" % (self.src_path)
        self.utils_header_output_path = "%s/utils.h" % (self.include_path)

        # Variable Definition
        self.vardef_header_template_path =  "%svar_definition_header_template.h" % (self.template_path)
        self.vardef_src_template_path =  "%svar_definition_src_template.cpp" % (self.template_path)

        self.vardef_src_output_path = "%s/var_definition.cpp" % (self.src_path)
        self.vardef_header_output_path = "%s/var_definition.h" % (self.include_path)

        # Type system
        self.ts_header_template_path =  "%stypesystem_header_template.h" % (self.template_path)
        self.ts_src_template_path =  "%stypesystem_src_template.cpp" % (self.template_path)

        self.ts_src_output_path = "%s/typesystem.cpp" % (self.src_path)
        self.ts_header_output_path = "%s/typesystem.h" % (self.include_path)

        self.case_num = 400

    def gen_flex_definition(self):
        res = ""

        res += include_all(self.flex_include_files)

        #res += "#define TOKEN(name) { cout <<\"get token:\" <<#name << endl; return %s##name; }\n" %  self.token_prefix
        res += "#define TOKEN(name) { return %s##name; }\n" %  self.token_prefix

        for i in self.flex_declarations:
            res += i + "\n"

        return "%%{\n%s%%}\n" % res

    def gen_flex_options(self):
        res = ""
        for i in self.flex_option_list:
            res += "%%option %s\n" % i

        for i in self.flex_extra_option:
            res += "%s\n" % i

        return res

    def gen_bison_definition(self):
        res = ""

        res += include_all(self.bison_include_files)

        for i in self.bison_declarations:
            res += i + "\n"

        return "%%{\n%s%%}\n" % res

    def gen_bison_code_require(self):
        res = ""
        for i in self.bison_code_require_include_files:
            res += "#include %s\n" % i
        res += "#define YYDEBUG 1"
        return "%%code requires {\n%s}\n" % res

    def gen_bison_params(self):
        res = ""
        for (key,value) in self.bison_code_require_defines.items():
            res += "%%define %s\t%s\n" % (key,value)

        for i in self.bison_code_require_extra_defines:
            res += "%s\n" % i

        for (key,value) in self.bison_code_require_lex_params.items():
            res += "%%lex-param { %s %s }\n" % (value, key)

        for (key,value) in self.bison_code_require_parse_params.items():
            res += "%%parse-param { %s %s }\n" % (value, key)

        return res

    def gen_parser_typedef(self):
        with open(self.bison_parser_typedef_template_path, "rb") as f:
            contents = f.read()
            contents = contents.replace("REPLACEME", self.bison_data_type_prefix)
            with open(self.bison_parser_typedef_output_path, 'wb') as ff:
                ff.write(contents)

configuration = Config()
