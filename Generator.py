# FuzzerFactory
import re
import argparse
import os
import json
import copy

from Config import *
#from InputGeneration import *
from Utils import *

# Grammar Format
"""
select_statement:
    SELECT column_name FROM table_name {xxxxx}
    SELECT column_list FROM table_name JOIN table_name {xxxxxx}
-------------------------------------------------------------
create_statement:
    CREATE xxx

-------------------------------------------------------------

"""
allClass = list()

tokenDict = dict()
#notKeywordSym = dict()
literalValue = dict() #{"float":["float_literal"], "int":["int_literal"], "string":["identifier", "string_literal"]}
destructorDict = dict()
gDataFlagMap = dict()
gDataFlagMap["Define"] = 1
gDataFlagMap["Undefine"] = 2
gDataFlagMap["Global"] = 4
gDataFlagMap["Use"] = 8
gDataFlagMap["MapToClosestOne"] = 0x10
gDataFlagMap["MapToAll"] = 0x20
gDataFlagMap["Replace"] = 0x40
gDataFlagMap["Alias"] = 0x80
gDataFlagMap["NoSplit"] = 0x100
gDataFlagMap["ClassDefine"] = 0x200
gDataFlagMap["FunctionDefine"] = 0x400
gDataFlagMap["Insertable"] = 0x800
#gDataFlagMap["VarDefine"] = 0x800
#gDataFlagMap["VarType"] = 0x1000
#gDataFlagMap["VarName"] = 0x2000

semanticRule = None

def is_specific(classname):
    '''
        test if this class need specific handling
        return [bool, type]
        e.g. [Flase, ""] or [True, "string"]
    '''
    classname = hump_to_underline(classname)
    global literalValue
    flag = False
    for k, v in literalValue.items():
        for cname in v:
            if(cname == classname):
                return [True, k]

    return [False, ""]


def expect():
    id = [0]
    def func():
        id[0] += 1
        return 'v' + str(id[0])
    return func


'''
DataTypeRule:
    path: list[] of string
    datatype: string
Token:
    token_name: string
    prec : int
    association: string
    match_string: string
    is_keyword: bool

Symbol: class
    name: string
    isTerminator: bool
    isId: bool

Case: class
    caseidx: int
    symbolList:
    idTypeList:

GenClass: class
    name: string
    memberList : dict # A dictionary of this class's members
    caseList : list of case  # A list of each case of this class
'''

class DataTypeRule:
    def __init__(self, path, datatype, scope, dataflag):
        self.path = path
        self.datatype = datatype
        self.scope = int(scope)
        flag = 0
        for i in dataflag:
            flag |= gDataFlagMap[i]
        self.dataflag = flag

class Token:
    def __init__(self, token_name, prec, assoc, match_string, ttype):
        self.token_name = token_name
        self.prec = prec
        self.assoc = assoc
        self.ttype = ttype
        self.match_string = match_string
        self.is_keyword = (token_name == match_string)

class Symbol:
    def __init__(self, name, isTerminator = False, isId = False):
        self.name = name
        self.isTerminator = isTerminator
        self.isId = isId
        self.analyze()

    def analyze(self):
        token = self.analyze_item(self.name)
        if(token == "_COMMA_" or token == "_QUOTA_" or token == "_LD_" or token == "_RD_" or "_QUEAL_"):
            self.name = self.name.replace("\'", "")
        if(self.name.isupper()):
            self.isTerminator = True
        if(token == "_IDENTIFIER_"):
            #self.isTerminator = False
            self.isId = True

    def analyze_item(self, i):
        if i == "IDENTIFIER":
            return "_IDENTIFIER_"
        elif i == "EMPTY":
            return "_EMPTY_"
        elif i == "'='":
            return "_EQUAL_"
        elif i == "','":
            return "_COMMA_"
        elif i == "';'":
            return "_QUOTA_"
        elif i == "'('":
            return "_LD_"
        elif i == "')'":
            return "_RD_"
        elif i.isupper():
            return "_KEYWORD_"
        else:
            return "_VAR_"

    def __str__(self):
        return "name:%s, isTerminator:%s, isId:%s" % (self.name, self.isTerminator, self.isId)

class Case:
    def __init__(self, caseidx, isEmpty, prec, data_type_to_replace, expected_value_generator=expect()):
        self.caseidx = caseidx
        self.symbolList = list()
        self.idTypeList = list()
        self.isEmpty = isEmpty
        self.prec = prec
        self.datatype_rules = self.parse_datatype_rules(data_type_to_replace)

    def parse_datatype_rules(self, s):
        res = []
        if s == None:
            return res

        s = s.split(";")
        for i in s:
            #print(i)
            if i.strip() == "":
                continue
            i = i.split("=")
            path = i[0].strip().split("->")
            prop = i[1].strip().split(":")
            res.append(DataTypeRule(path, prop[0], prop[1], prop[2:]))

        return res

    def gen_member_list(self):
        member_list = {}
        member_list.clear()

        #if empty, we won't add anything into memberlist
        if(self.isEmpty == True):
            return member_list

        for s in self.symbolList:
            if s.isTerminator == True:
                continue
            if(s.name in member_list):
                member_list[s.name] += 1
            else:
                member_list[s.name] = 1
        return member_list

    def __str__(self):
        symbol = ""
        for i in self.symbolList:
            symbol += "(" + str(i) + ")" + ","
        symbol = symbol[:-1]
        res = "caseidx:%d, SymbolList:(%s), isEmpty:%s, prec: %s\n" % (self.caseidx, symbol, self.isEmpty, self.prec)
        return res


class Genclass:
    def __init__(self, name, memberList = None, caseList = None):
        if(memberList == None):
            memberList = dict()
        if(caseList == None):
            caseList = list()

        self.name = underline_to_hump(name)
        self.memberList = memberList
        self.caseList = list()
        self.isTerminatable = False

    def merge_member_list(self, current_member_list):
        for m in current_member_list:
            if(m in self.memberList and current_member_list[m]>self.memberList[m]):
                self.memberList[m] = current_member_list[m]
            elif(m not in self.memberList):
                self.memberList[m] = current_member_list[m]

    def __str__(self):
        name = self.name
        members = "("
        for k, v in self.memberList.items():
            members += k + ":" + str(v) + ","
        members = members[:-1] + ")"
        case = "("
        for i in self.caseList:
            case += str(i) + ","
        case = case[:-1] + ")"

        return "Genclass:[name:%s,\n memberList:%s,\n caseList:%s]" %(name, members, case)


def handle_datatype(vvlist, data_type, scope, dataflag, gen_name, tnum = 0):
    tab = "\t"
    vlist = [i[0] for i in vvlist]
    if(len(vlist) == 1):

        if('fixme' in vlist[0]):
            vlist[0] = '$$'

        res = tab*tnum + "if(%s){\n" % vlist[0]
        res += tab*(tnum+1) + "%s->data_type_ = %s; \n" %(vlist[0], data_type)
        res += tab*(tnum+1) + "%s->scope_ = %d; \n" %(vlist[0], scope)
        res += tab*(tnum+1) + "%s->data_flag_ =(DATAFLAG)%d; \n" %(vlist[0], dataflag)
        res += tab*tnum + "}\n"
        return res
    if(is_list(vvlist[0][1]) == False):
        res = ""
        if(len(vlist) == 2 and vlist[1] == "fixme"):
            res += handle_datatype(vvlist[1:], data_type, scope, dataflag, gen_name, tnum)
        else:
            tmp_name = gen_name()
            res = tab*tnum + "if(%s){\n" % vlist[0]
            res += tab*(tnum+1) + "auto %s = %s->%s_; \n" %(tmp_name, vlist[0], vlist[1])
            tmp_list = vvlist[1:]
            tmp_list[0][0] = tmp_name
            res += handle_datatype(tmp_list, data_type, scope, dataflag, gen_name, tnum+1)
            res += tab*tnum + "}\n"
        return res
    else:
        tmp_name = gen_name()
        res = tab*(tnum+1) + "auto %s = %s->%s_; \n" %(tmp_name, vlist[0], vlist[1])
        tmp_list = vvlist[1:]
        tmp_list[0][0] = tmp_name
        res += handle_datatype(tmp_list, data_type, scope, dataflag, gen_name, tnum + 1)

        loop =  tab*tnum + "while(%s){\n" % vlist[0]
        loop += res
        loop += tab*(tnum+1) + "%s = %s->%s_;\n" %(vvlist[0][0], vvlist[0][0], vvlist[0][1])
        loop +=  tab*tnum + "}\n"

        return loop
    return res

def genDataTypeOneRule(datatype_rule):
    vvlist = [[i, i] for i in datatype_rule.path]
    vvlist = [["$$", "$$"]] + vvlist
    datatype = datatype_rule.datatype
    scope = datatype_rule.scope
    dataflag = datatype_rule.dataflag
    return handle_datatype(vvlist, datatype, scope, dataflag,  gen_tmp(), 2)


def parseGrammar(grammarDescription):

    content = grammarDescription.split("\n")
    res = Genclass(content[0].strip(":\n"))
    #res.memberList = dict()
    #print "*************"
    #print content[0].strip(":\n")
    #print res #this is wrong
    #print "*************"
    cases = content[1:]
    cases = [i.lstrip() for i in cases]
    caseIndex = 0

    for c in cases:
        ptn = re.compile("\*\*.*\*\*")
        data_type_to_replace = re.search(ptn, c) #ptn.findall(c)
        if(data_type_to_replace != None):
            #print("here1")
            #print data_type_to_replace.group()
            data_type_to_replace = data_type_to_replace.group()[2: -2]
            c = re.sub(ptn, '', c)

        #print c
        tmp_c = (c + "%") .split("%")
        c = tmp_c[0].strip()
        if(tmp_c[1] != ''):
            tmp_c[1] = "%" + tmp_c[1]
        case = Case(caseIndex, len(c)==0, tmp_c[1], data_type_to_replace)
        caseIndex += 1
        symbols = c.split(" ")
        case.symbolList = [Symbol(s) for s in symbols]
        #print(symbols)
        current_member_list = case.gen_member_list()
        res.merge_member_list(current_member_list)
        res.caseList.append(case)

    global allClass
    allClass.append(res)

    return res


def is_list(class_name):
    global allClass
    #print class_name#list
    class_name = underline_to_hump(class_name)

    for i in allClass:
        if(class_name == i.name):
            for member in i.memberList.keys():
                if(underline_to_hump(member) == class_name):
                    return True

            return False

    return False


def genClassDef(current_class, parent_class):
    '''
        Input:
            s : class

        Output:
            definition of this class in cpp: string
    '''
    res = "class "
    res += current_class.name + ":"
    res += "public " + "Node "+ "{\n"
    res += "public:\n" + "\tvirtual void deep_delete();\n"
    res += "\tvirtual IR* translate(vector<IR*> &v_ir_collector);\n"
    res += "\tvirtual void generate();\n"
    #res += "\t" + current_class.name + "(){type_ = k%s; cout << \" Construct node: %s \\n\" ;}\n" % (current_class.name, current_class.name)
    #res += "\t~" + current_class.name + "(){cout << \" Destruct node: %s \\n\" ;}\n" % (current_class.name)

    #res += "\t" + current_class.name + "(){}\n"
    #res += "\t~" + current_class.name + "(){}\n"

    res += "\n"

    #Handle classes which need specific variable
    specific = is_specific(current_class.name)
    if(specific[0]):
        res += "\t" + specific[1] + " " + specific[1] + "_val_;\n"
        res += "};\n\n"
        return res

    for member_name, count in current_class.memberList.items():
        if(member_name == ""):
            continue
        if(count == 1):
            res += "\t" + underline_to_hump(member_name) + " * " + member_name + "_;\n"
        else:
            for idx in range(count):
                res += "\t" + underline_to_hump(member_name) + " * " + member_name + "_" + str(idx+1) + "_;\n"
    res += "};\n\n"

    return res



def genDeepDelete(current_class):

    res = "void " + current_class.name + "::deep_delete(){\n"
    for member_name, count in current_class.memberList.items():
        if(count == 1):
            res += "\t" + "SAFEDELETE(" + member_name + "_);\n"
        else:
            for idx in range(count):
                res += "\t" + "SAFEDELETE(" + member_name + "_" + str(idx+1) + "_);\n"

    res += "\tdelete this;\n"
    res += "};\n\n"

    return res


def translateOneMember(member_name, variable_name):
    """
        Input:
            MemberName: string
            VariableName: string

        Output:
            a translate call string
            no \t at the front

    """
    return "auto " + variable_name + "= SAFETRANSLATE(" + member_name + ");\n"


def findOperator(symbol_list):
    res = ""
    idx = 0
    while(idx < len(symbol_list)):
        if(symbol_list[idx].isTerminator == True):
            res += get_match_string(symbol_list[idx].name) + " "
            symbol_list.remove(symbol_list[idx])
            idx -= 1
        else:
            res = res[:-1]
            break

        idx += 1

    return res.strip()

def get_match_string(token):
    #print(token)
    if tokenDict.has_key(token) and tokenDict[token].is_keyword == False:
        return strip_quote(tokenDict[token].match_string)
    return token

def findLastOperator(symbol_list):
    res = ""
    idx = 0
    if(idx < len(symbol_list)):
        if(symbol_list[idx].isTerminator == True):
            res += get_match_string(symbol_list[idx].name)
            symbol_list.remove(symbol_list[idx])

    is_case_end = True
    for i in range(idx, len(symbol_list)):
        if (symbol_list[i].isTerminator == False):
            is_case_end = False
            break
    if(is_case_end == True):
        i = 0
        while(i < len(symbol_list)):
            res += " " + get_match_string(symbol_list[i].name)
            symbol_list.remove(symbol_list[i])

    res = res.strip()

    return res

def findOperand(symbol_list):
    res = ""
    idx = 0
    if(idx < len(symbol_list)):
        if(symbol_list[idx].isTerminator == False):
            res = symbol_list[idx].name
            symbol_list.remove(symbol_list[idx])
        else:
            print("[*]error occur in findOperand")
            exit(0)

    return res.strip()


def strip_quote(s):
    if(s[0] == '"' and s[-1] == '"'):
        return s[1:-1]
    return s

def collectOpFromCase(symbol_list):
    """
        Input:
            case: list
            member_list: dict

        Output:
            (operands, operators): tuple(list, list)
    """

    operands = list()
    operators = list()

    operators.append(findOperator(symbol_list))
    operands.append(findOperand(symbol_list))
    operators.append(findOperator(symbol_list))
    operands.append(findOperand(symbol_list))
    operators.append(findLastOperator(symbol_list))

    res = ([i for i in operands if i != ""], [i for i in operators])


    return res


def translateOneIR(ir_argument, classname, irname):
    """
    Input:
        0-2 operands, 0-3 operators, classname

    Output:
        (irname, irstring): tuple(string, string)
    """
    operands = ir_argument[0]
    operators = ir_argument[1]

    for i in range(3):
        operators[i] = '"' + operators[i] + '"'

    res_operand = ""

    for operand in operands:
        res_operand += ", " + operand

    res_operator = "OP3(" + operators[0] +"," + operators[1] + "," + operators[2] + ")"
    #operand_num = len(operands)

    if irname != "res":
        irname  = "auto " + irname

    res =  irname + " = new IR(" + "k" + underline_to_hump(classname) + ", " + res_operator + res_operand +");\n"
    return res


def gen_tmp():
    count = [0]
    def get_tmp():
        count[0] += 1
        return "tmp" + str(count[0])
    return get_tmp


def transalteOneCase(current_class, case_idx):
    current_case = current_class.caseList[case_idx]
    member_list = current_class.memberList
    symbols = [i for i in current_case.symbolList]
    process_queue = list()
    name_idx_dict = dict()
    res = ""

    """
    Handle Empty Case Here
    """
    if current_case.isEmpty == True:
        return "\t\tres = new IR(k" + underline_to_hump(current_class.name) +", string(\"\"));\n"

    for name, count in member_list.items():
        if(count > 1):
            name_idx_dict[name] = 1

    get_tmp = gen_tmp()
    for symbol in symbols:
        if(symbol.isTerminator == False):
            tmp_name = get_tmp()
            new_symbol = Symbol(tmp_name, False, symbol.isId)
            process_queue.append(new_symbol)
            cur_symbol_real_name = ""
            if(symbol.name in name_idx_dict.keys()):
                cur_symbol_real_name = symbol.name + "_" + str(name_idx_dict[symbol.name]) + "_"
                name_idx_dict[symbol.name] += 1
            else:
                cur_symbol_real_name = symbol.name + "_"
            res += "\t\t" + translateOneMember(cur_symbol_real_name, tmp_name)
        else:
            process_queue.append(symbol)

    while(len(process_queue) != 0):
        ir_info = collectOpFromCase(process_queue)
        if(len(process_queue) == 0):
            res += "\t\t" + translateOneIR(ir_info, current_class.name, "res")
        else:
            tmp_name = get_tmp()
            res += "\t\t" + translateOneIR(ir_info, "Unknown", tmp_name)
            res += "\t\tPUSH(" + tmp_name + ");\n"
            new_symbol = Symbol(tmp_name, False, False)
            process_queue = [new_symbol] + process_queue


    return res

def genGenerateOneCase(current_class, case_idx):

    res = ""
    case_list = current_class.caseList[case_idx]

    if case_list.isEmpty == True:
        return "\n"

    counter = dict()

    for key in current_class.memberList.keys():
        counter[key] = 1

    for sym in case_list.symbolList:
        if sym.isTerminator:
            continue
        membername = sym.name+"_"
        if(current_class.memberList[sym.name] > 1):
            membername = "%s_%d_" % (sym.name, counter[sym.name])
            counter[sym.name] += 1

        #ddprint(sym.name)
        res += "\t\t%s = new %s();\n" %(membername, underline_to_hump(sym.name))
        res += "\t\t%s->generate();\n" % membername

    return res

def getNotSelfContainCases(current_class):
    #print(current_class.name)
    mytest_list = {"SimpleSelect":[0, 1, 2, 3], "CExpr": [0], "AExpr": [0]}
    if(mytest_list.has_key(current_class.name)):
        return mytest_list[current_class.name]
    case_idx_list = []
    class_name = hump_to_underline(current_class.name)
    for case in current_class.caseList:
        flag = False
        for i in case.symbolList:
            if i.name == class_name:
                flag = True
                break
        if flag == False:
            case_idx_list.append(case.caseidx)

    return case_idx_list

def genGenerate(current_class):

    case_nums = len(current_class.caseList)
    has_switch = case_nums != 1
    class_name = underline_to_hump(current_class.name)

    res =  "void " + class_name + "::generate(){\n"
    not_self_contained_list = getNotSelfContainCases(current_class)
    not_len = len(not_self_contained_list)
    #print(class_name)
    #print(not_self_contained_list)
    has_default = True
    if(case_nums == 0):
        print class_name
    if(not_len / case_nums > 0.8):
        has_default = False

    if has_default == False:
        res += "\tGENERATESTART(%d)\n\n" %(case_nums)
    else:
        res += "\tGENERATESTART(%d)\n\n" %(case_nums * 100)

    #Handling class need specific variable like identifier
    specific = is_specific(current_class.name)
    if(specific[0]):
        res += "\t\t%s_val_ = gen_%s();\n" % (specific[1], specific[1])
        res += "\n\tGENERATEEND\n}\n\n"
        return res

    if(has_switch):
        res += "\tSWITCHSTART\n"

    case_id = 0
    #for each_case in current_class.caseList:
    for i in range(len(current_class.caseList)):
        if(has_switch):
            res += "\t\tCASESTART(" + str(case_id) + ")\n"
            case_id += 1

        res += genGenerateOneCase(current_class, i)
        if(has_switch):
            res += "\t\tCASEEND\n"

    #if(has_switch):
    #    res += "\tSWITCHEND\n"

    tmplate_str = """
    default:{
        int tmp_case_idx = rand() %% %d;
        switch(tmp_case_idx){
            %s
        }
    }
}
"""

    if(has_switch and has_default == False):
        res += "\tSWITCHEND\n"
    elif(has_default):
        tmp_str = ""
        for i in range(not_len):
            tmp_str += "CASESTART(" + str(i) + ")\n"

            tmp_str += genGenerateOneCase(current_class, not_self_contained_list[i])

            tmp_str += "case_idx_ = %d;\n" % (not_self_contained_list[i])

            tmp_str += "CASEEND\n"

        res += tmplate_str % (not_len, tmp_str)


    res += "\n\tGENERATEEND\n}\n\n"
    return res

def genTranslateBegin(class_name):
    """
    dict(): class_name --> api_dict
    """
    global semanticRule

    ir_handlers = semanticRule["IRHandlers"]
    
    res = list()
    for item in ir_handlers:
        if item["IRType"] == class_name:
            for handler in item["PreHandler"]:
                res.append(handler["Function"] + "(" + ", ".join(handler["Args"]) + ");")

    return "\t" + "\n\t".join(res) + "\n"

def genTranslateEnd(class_name):
    """
    dict(): class_name --> api_dict
    """

    res = list()
    ir_handlers = semanticRule["IRHandlers"]
    for item in ir_handlers:
        if item["IRType"] == class_name:
            for handler in item["PostHandler"]:
                res.append(handler["Function"] + "(" + ", ".join(handler["Args"]) + ");")
    return "\t" + "\n\t".join(res) + "\n"



def genTranslate(current_class):
    """
        input:
            s
        output:
            translate def : stirng
    """
    has_switch = len(current_class.caseList) != 1
    class_name = underline_to_hump(current_class.name)

    res = "IR*  " + class_name + "::translate(vector<IR *> &v_ir_collector){\n"
    res += "\tTRANSLATESTART\n\n"

    res += genTranslateBegin(current_class.name)
    #Handling class need specific variable like identifier
    specific = is_specific(current_class.name)
    if(specific[0]):
        if "iteral" in class_name and specific[1] == 'string':
            res += "\t\tres = new IR(k" + current_class.name + ", " + specific[1] + "_val_, data_type_, scope_, data_flag_);\n"
        else:
            res += "\t\tres = new IR(k" + current_class.name + ", " + specific[1] + "_val_, data_type_, scope_, data_flag_);\n"
        res += genTranslateEnd(current_class.name)
        res += "\n\tTRANSLATEEND\n}\n\n"
        return res

    if(has_switch):
        res += "\tSWITCHSTART\n"

    case_id = 0
    #for each_case in current_class.caseList:
    for i in range(len(current_class.caseList)):
        #each_case = current_class.caseList[i]
        if(has_switch):
            res += "\t\tCASESTART(" + str(case_id) + ")\n"
            case_id += 1

        res += transalteOneCase(current_class, i)
        if(has_switch):
            res += "\t\tCASEEND\n"

    if(has_switch):
        res += "\tSWITCHEND\n"

    res += genTranslateEnd(current_class.name)
    res += "\n\tTRANSLATEEND\n}\n\n"
    return res


def genDataTypeEnum():
    res = "enum DATATYPE{"

    res += """
#define DECLARE_TYPE(v)  \\
    k##v,
ALLDATATYPE(DECLARE_TYPE)
#undef DECLARE_TYPE
"""
    res += "};\n"
    return res

def genClassTypeEnum():
    res = "enum NODETYPE{"

    res += """
#define DECLARE_TYPE(v)  \\
    v,
ALLTYPE(DECLARE_TYPE)
#undef DECLARE_TYPE
"""
    res += "};\n"

    res += "typedef NODETYPE IRTYPE;\n"
    return res

def genClassDeclaration(class_list):
    res = ""

    res += """
#define DECLARE_CLASS(v) \\
    class v ; \

ALLCLASS(DECLARE_CLASS);
#undef DECLARE_CLASS
"""
    return res

def genTokenForOneKeyword(keyword, token):
    return "%s\tTOKEN(%s)\n" % (keyword, token)

def genParserUtilsHeader(token_dict):
    with open(configuration.parser_utils_header_template_path, 'r') as f:
        content = f.read()

    replaced = ""
    for i in token_dict.values():
        replaced += "TOKEN_%s, \n" % i.token_name
    
    content = content.replace("__TOKEN_STATE__", replaced)
    
    return content

def genFlex(token_dict):
    res = ""

    res += configuration.gen_flex_definition()
    res += "\n"
    res += configuration.gen_flex_options()
    res += "\n"

    res += "%%\n"

    extra_rules_path = "./data/extra_flex_rule"
    if args.extraflex:
        extra_rules_path = args.extraflex
    contents = ""
    with open(extra_rules_path, 'rb') as f:
        contents = f.read()
        ban_ptn = re.compile(r"PREFIX_([A-Z_]*)")
        ban_res = ban_ptn.findall(contents)
        ban_list = set(ban_res)
        if(len(contents) > 2 and contents[-1] != '\n'):
            contents += '\n'

        contents = contents.split("+++++\n")
        contents = [i.split("-----\n") for i in contents]
        #print(contents)

    test = ""
    for i in contents:
        if(len(i) < 2):
            continue
        if len(i[1]) > 0 and i[1][0] == '[':
            ## Update match string here
            tmpp = copy.deepcopy(i)
            token_list = tmpp[1].split("\n")[0]
            i[1] = "\n".join(tmpp[1].split("\n")[1:])
            token_list = token_list[1:-1].split(", ")
            for dd in token_list:
                for token in token_dict.values():
                    if dd == token.token_name:
                        if token.match_string != tmpp[0].strip():
                            token.match_string = tmpp[0].strip()
                            token.is_keyword = False
                        break
            #print(token_list)
        test += "%s {\n\t%s}\n" % (i[0].strip(), "\n\t".join(i[1].replace("PREFIX_", configuration.token_prefix).split("\n")))
        #print(test)
        #raw_input()
        test += "\n"
    #print(test)
    #print(token_dict)
    for token in token_dict.values():
        #print(token.token_name)
        if(token.token_name in ban_list):
            continue
        res += genTokenForOneKeyword(token.match_string, token.token_name)

    res += test

    res += "%%\n"
    return res

def genBisonType(class_set):
    """
    input:
        set of GenClass : set

    Output:
        "%type <import_statement_t>	import_statement\nxxxx": string
    """
    res = ""
    for c in class_set:
        cname = c.name
        underline_name = hump_to_underline(cname)
        res += "%%type <%s_t>\t%s\n" % (underline_name, underline_name)

    return res

def genBisonPrec(token_dict):
    """
    input:
        token_dict: dict

    output:
        "%right xxx yyy": string
    """
    precdict = dict()
    for token_name, token in token_dict.items():
        if token.prec != 0:
            if(precdict.has_key(token.prec) == False):
                precdict[token.prec] = []
            precdict[token.prec].append(token_name)

    res = ""

    for (key, value) in precdict.items():
        tmp = ""
        tmp_name = ""
        for token_name in value:
            tmp += " %s" % token_name
        res += "%%%s %s\n" % (token_dict[value[0]].assoc, tmp)

    return res

def genBisonToken(token_dict):
    """
    input:
        token_dict: dict

    output:
        "%token xxx yyy": string
    """
    counter = 0
    token_res = ""

    type_token = ""
    for token_name in token_dict.keys():
        if(token_dict[token_name].ttype != "0"):
            type_token += "\n%%token <%s> %s\n" % (token_dict[token_name].ttype, token_name)
            continue
        if counter % 8 == 0:
            token_res += "\n%token"
        token_res += " " + token_name
        counter += 1



    token_res += type_token
    token_res += "\n"

    # This is hardcoded. To fix
    if "php" in args.input:
        token_res += "%token OP_DOLLAR PREC_ARROW_FUNCTION\n"
        token_res += "%token <sval> T_VARIABLE T_STRING_VARNAME\n"

    return token_res

def genBisonDataType(extra_data_type, allClass):

    res = ""
    for k, v in extra_data_type.items():
        res += "\t%s\t%s;\n" % (v, k)


    for c in allClass:
        underline_name = hump_to_underline(c.name)
        res += "\t%s\t%s;\n" % (c.name+" *", underline_name+"_t")

    return res


def genBisonTypeDefOneCase(gen_class, caseidx):
    case = gen_class.caseList[caseidx]
    symbol_name_list = []
    counter = dict()
    res = []
    if(gen_class.name == configuration.bison_top_input_type):
        res.append("$$ = result;\n")
        res.append("$$->case_idx_ = CASE%d;\n" % caseidx)
    else:
        res.append("$$ = new %s();\n" % (gen_class.name))
        res.append("$$->case_idx_ = CASE%d;\n" % caseidx)

    if(case.isEmpty == False):
        #tmp = "\t/* empty */ {\n \t\t%s \t}\n"
        for idx in range(len(case.symbolList)):
            sym = case.symbolList[idx]
            symbol_name_list.append(sym.name)
            if sym.isTerminator == True:
                continue
            tmp = "$$->%s_ = $%d;\n"
            if(counter.has_key(sym.name) == False):
                counter[sym.name] = 0
            counter[sym.name] += 1
            if(gen_class.memberList[sym.name] == 1):
                res.append(tmp % (sym.name, idx+1))
            else:
                res.append(tmp % (sym.name + "_%d" % counter[sym.name], idx+1))


        #Handling class which need specific variable
        specific = is_specific(gen_class.name)
        if(specific[0]):
            res = []
            res.append("$$ = new %s();\n" % (gen_class.name))
            res.append("$$->%s_val_ = $%d;\n" % (specific[1], 1))
            if specific[1] == 'string':
                res.append("free($%d);\n" % 1)



    data_type_rule = ""
    for dtr in case.datatype_rules:
        data_type_rule += genDataTypeOneRule(dtr) + "\n"

    res.append(data_type_rule[2:])
    if(gen_class.name == configuration.bison_top_input_type):
        res.append("$$ = NULL;\n")

    return "\t%s %s {\n\t\t%s\n\t}\n" % (" ".join(symbol_name_list), case.prec,  "\t\t".join(res))

def genBisonTypeDef(gen_class):
    res = hump_to_underline(gen_class.name) + ":\n"

    for i in range(len(gen_class.caseList)):
        res += genBisonTypeDefOneCase(gen_class, i)
        res += "   |"

    #res[-2] = ";"

    return res[0:-2] + ";\n"


def genBisonDestructor(destructorDict, extra_data_type):

    res = ""
    for dest_define, name in destructorDict.items():
        typename = ""
        for i in name:
            if(i in extra_data_type.keys()):
                typename += " <%s>" % (i)
            else:
                typename += " <%s_t>" % (i)
        if(dest_define == "EMPTY"):
            dest_define = " "
        res += "%destructor{\n\t" + dest_define + "\n} " + typename + "\n\n"

    res += "%destructor { if($$!=NULL)$$->deep_delete(); } <*>\n\n"
    return res

def genBison(allClass):
    global tokenDict
    global destructorDict

    configuration.gen_parser_typedef()

    res = ""
    res += configuration.gen_bison_definition()
    res += configuration.gen_bison_code_require()
    res += configuration.gen_bison_params()

    extra = {"fval":"double", "sval":"char*", "ival":"long"}
    res += "%%union %s_STYPE{\n%s}\n" % (configuration.bison_data_type_prefix, genBisonDataType(extra, allClass))

    res += genBisonToken(tokenDict)
    res += "\n"
    res += genBisonType(allClass)
    res += "\n"
    res += genBisonPrec(tokenDict)
    res += "\n"
    res += genBisonDestructor(destructorDict, extra)

    res += "%%\n"
    for i in allClass:
        res += genBisonTypeDef(i)
        res += "\n"
    res += "%%"
    return res

def genCaseEnum(casenum):
    res = "enum CASEIDX{\n\t"

    for i in range(casenum):
        res += "CASE%d, " % i
    res += "\n};\n"

    return res

def genDataFlag():
    res = "enum DATAFLAG {\n"
    helper = ""
    for (key, value) in gDataFlagMap.items():
        res += "\t k%s = 0x%x,\n" % (key, value)
        helper += "#define is%s(a) ((a) & k%s)\n" % (key, key)
    res += "};\n"

    res += helper
    return res

def genAstHeader(allClass):

    include_list = [standard_header("vector"), standard_header("string"), normal_header("define.h"), standard_header("iostream"), standard_header("map"),standard_header("memory"), standard_header("fstream")]
    res = """#ifndef __AST_H__\n#define __AST_H__\n"""

    for i in include_list:
        res += "#include %s\n" % i
    res += "using namespace std;\n"
    res +="\n"

    res += genClassTypeEnum()
    res += "\n"
    res += genCaseEnum(configuration.case_num)
    res += "\n"

    res += genDataTypeEnum()
    res += "\n"

    with open(configuration.ast_header_template_path, 'r') as f:
        content = f.read()
        res = content.replace("HEADER_BEGIN", res)


    res = res.replace("__ALLCLASSDECLARATION__", genClassDeclaration(allClass))
    res = res.replace("__ALLDATAFLAG__", genDataFlag())
    res += "\n"
    #res = res.replace("__TOPASTNODE__", configuration.bison_top_input_type)

    res += "\n"
    for each_class in allClass:
        res += genClassDef(each_class, each_class)

    res += "#endif\n"
    return res

def genAstSrc(allClass):

    include_list = [normal_header("../include/ast.h"), normal_header("../include/define.h"), normal_header("../include/utils.h"), standard_header("cassert"), normal_header("../include/var_definition.h"), normal_header("../include/typesystem.h")]
    res = ""

    res += include_all(include_list)
    with open(configuration.ast_src_template_path, 'r') as f:
        content = f.read()
        res = content.replace("SRC_BEGIN", res)

        res = res.replace("__TOSTRINGCASE__", genToStringCase())
        #res = res.replace("__TOPASTNODE__", configuration.bison_top_input_type)

    tmp_set = ["kDeclaration"]
    tmp_str = ""
    for i in tmp_set:
        tmp_str+= "\t" + "define_top_set.insert(" + i + ");\n"

    res = res.replace("__INIT_TOP_DEFINE_SET__", tmp_str)

    for each_class in allClass:
        res += genTranslate(each_class)
        res += genDeepDelete(each_class)
        res += genGenerate(each_class)
        res += "\n\n"

    return res

def genDefineHeader(all_class, all_datatype):
    global semanticRule

    content = ""
    with open(configuration.define_header_template_path, 'r') as f:
        content = f.read()

    content = content.replace("__INIT_FILE_DIR__", semanticRule["InitFileDir"])
    all_type = "#define ALLTYPE(V) \\\n"
    all_classes = "#define ALLCLASS(V) \\\n"
    all_data_types = "#define ALLDATATYPE(V) \\\n"

    for c in all_class:
        cname = c.name
        all_type += "\tV(k%s) \\\n" % cname
        all_classes += "\tV(%s) \\\n" % cname

    all_type += "\tV(kUnknown) \\\n"
    all_type += "\n"
    all_classes += "\n"

    for c in all_datatype:
        all_data_types += "\tV(%s) \\\n" % c

    all_data_types += "\n"

    content = content.replace("DEFINE_ALL_TYPE", all_type)
    content = content.replace("DEFINE_ALL_CLASS", all_classes)
    content = content.replace("DEFINE_ALL_DATATYPE", all_data_types)

    return content

def genUtilsHeader():
    content = ""
    with open(configuration.utils_header_template_path, 'r') as f:
        content = f.read()

    # content
    content = content.replace("__TOPASTNODE__",  configuration.bison_top_input_type)
    #print content
    return content

def genUtilsSrc():
    content = ""
    with open(configuration.utils_src_template_path, 'r') as f:
        content = f.read()

    content = content.replace("__TOPASTNODE__",  configuration.bison_top_input_type)
    content = content.replace("__API_PREFIX__", configuration.api_prefix)

    return content

def parseBasicType(filename):
    content = ""
    with open(filename, 'r') as f:
        content = f.readlines()

    content = [i.strip().split(",") for i in content]

    result = dict()

    for i in content:
        result[i[0]] = i[1:]

    return result

def genBasicTypeHandler(input):
    res = ""
    res += "int result_type = 0;\n"
    res += "\t\t\t" + "shared_ptr<map<int, vector<pair<int,int>>>> ptr;\n"
    for k, btypes in input.items():
        k = "k" + underline_to_hump(k)
        #res += "\t\t" + "case %s:{\n" % k
        res += "\t\t\t" + "ptr = make_shared<map<int, vector<pair<int,int>>>>();\n"
        for btype in btypes:
            res += "\t\t\t" + "result_type = get_basic_type_id_by_string(\"%s\");\n" % btype
            res += "\t\t\t" + "(*ptr)[result_type].push_back(make_pair(result_type, 0));\n"

        res += "\t\t\t" + "basic_types_cache[%s] = ptr;\n" % k
        #res += "\t\t\t" + "break;\n\t\t"
        #es += "}\n"

    return res

def genBasicTypeHandlerNew(input):
    res = ""
    for k, btypes in input.items():

        k = "k" + underline_to_hump(k)
        res += "\t\t" + "case %s:\n" % k
        for btype in btypes:
            res += "\t\t\t" + "res_type = get_type_id_by_string(\"%s\");\n" % btype
            res += "\t\t\t" + "(*cur_type)[res_type].push_back(make_pair(0, 0));\n"
            res += "\t\t\t" + "cache_inference_map_[cur] = cur_type;\n"
            res += "\t\t\t" + "return true;\n"

    return res

def parseFixBasicType(filename):
    content = ""
    with open(filename, 'r') as f:
        content = f.readlines()

    content = [i.strip().split(",") for i in content]
    res = dict()
    for i in content:
        if res.has_key(i[0]) == False:
            res[i[0]] = [[x for x in i if x!=i[0]]]
        else:
            res[i[0]].append([x for x in i if x!=i[0]])
    #print res
    return res

def genFixBasicType(input):
    """
    input-->dict of list, element: string_literal: [[INT, int_literal, string_to_int]]
    """
    res = ""
    for k, eachs in input.items():
        res += "\t\t\t" + "case k%s:\n" % underline_to_hump(k)
        for each in eachs:
            res += "\t\t\t\t" + "if(type == get_basic_type_id_by_string(\"%s\")) %s(cur, k%s);\n" % (each[0], each[2], underline_to_hump(each[1]))
        res += "\t\t\t\tbreak;\n"
        #res += "\n\t\t\t}\n"
    return res

def genConvertIRTypeMap():
    global allClass

    res = ""
    convert_map = build_convert_ir_type_map(allClass)
    for class_a in allClass:
        for class_b in allClass:
            type_a = underline_to_hump(class_a.name)
            type_b = underline_to_hump(class_b.name)
            if can_be_converted(type_a, type_b, convert_map) != None:
                res += "\t\tm_convertable_map_[k%s].insert(k%s);\n" % (type_a, type_b)

    return res

def genMutateHeader():
    content = ""
    with open(configuration.mutate_header_template_path, 'r') as f:
        content = f.read()

    return content

def genMutateSrc():
    content = ""
    with open(configuration.mutate_src_template_path, 'r') as f:
        content = f.read()

    convert_map = genConvertIRTypeMap()
    content = content.replace("__INIT_CONVERTABLE_TYPE_MAP__", convert_map)

    return content

def genVarDefSrc():
    content = ""
    with open(configuration.vardef_src_template_path, 'r') as f:
        content = f.read()

    global semanticRule
    convert_chain = ""

    if semanticRule["IsWeakType"]:
        content = content.replace("__IS_WEAK_TYPE__", "#define WEAK_TYPE")   
    else:
        content = content.replace("__IS_WEAK_TYPE__", "")

    for i in semanticRule["ConvertChain"]:
        for kk in range(0, len(i) - 1):
            convert_chain += "\t" + "v_convert.push_back(make_pair(\"%s\", \"%s\")); \n" % (i[kk], i[kk + 1])
    
    content = content.replace("__SEMANTIC_CONVERT_CHAIN__", convert_chain)

    basic_types = ["\"%s\""% i for i in semanticRule["BasicTypes"]]
    basic_types = ", ".join(basic_types)
    content = content.replace("__SEMANTIC_BASIC_TYPES__", basic_types)
    return content

def genVarDefHeader():
    with open(configuration.vardef_header_template_path, 'r') as f:
        content = f.read()

    return content

def genTypeSystemHeader():
    global semanticRule
    with open(configuration.ts_header_template_path, 'r') as f:
        content = f.read()

    if semanticRule["IsWeakType"] == 1:
        content = content.replace("__IS_WEAK_TYPE__", "#define WEAK_TYPE")   
    else:
        content = content.replace("__IS_WEAK_TYPE__", "")
    return content

def parserInferIDByDataType(filename):
    content = []
    with open(filename, 'r') as f:
        content = f.readlines()

    content = [i.strip().split(",") for i in content]
    return content

def genInferIDByDataType(infer_input):

    res = ""
    template = """
    result_type = get_basic_type_id_by_string("%s");
	(*ptr)[result_type].push_back(make_pair(result_type, 0));
    """
    for case in infer_input:
        datatype = case[0]
        res += "case k%s: {\n" % datatype
        for kk in case[1:]:
            res += template % kk

        res += "break;\n}\n"

    return res

def genTypeSystemSrc():
    global semanticRule
    with open(configuration.ts_src_template_path, 'r') as f:
        content = f.read()


    basic_unit = ",".join(semanticRule['BasicUnit'])
    content = content.replace("__SEMANTIC_BASIC_UNIT__", basic_unit)
    content = content.replace("__SEMANTIC_BUILTIN_OBJ__", "\"%s\"" % semanticRule["BuiltinObjFile"])

    if(semanticRule['IsWeakType'] == 0):
        content = content.replace("__FIX_IR_TYPE__", semanticRule["FixIRType"])
        content = content.replace("__FUNCTION_ARGUMENT_UNIT__", semanticRule["FunctionArgumentUnit"])

    op_rules = []
    for oprule in semanticRule['OPRule']:
        tmp = []
        tmp.append(str(oprule['OperandNum']))
        tmp += oprule['Operator']
        tmp.append(oprule['OperandLeftType'])
        if oprule['OperandNum'] == 2:
            tmp.append(oprule['OperandRightType'])
        tmp.append(oprule['ResultType'])
        tmp.append(oprule['InferDirection'])
        tmp += oprule['Property']
        op_rules.append('\"%s\"' % " # ".join(tmp))

    op_rules = ", ".join(op_rules)
    content = content.replace("__SEMANTIC_OP_RULE__", op_rules)
    btm = parseBasicType("./data/basic_type_map")
    #basic_type_handler = genBasicTypeHandler(btm)
    basic_type_handler_new = genBasicTypeHandlerNew(btm)
    #fbtm = parseFixBasicType("./c_grammar/fix_basic_type_map")
    #fix_basic_type_handler = genFixBasicType(fbtm)

    #infer_input = parserInferIDByDataType("./c_grammar/infer_data_type_map")
    #infer_id_by_datatype_cases = genInferIDByDataType(infer_input)
    content = content.replace("__HANDLE_BASIC_TYPES_NEW__", basic_type_handler_new)
    #content = content.replace("__HANDLE_BASIC_TYPES__", basic_type_handler)
    #content = content.replace("__FIX_BASIC_TYPES__", fix_basic_type_handler)
    #content = content.replace("__INFER_INDENTIFER_BY_DATATYPE__", infer_id_by_datatype_cases)
    return content

def genOthers():
    #os.system("cp template/mutate_header_template.h include/mutate.h")
    os.system("cp template/mutate_src_template.cpp src/mutate.cpp")
    return

def genToStringCase():
    global literalValue
    global allClass
    tmp_set = set()
    for c in allClass:
        tmp_set.add(underline_to_hump(c.name))

    res = ""
    for (key,value) in literalValue.items():
        for kk in value:
            kk = underline_to_hump(kk)
            if(kk in tmp_set):
                cmd = "\tcase k%s: return " % kk
                if(key == "string"):
                    cmd += "str_val_;"
                else:
                    cmd += "std::to_string(%s_val_);" % key
                cmd += "\n"
                res += cmd

    res = "switch(type_){\n%s\n}" % res
    return res

def parseScopeAction(filename):
    """
    format:
    classname:
    startfunction1,arg1,arg2;startfunction2,arg1,arg2...
    endfunction1,arg1,arg2;...
    """
    if(os.path.exists(filename) == False):
        print("No such file")
        return None

    content = ""
    with open(filename, 'rb') as f:
        content = f.read().strip()


    result = dict()
    result['start'] = dict()
    result['end'] = dict()
    if content == "":
        return result
    content = content.strip().split("---\n")

    for i in content:

        i = i.split("\n")
        i[0] = underline_to_hump(i[0])

        start_funcs = i[1].strip().split(";")
        start_funcs = [kk.split(",") for kk in start_funcs]
        end_funcs = i[2].strip().split(";")
        end_funcs = [kk.split(",") for kk in end_funcs]


        result['start'][i[0]] = dict()
        for sf in start_funcs:
            if(len(sf) > 1):
                result['start'][i[0]][sf[0]] = sf[1:]
            else:
                result['start'][i[0]][sf[0]] = []

        result['end'][i[0]] = dict()
        for sf in end_funcs:
            if(len(sf) > 1):
                result['end'][i[0]][sf[0]] = sf[1:]
            else:
                result['end'][i[0]][sf[0]] = []
    #print(result)
    return result

def format_output_files():
    def format_file(filename):
        formatter = "clang-format -style=\"{BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 1000}\" -i"
        os.system("%s %s" % (formatter, filename))

    format_file(configuration.ast_header_output_path)
    format_file(configuration.ast_src_output_path)
    format_file(configuration.mutate_header_output_path)
    format_file(configuration.mutate_src_output_path)
    #format_file(configuration.ts_header_output_path)
    #format_file(configuration.ts_src_output_path)


def is_non_term_case(a_case):
    res = True
    if len(a_case.symbolList) != 1 or a_case.symbolList[0].isTerminator == True or a_case.symbolList[0].name == "":
        return None
    return underline_to_hump(a_case.symbolList[0].name)

def build_convert_ir_type_map(all_class):
    result = dict()

    for c in all_class:
        c_name = c.name
        for case in c.caseList:
            node = is_non_term_case(case)
            if node == None:
                continue
            if(result.has_key(node) == False):
                result[node] = set()
            result[node].add(c_name)

    #for (key, value) in result.items():
    #    if(len(value) == 0):
    ##        continue
    #    print ("%s connected to" % key)
    #    for i in value:
    #        print("\t" + i)
    #    print("")

    return result

def can_be_converted(a, b, graph):
    parent_a = set()
    visited = set()

    bfs = list()
    bfs.append(a)
    while len(bfs) != 0:
        new_bfs = list()
        for i in bfs:
            if i in parent_a:
                continue
            parent_a.add(i)
            if graph.has_key(i) == False:
                continue
            for node in graph[i]:
                if(node not in visited):
                    new_bfs.append(node)
        bfs = new_bfs


    bfs.append(b)
    visited = set()
    while len(bfs) != 0:
        new_bfs = list()
        for i in bfs:
            visited.add(i)
            if i in parent_a:
                #print("Common parent: %s" % i)
                return i
            if graph.has_key(i) == False:
                continue
            for node in graph[i]:
                if(node not in visited):
                    new_bfs.append(node)
        bfs = new_bfs

    #print("No common parent")
    return None



if __name__ == "__main__":

    parse = argparse.ArgumentParser()
    parse.add_argument("-i", "--input", help = "Grammar description")
    parse.add_argument("-f", "--flex", help = "path of flex.l file")
    parse.add_argument("-b", "--bison", help = "path of bison.y file")
    parse.add_argument("-a", "--ast", help = "name of ast file(both .cpp and .h)")
    parse.add_argument("-t", "--token", help = "path of config file for token")
    parse.add_argument("-d", "--destructor", help = "path of destructor for bison")
    parse.add_argument("-D", "--datatype", help = "path of datatype collection")
    parse.add_argument("-s", "--semantic", help = "path of semantic rule file")
    parse.add_argument("-e", "--extraflex", help = "path of extra flex rules")
    args = parse.parse_args()

    class_define = ""
    class_content = ""
    bison_type_define = ""

    ## Parse it from a file. To do
    #notKeywordSym["COMMA"] = ','
    #notKeywordSym["LD"] = '('
    #notKeywordSym["RD"] = ')'
    #notKeywordSym["EQUAL"] = '='

    literalValue = {"float":["float_literal", "fconst"], "int":["int_literal", "iconst"], "string":["fixed_type", "identifier", "string_literal", "sconst", "guess_op","dollar_variable", "string_varname","char_literal","hex_literal", "hex_number", "pragma_directive"]}


    if(args.semantic != None):
        semanticRule = json.load(open(args.semantic))

    if(args.input != None):
        with open(args.input, "r") as input_file:
            content = input_file.read().strip()
            ptn = re.compile("Data\w+")
            kkk = re.findall(ptn, content) #ptn.findall(c)
            ss = set()
            ss.add("DataVarDefine")

            ss.add("DataFunctionType")
            ss.add("DataFunctionName")
            ss.add("DataFunctionArg")
            ss.add("DataFunctionBody")
            ss.add("DataFunctionReturnValue")

            ss.add("DataClassType")
            ss.add("DataClassName")
            ss.add("DataStructBody")

            ss.add("DataVarType")
            ss.add("DataVarName")
            ss.add("DataInitiator")
            ss.add("DataVarScope")

            ss.add("DataDeclarator")
            ss.add("DataPointer")

            ss.add("DataFixUnit")
            for kkkk in kkk:
                if str(kkkk) != "DataWhatever":
                    ss.add(str(kkkk))
            
            all_data_t = ["DataWhatever"] + list(ss)

            if(content[-1] != '\n'):
                content += "\n"
            content = content.replace("\r\n", "\n")
            content = content.split("---\n")

            for i in content:
                if i == "":
                    continue
                genclass = parseGrammar(i[:-1])

                #print((genclass))

    if(args.token != None):
        with open(args.token, "r") as token_file:
            token_info = token_file.read()
            split_by_mytest = False
            if "mytest" in token_info:
                split_by_mytest = True
            token_info = token_info.split("\n")
            if split_by_mytest:
                token_info = [i.split("mytest") for i in token_info]
            else:
                token_info = [i.split(" ") for i in token_info]
            #print(token_info)
            #print(token_info)
            for each_token in token_info:
                if(len(each_token) < 2):
                    continue
                tokenDict[each_token[0]] = Token(each_token[0], int(each_token[1]), each_token[2], each_token[3], each_token[4])

    if(args.destructor != None):
        with open(args.destructor, "r") as dest_file:
            dest_info = dest_file.read()
            dest_info = dest_info.split("---")
            for each_dest in dest_info:
                typename = each_dest.split(":\n")[0]
                dest_define = each_dest.split(":\n")[1].strip()
                destructorDict[dest_define] = [i.strip() for i in typename.split(",")]



    pu_h = genParserUtilsHeader(tokenDict)
    with open(configuration.parser_utils_header_output_path, "wb") as f:
        f.write(pu_h)

    with open(configuration.flex_output_path, "wb") as flex_file:
        flex_file.write(genFlex(tokenDict))
        flex_file.close()

    with open(configuration.bison_output_path, "wb") as bison_file:
        bison_file.write(genBison(allClass))
        bison_file.close()

    with open(configuration.ast_header_output_path, "wb") as ast_header_file:
        ast_header_file.write(genAstHeader(allClass))
        ast_header_file.close()

    with open(configuration.ast_src_output_path, "wb") as ast_content_file:
        ast_content_file.write(genAstSrc(allClass))
        ast_content_file.close()

    with open(configuration.utils_header_output_path, "wb") as f:
        f.write(genUtilsHeader())
        f.close()

    with open(configuration.utils_src_output_path, "wb") as f:
        f.write(genUtilsSrc())
        f.close()

    #all_data_t = []
    #if(args.datatype != None):
    #    with open(args.datatype, "r") as datatype:
    #        a = datatype.readlines()
    #        all_data_t = [i.strip() for i in a]
    #        print(all_data_t)

    def_h = genDefineHeader(allClass, all_data_t)
    with open(configuration.define_header_output_path, "wb") as f:
        f.write(def_h)

    mutate_h = genMutateHeader()
    with open(configuration.mutate_header_output_path, "wb") as f:
        f.write(mutate_h)

    mutate_src = genMutateSrc()
    with open(configuration.mutate_src_output_path, "wb") as f:
        f.write(mutate_src)

    ts_src = genTypeSystemSrc()
    with open(configuration.ts_src_output_path, "wb") as f:
        f.write(ts_src)

    ts_h = genTypeSystemHeader()
    with open(configuration.ts_header_output_path, "wb") as f:
        f.write(ts_h)

    vd_src = genVarDefSrc()
    with open(configuration.vardef_src_output_path, "wb") as f:
        f.write(vd_src)

    vd_h = genVarDefHeader()
    with open(configuration.vardef_header_output_path, "wb") as f:
        f.write(vd_h)

    format_output_files()

    #convert_graph = build_convert_ir_type_map(allClass)
    #while True:
    #    a = raw_input("Enter a: ")
    #    b = raw_input("Enter b: ")
    #    can_be_converted(underline_to_hump(a), underline_to_hump(b), convert_graph)
    #genOthers()
    # Test scope
    #classAPIMap["program"]



"""
    input_generation = InputGeneration(allClass)

    while(True):
        c_name = input_generation.findNextTermintable()

        if(c_name == None):
            if (len(input_generation.to_solve_set) != 0):
                print("Can't solve this grammar")
            break

        input_generation.termintateClass(c_name)
"""
