import re
import sys

ptn = re.compile(r"([a-zA-Z]|\d)([A-Z])")
symbol = re.compile(r"'.*?'")
entry_name_list = list()
conflict_dict = dict()
def hump_to_underline(input_str):
    '''
        input: a str represents classname in hump style
        output: underline style of that classname
        e.g. SelectStatement -> select_statement
    '''
    input_str = input_str.split()
    res = ""
    flag = False
    for s in input_str:
        if(s.endswith("_LA")):
            pass
            #return ""
        if(s.isupper() == True):
            res += s + " "
            continue
        if(s.endswith(":")):
            s = s[:-1]
            flag = True
        output = re.sub(ptn, r"\1_\2", s).lower()
        if(s in conflict_dict.keys()):
            output += conflict_dict[s]
        if(flag):
            output += ":"
        res += output + " "
        #res = res.replace(" \n", '\n')
    return res


def h2u(input_str):
    '''
        input: a str represents classname in hump style
        output: underline style of that classname
        e.g. SelectStatement -> select_statement
    '''
    ptn = re.compile(r"([a-zA-Z]|\d)([A-Z])")
    output = re.sub(ptn, r"\1_\2", input_str).lower()
    return output

def readfile(filename):
    with open(filename, 'r') as f:
        content = f.read()
    return content

'''
def replace_to_hump(content):

    content = content.split("---\n")
    res = ""
    for i in content:
        if(i == ""):
            continue
        content1 = i.split("\n")
        each_block = ""
        for line in content1:
            #print hump_to_underline(line)
            tmp = hump_to_underline(line)
            if(tmp != ""):
                each_block += tmp + "\n"
        #if(len(each_block.split("\n")) > 2):
        res += each_block
        res = res[:-1] + "\n---\n"
    return res
 '''

def replace_to_hump(content):

    content = content.split("\n")
    res = ""
    for i in content:
        res += hump_to_underline(i) + "\n"
    res = res[:-1]
    return res

def get_all_symbol(content):
    symbols = symbol.findall(content)
    res = list()
    for i in symbols:
        if(i not in res):
            res.append(i)
    return res

def replace_all_symbol(content, replace_dict):
    for k, v in replace_dict.items():
        content = content.replace(k, v)

    return content.replace(" \n", "\n")

def read_symbol_file(filename):
    res = dict()
    with open(filename, 'r') as f:
        content = f.read()
        for line in content.split("\n"):
            if(line == ""):
                continue
            line = line.split()
            res[line[0]] = line[1]
    return res

def collect_tokens(content):
    res = list()
    desc_list = content.split("---")
    for desc in desc_list:
        for each_line in desc.split("\n"):
            for each_word in each_line.split():
                if each_word.isupper() and each_word not in res:
                    res.append(each_word)
    return res

def generate_token_file(token_list, symbols, prec_info):
    res = ""
    #ban_list = ["INTLITERAL", "FLOATLITERAL", "STRINGLITERAL"]
    ban_list = []
    type_dict = {'HEXLITERAL':'sval', "HEXNUNBERLITERAL":"sval", "PRAGMALITERAL": "sval", "IDENTIFIER": "sval", "OP": "sval", "INTLITERAL":"ival", "FLOATLITERAL":"fval", "STRINGLITERAL":"sval", "FIXTYPE": "sval"}
    for token in token_list:
        if token == "\n" or token in ban_list:
            continue
        token2 = token
        for k, v in symbols.items():
            if(v == token):
                token2 = '"' + k[1:-1] + '"'

        prec_idx = 0
        assoc = "0"
        ttype = "0"
        for each in prec_info:
            if(each[0] == token):
                prec_idx = each[1]
                assoc = each[2]

        for k, v in type_dict.items():
            if(k == token):
                ttype = v

        res += "%s %d %s %s %s\n" % (token, prec_idx, assoc, token2, ttype)
    return res

def replace_data_type(content):
    data_list = [["IDENT", "identifier", "IDENTIFIER"], ["ICONST", "Iconst", "INTLITERAL"], ["SCONST", "Sconst", "STRINGLITERAL"], ["FCONST", "Fconst", "FLOATLITERAL"]]
    #data_list += [["Op", ""]]
    to_add = [["identifier", "IDENTIFIER"], ["Fconst", "FLOATLITERAL"]]
    #to_add.append(['op', "OP"])
    content = content.strip()

    for i in data_list:
        content = content.replace(i[0] + "\n", i[1]+'\n')
        content = content.replace(i[0] + " ", i[1]+' ')

    for i in data_list:
        m = re.compile("\n%s:\s*%s" % (i[1], i[1]))
        content = re.sub(m, "\n%s:\n%s" %(i[1], i[2]), content)

    content += "\n"
    for i in to_add:
        content += "%s:\n%s\n---\n" %(i[0], i[1])

    #content = content.replace("prec op", "prec OP")
    return content

def fix_conflict(content):
    global entry_name_list
    global conflict_dict

    content = content.split("---\n")
    for each_grammar in content:
        entry_name = each_grammar.split(":")[0]
        uname = h2u(entry_name).strip()
        if(uname in entry_name_list):
            count = len([i for i in conflict_dict.keys() if h2u(i) == uname]) + 1
            conflict_dict[entry_name.strip()] = "_conflict" + chr(ord("a") - 1 + count)
        else:
            entry_name_list.append(uname)

def analyze_ff_info(info):
    res = list()
    if(info == ''):
        return res

    info = info.split("\n")
    prec_idx = 1

    for line in info:
        if(line == ""):
            continue
        line_info = line.split()
        ass = line_info[0][1:]
        line_info.remove(line_info[0])
        for each_keyword in line_info:
            res.append([each_keyword, prec_idx, ass])
        prec_idx += 1

    return res


if __name__ == "__main__":
    content = readfile(sys.argv[2])
    if(sys.argv[1] == "-r"):
        fix_conflict(content)
        content = replace_data_type(content)
        hump_file_content = replace_to_hump(content)
        print hump_file_content
    elif(sys.argv[1] == "-e"):
        symbols = get_all_symbol(content)
        for i in symbols:
            print i
    elif(sys.argv[1] == "-t"):
        symbols_replace = read_symbol_file("to_replace")
        symbols_replace["_P "] = " "
        symbols_replace["prec op"] = "prec OP"
        print replace_all_symbol(content, symbols_replace).strip()
    elif(sys.argv[1] == "-c"):
        ff_info = readfile(sys.argv[3])
        prec_info = analyze_ff_info(ff_info)
        token_list =  collect_tokens(content)
        symbols_replace = read_symbol_file("to_replace")
       # symbols_replace["'<='"] = "LESS_EQUALS"
       # symbols_replace["':='"] = "COLON_EQUALS"
       # symbols_replace["'!='"] = "NOT_EQUALS"
       # symbols_replace["'=>'"] = "EQUALS_GREATER"
       # symbols_replace["'>='"] = "GREATER_EQUALS"
       # symbols_replace["'->'"] = "PTR_OP"
        print generate_token_file(token_list, symbols_replace, prec_info)

