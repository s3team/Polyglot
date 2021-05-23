#include "../include/var_definition.h"
#include "../include/typesystem.h"
#include "../include/ast.h"
#include "../include/mutate.h"
#include "../include/define.h"
#include "../include/utils.h"

using namespace std;

#define DBG 0
#define NONEXISTTOANYTYPE
#define dbg_cout if(DBG)cout

map<int, vector<OPRule>> TypeSystem::op_rules_;
map<string, map<string, map<string, OPTYPE>>> TypeSystem::op_id_map_;
set<IRTYPE> TypeSystem::s_basic_unit_ = {__SEMANTIC_BASIC_UNIT__};
int TypeSystem::gen_counter_;
int TypeSystem::function_gen_counter_;
int TypeSystem::current_fix_scope_;
bool TypeSystem::contain_used_;
int TypeSystem::current_scope_id_;
shared_ptr<Scope> TypeSystem::current_scope_ptr_;
map<IR*, shared_ptr<map<TYPEID, vector<pair<TYPEID, TYPEID>>>>> TypeSystem::cache_inference_map_;

IR * cur_statement_root = NULL;

unsigned long type_fix_framework_fail_counter = 0;
unsigned long top_fix_fail_counter = 0;
unsigned long top_fix_success_counter = 0;

static set<TYPEID> current_define_types;


void TypeSystem::debug(){
    cout << "---------debug-----------" << endl;
    cout << "all_internal_compound_types: " <<  all_internal_compound_types.size() << endl;
    for(auto i: all_internal_compound_types){
        auto p = get_compound_type_by_type_id(i);
        cout << "class: " << p->type_name_ << endl;
        for(auto &i: p->v_members_){
            for(auto &name: i.second){
                cout << "\t" << name << endl;
            }
        }
        cout << endl;
    }
    cout << "---------end------------" << endl;
}

void TypeSystem::init_internal_obj(string dirname){
    auto files = get_all_files_in_dir(dirname.c_str());
    for(auto &file: files){
        init_one_internal_obj(dirname + "/" + file);
    }
}

void TypeSystem::init_one_internal_obj(string filename){

    if(DBG) cout << "Initting builtin file: " << filename << endl;
    char content[0x4000] = {0};
    auto fd = open(filename.c_str(), 0);

    read(fd, content, 0x3fff);
    close(fd);

    auto p = parser(content);
    if(p == NULL) {
        cout << "[init_internal_obj] parse " << filename << " failed" << endl; 
        return;
    }
    
    vector<IR *> v_ir;
    set_scope_translation_flag(true);
    auto res = p->translate(v_ir);
    set_scope_translation_flag(false);
    p->deep_delete();
    p = NULL;

    is_internal_obj_setup = true;
    if(type_fix_framework(res) == false)
        cout << "[init_internal_obj] setup " << filename << " failed" << endl;
    is_internal_obj_setup = false;
    
    deep_delete(res);
    
}

void TypeSystem::init(){
    init_basic_types();
    init_convert_chain();
    init_type_dict();
    init_internal_obj(__SEMANTIC_BUILTIN_OBJ__);
}

void TypeSystem::init_type_dict(){

    vector<string> type_dict = {__SEMANTIC_OP_RULE__};
    for(auto &line: type_dict){
        if(line.empty()) continue;

        OPRule rule = parse_op_rule(line);
        op_rules_[rule.op_id_].push_back(rule);
    }
}
int TypeSystem::gen_id(){
    static int id = 1;
    return id++;
}

void TypeSystem::split_to_basic_unit(IR * root, queue<IR *> &q, map<IR**, IR*> &m_save, set<IRTYPE>& s_basic_unit){
    
    if(root->left_ && s_basic_unit.find(root->left_->type_) != s_basic_unit.end()){
        m_save[&root->left_] = root->left_;
        q.push(root->left_);
        root->left_ = NULL;
    }
    if(root->left_) split_to_basic_unit(root->left_, q, m_save, s_basic_unit);

    if(root->right_ && s_basic_unit.find(root->right_->type_) != s_basic_unit.end()){
        m_save[&root->right_] = root->right_;
        q.push(root->right_);
        root->right_ = NULL;
    }
    if(root->right_) split_to_basic_unit(root->right_, q, m_save, s_basic_unit);   
}


void TypeSystem::connect_back(map<IR**, IR*> &m_save){
    for(auto &i: m_save){
        *i.first = i.second;
    }
}

bool TypeSystem::type_fix_framework(IR* root){
    static unsigned recursive_counter = 0;
    queue<IR *> q;
    map<IR**, IR*> m_save;
    int node_count = 0;
    q.push(root);
    split_to_basic_unit(root, q, m_save);

    recursive_counter ++;
    if(is_internal_obj_setup == false){
    //limit the number and the length of statements.s
        if(q.size() > 15 ){
            connect_back(m_save);
            recursive_counter--;
            return false;
        }
    }

    while(!q.empty()){
        auto cur = q.front();
        if (recursive_counter == 1)
        {
            int tmp_count = calc_node(cur);
            node_count += tmp_count;
            if ((tmp_count > 250 || node_count > 1500) && is_internal_obj_setup == false)
            {
                connect_back(m_save);
                recursive_counter--;
                return false;
            }
        }
        if (DBG)
            cout << "[splitted] " << cur->to_string() << endl;
        if (is_contain_definition(cur))
            collect_definition(cur);
        q.pop();
    }
    connect_back(m_save);
    recursive_counter--;
    return true;
}

FIXORDER TypeSystem::get_fix_order(int op)
{
    assert(op_rules_.find(op) != op_rules_.end());
    assert(op_rules_[op].empty() == false);
    return op_rules_[op].front().fix_order_;
}

int TypeSystem::get_op_value(IROperator *op)
{
    if (op == NULL)
        return 0;
    return op_id_map_[op->prefix_][op->middle_][op->suffix_];
}

bool TypeSystem::is_op_null(IROperator *op)
{
    return (op == NULL || (op->suffix_ == "" && op->middle_ == "" && op->prefix_ == ""));
}

bool TypeSystem::is_contain_definition(IR *cur)
{
    bool res = false;
    stack<IR*> stk;

    stk.push(cur);

    while(stk.empty() == false){
        cur = stk.top();
        stk.pop();
        if (cur->data_type_ == kDataVarDefine || isDefine(cur->data_flag_))
        {
            return true;
        }
        if (cur->right_) stk.push(cur->right_);
        if (cur->left_) stk.push(cur->left_);
    }
    return res;
}

void search_by_data_type(IR *cur, DATATYPE type, vector<IR *> &result, DATATYPE forbit_type = kDataWhatever, bool go_inside = false)
{
    if (cur->data_type_ == type)
    {
        result.push_back(cur);
    }
    else if (forbit_type != kDataWhatever && cur->data_type_ == forbit_type)
    {
        return;
    }
    if (cur->data_type_ != type || go_inside == true)
    {
        if (cur->left_)
        {
            search_by_data_type(cur->left_, type, result, forbit_type, go_inside);
        }
        if (cur->right_)
        {
            search_by_data_type(cur->right_, type, result, forbit_type, go_inside);
        }
    }
    return;
}

IR *search_by_data_type(IR *cur, DATATYPE type, DATATYPE forbit_type = kDataWhatever)
{
    if (cur->data_type_ == type)
    {
        return cur;
    }
    else if (forbit_type != kDataWhatever && cur->data_type_ == forbit_type)
    {
        return NULL;
    }
    else
    {
        if (cur->left_)
        {
            auto res = search_by_data_type(cur->left_, type, forbit_type);
            if (res != NULL)
                return res;
        }
        if (cur->right_)
        {
            auto res = search_by_data_type(cur->right_, type, forbit_type);
            if (res != NULL)
                return res;
        }
    }
    return NULL;
}

ScopeType scope_js(const string &s)
{
    if (s.find("var") != string::npos)
        return kScopeFunction;
    if (s.find("let") != string::npos || s.find("const") != string::npos)
        return kScopeStatement;
    for (int i = 0; i < 0x1000; i++)
        cout << s << endl;
    assert(0);
    return kScopeStatement;
}

#ifdef WEAK_TYPE
void TypeSystem::collect_simple_variable_defintion_wt(IR *cur)
{
    if (DBG)
        cout << "Collecting: " << cur->to_string() << endl;

    auto var_scope = search_by_data_type(cur, kDataVarScope);
    ScopeType scope_type = kScopeGlobal;

    // handle specill
    if (var_scope)
    {
        string str = var_scope->to_string();
        scope_type = scope_js(str);
    }

    vector<IR *> name_vec;
    vector<IR *> init_vec;
    vector<int> type_vec;
    search_by_data_type(cur, kDataVarName, name_vec);
    search_by_data_type(cur, kDataInitiator, init_vec);
    if (name_vec.empty())
    {
        if (DBG)
            cout << "fail to search for the name" << endl;
        return;
    }
    else if (name_vec.size() != init_vec.size())
    {
        for (auto i = 0; i < name_vec.size(); i++)
        {
            type_vec.push_back(ANYTYPE);
        }
    }
    else
    {
        for (auto t : init_vec)
        {
            bool flag = type_inference_new(t);
            int type = ALLTYPES;
            if (flag == true)
            {
                type = cache_inference_map_[t]->begin()->first;
            }
            //cout << "Infer type: " << type << endl;
            if (type == ALLTYPES || type == NOTEXIST)
                type = ANYTYPE;
            type_vec.push_back(type);
        }
    }

    auto cur_scope = get_scope_by_id(cur->scope_id_);
    for (auto i = 0; i < name_vec.size(); i++)
    {
        auto name_ir = name_vec[i];
        auto type = type_vec[i];
        if (DBG)
            cout << "Adding: " << name_ir->to_string() << endl;
        if (DBG)
            cout << "Scope: " << scope_type << endl;
        if (DBG)
            cout << "Type:" << get_type_name_by_id(type) << endl;
        if (cur_scope->scope_type_ == kScopeClass)
        {
            if (DBG)
            {
                cout << "Adding in class: " << name_ir->to_string() << endl;
            }
            cur_scope->add_definition(type, name_ir->to_string(), name_ir->id_, kScopeStatement);
        }
        else
        {
            cur_scope->add_definition(type, name_ir->to_string(), name_ir->id_, scope_type);
        }
    }
}

void TypeSystem::collect_function_definition_wt(IR *cur)
{
    if (DBG)
        cout << "Collecting " << cur->to_string() << endl;
    auto function_name_ir = search_by_data_type(cur, kDataFunctionName);
    auto function_args_ir = search_by_data_type(cur, kDataFunctionArg);
    //assert(function_name_ir || function_args_ir);

    string function_name;
    if (function_name_ir)
    {
        function_name = function_name_ir->to_string();
    }

    size_t num_function_args = 0;
    vector<IR *> args;
    vector<string> arg_names;
    vector<int> arg_types;
    if (function_args_ir)
    {
        search_by_data_type(function_args_ir, kDataVarName, args);
        num_function_args = args.size();
        if (DBG)
            cout << "Num arg: " << num_function_args << endl;
        for (auto i : args)
        {
            arg_names.push_back(i->to_string());
            if (DBG)
                cout << "Arg:" << i->to_string() << endl;
            arg_types.push_back(ANYTYPE);
        }
    }

    auto cur_scope = get_scope_by_id(cur->scope_id_);
    if (function_name.empty())
        function_name = "Anoynmous" + to_string(cur->id_);
    auto function_type = make_function_type(function_name, ANYTYPE, arg_types);
    if (DBG)
    {
        cout << "Collecing function name: " << function_name << endl;
    }

    cur_scope->add_definition(function_type->type_id_, function_name, function_name_ir == nullptr ? cur->id_ : function_name_ir->id_);

    auto function_body_ir = search_by_data_type(cur, kDataFunctionBody);
    if (function_body_ir)
    {
        cur_scope = get_scope_by_id(function_body_ir->scope_id_);
        for (auto i = 0; i < num_function_args; i++)
        {
            cur_scope->add_definition(ANYTYPE, arg_names[i], args[i]->id_);
        }
        if (DBG)
            cout << "Recursive on function body: " << function_body_ir->to_string() << endl;
        type_fix_framework(function_body_ir);
    }
}

void TypeSystem::collect_structure_definition_wt(IR *cur, IR *root)
{
    auto cur_scope = get_scope_by_id(cur->scope_id_);

    if (isDefine(cur->data_flag_))
    {
        vector<IR *> structure_name, strucutre_variable_name, structure_body;

        search_by_data_type(cur, kDataClassName, structure_name);
        auto struct_body = search_by_data_type(cur, kDataStructBody);
        if (struct_body == NULL)
            return;
        shared_ptr<CompoundType> new_compound;
        string current_compound_name;
        if (structure_name.size() > 0)
        {
            if (DBG)
                cout << "not anonymous " << structure_name[0]->str_val_ << endl;
            // not anonymous
            new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), structure_name[0]->str_val_);
            current_compound_name = structure_name[0]->str_val_;
        }
        else
        {
            if (DBG)
                cout << "anonymous" << endl;
            // anonymous structure
            static int anonymous_idx = 1;
            string compound_name = string("ano") + std::to_string(anonymous_idx++);
            new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), compound_name);
            current_compound_name = compound_name;
        }
        if (DBG)
            cout << struct_body->to_string() << endl;
        is_in_class = true;
        type_fix_framework(struct_body);
        is_in_class = false;
        auto compound_id = new_compound->type_id_;
        new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), current_compound_name);
    }
    else
    {
        if (cur->left_)
            collect_structure_definition_wt(cur->left_, root);
        if (cur->right_)
            collect_structure_definition_wt(cur->right_, root);
    }
}

#else

void collect_simple_variable_defintion(IR *cur)
{
    string var_type;

    if (DBG)
        cout << "Collecting: " << cur->to_string() << endl;
    vector<IR *> ir_vec;

    search_by_data_type(cur, kDataVarType, ir_vec);


    if(!ir_vec.empty()){
        for (auto ir : ir_vec)
        {
            if(ir->op_ == NULL || ir->op_->prefix_.empty()){
                auto tmpp = ir->to_string();
                var_type += tmpp.substr(0, tmpp.size() - 1);
            }else{
                var_type += ir->op_->prefix_;
            }
            var_type += " ";
        }
        var_type = var_type.substr(0, var_type.size() - 1);
    }

    int type = get_type_id_by_string(var_type);

    if(type == NOTEXIST){
        #ifdef NONEXISTTOANYTYPE
        type = ANYTYPE;
        #else
        return;
        #endif
    }

    if(DBG){
        cout << "Variable type: " << var_type << ", typeid: " << type << endl;
    }
    auto cur_scope = get_scope_by_id(cur->scope_id_);

    ir_vec.clear();

    search_by_data_type(cur, kDataDeclarator, ir_vec);
    if (ir_vec.empty())
        return;

    for (auto ir : ir_vec)
    {
        if (DBG)
            cout << "var: " << ir->to_string() << endl;
        auto name_ir = search_by_data_type(ir, kDataVarName);
        auto new_type = type;
        vector<IR *> tmp_vec;
        search_by_data_type(ir, kDataPointer, tmp_vec, kDataWhatever, true);

        if (!tmp_vec.empty())
        {
            if (DBG)
                cout << "This is a pointer definition" << endl;
            if (DBG)
                cout << "Pointer level " << tmp_vec.size() << endl;
            new_type = generate_pointer_type(type, tmp_vec.size());
        }
        else
        {
            if (DBG)
                cout << "This is not a pointer definition" << endl;
            // handle other
        }
        if (name_ir == NULL || cur_scope == NULL)
            return;
        cur_scope->add_definition(new_type, name_ir->str_val_, name_ir->id_);
    }
}

void TypeSystem::collect_structure_definition(IR *cur, IR *root)
{

    if (cur->data_type_ == kDataClassType)
    {
        if (DBG)
            cout << "to_string: " << cur->to_string() << endl;
        if (DBG)
            cout << "[collect_structure_definition] data_type_ = kDataClassType" << endl;
        auto cur_scope = get_scope_by_id(cur->scope_id_);

        if (isDefine(cur->data_flag_)){ // with structure define
            if (DBG)
                cout << "data_flag = Define" << endl;
            vector<IR *> structure_name, strucutre_variable_name, structure_body;
            search_by_data_type(cur, kDataClassName, structure_name);

            auto struct_body = search_by_data_type(cur, kDataStructBody);
            assert(struct_body);

            shared_ptr<CompoundType> new_compound;
            
            string current_compound_name;
            if (structure_name.size() > 0){
                if (DBG)
                    cout << "not anonymous" << endl;
                // not anonymous
                new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), structure_name[0]->str_val_);
                current_compound_name = structure_name[0]->str_val_;
            }
            else{
                if (DBG)
                    cout << "anonymous" << endl;
                // anonymous structure
                static int anonymous_idx = 1;
                string compound_name = string("ano") + std::to_string(anonymous_idx++);
                new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), compound_name);
                current_compound_name = compound_name;
            }
            type_fix_framework(struct_body);
            auto compound_id = new_compound->type_id_;
            new_compound = make_compound_type_by_scope(get_scope_by_id(struct_body->scope_id_), current_compound_name);

            // get all class variable define unit by finding kDataDeclarator.
            vector<IR *> strucutre_variable_unit;
            vector<IR *> structure_pointer_var;
            search_by_data_type(root, kDataDeclarator, strucutre_variable_unit, kDataStructBody);
            if (DBG)
                cout << strucutre_variable_unit.size() << endl;
            if (DBG)
                cout << root->to_string() << endl;
            if (DBG)
                cout << get_string_by_nodetype(root->type_) << endl;

            // for each class variable define unit, collect all kDataPointer.
            // it will be the reference level, if empty, it is not a pointer
            for (auto var_define_unit : strucutre_variable_unit)
            {
                search_by_data_type(var_define_unit, kDataPointer, structure_pointer_var, kDataWhatever, true);
                auto var_name = search_by_data_type(var_define_unit, kDataVarName);
                assert(var_name);
                if (structure_pointer_var.size() == 0){ //not a pointer
                    cur_scope->add_definition(compound_id, var_name->str_val_, var_name->id_);
                    if (DBG)
                        cout << "[struct]not a pointer, name: " << var_name->str_val_ << endl;
                }
                else{
                    auto new_type = generate_pointer_type(compound_id, structure_pointer_var.size());
                    cur_scope->add_definition(new_type, var_name->str_val_, var_name->id_);
                    if (DBG)
                        cout << "[struct]a pointer in level " << structure_pointer_var.size() << ", name: " << var_name->str_val_ << endl;
                }
                structure_pointer_var.clear();
            }
        }
        else if (isUse(cur->data_flag_)){ // only strucutre variable define
            if (DBG)
                cout << "data_flag = Use" << endl;
            vector<IR *> structure_name, strucutre_variable_name;
            search_by_data_type(cur, kDataClassName, structure_name);

            assert(structure_name.size());
            auto compound_id = get_type_id_by_string(structure_name[0]->str_val_);
            if (DBG){
                cout << structure_name[0]->str_val_ << endl;
                cout << "TYpe id: " << compound_id << endl;
            }
            if (compound_id == 0)
                return;

            // get all class variable define unit by finding kDataDeclarator.
            vector<IR *> strucutre_variable_unit;
            vector<IR *> structure_pointer_var;
            search_by_data_type(root, kDataDeclarator, strucutre_variable_unit, kDataStructBody);
            if (DBG)
                cout << strucutre_variable_unit.size() << endl;
            if (DBG)
                cout << root->to_string() << endl;
            if (DBG)
                cout << get_string_by_nodetype(root->type_) << endl;

            // for each class variable define unit, collect all kDataPointer.
            // it will be the reference level, if empty, it is not a pointer
            for (auto var_define_unit : strucutre_variable_unit){
                search_by_data_type(var_define_unit, kDataPointer, structure_pointer_var, kDataWhatever, true);
                auto var_name = search_by_data_type(var_define_unit, kDataVarName);
                assert(var_name);
                if (structure_pointer_var.size() == 0)
                { //not a pointer
                    cur_scope->add_definition(compound_id, var_name->str_val_, var_name->id_);
                    if (DBG)
                        cout << "[struct]not a pointer, name: " << var_name->str_val_ << endl;
                }
                else
                {
                    auto new_type = generate_pointer_type(compound_id, structure_pointer_var.size());
                    cur_scope->add_definition(new_type, var_name->str_val_, var_name->id_);
                    if (DBG)
                        cout << "[struct]a pointer in level " << structure_pointer_var.size() << ", name: " << var_name->str_val_ << endl;
                }
                structure_pointer_var.clear();
            }
        }
    }
    else
    {
        if (cur->left_)
            collect_structure_definition(cur->left_, root);
        if (cur->right_)
            collect_structure_definition(cur->right_, root);
    }
}

void TypeSystem::collect_function_definition(IR *cur)
{
    auto return_value_type_ir = search_by_data_type(cur, kDataFunctionReturnValue, kDataFunctionBody);
    auto function_name_ir = search_by_data_type(cur, kDataFunctionName, kDataFunctionBody);
    auto function_arg_ir = search_by_data_type(cur, kDataFunctionArg, kDataFunctionBody);

    string function_name_str;
    if (function_name_ir)
    {
        function_name_str = function_name_ir->to_string();
        strip_string(function_name_str);
    }

#ifdef NONEXISTTOANYTYPE
    TYPEID return_type = ANYTYPE;
#else
    TYPEID return_type = NOTEXIST;
#endif
    string return_value_type_str;
    if (return_value_type_ir)
    {
        return_value_type_str = return_value_type_ir->to_string();
        if (return_value_type_str.size() > 7)
        {
            if (return_value_type_str.substr(0, 7) == "struct ")
            {
                return_value_type_str = return_value_type_str.substr(7, return_value_type_str.size() - 7);
            }
            else if (return_value_type_str.substr(0, 5) == "enum ")
            {
                return_value_type_str = return_value_type_str.substr(5, return_value_type_str.size() - 5);
            }
        }
        strip_string(return_value_type_str);
        return_type = get_type_id_by_string(return_value_type_str);
    }

#ifdef NONEXISTTOANYTYPE
    if(return_type == NOTEXIST) return_type = ANYTYPE;
#else
#endif

    vector<TYPEID> arg_types;
    vector<string> arg_names;
    vector<unsigned long> arg_ids;
    if (function_arg_ir)
    {
        queue<IR *> q;
        map<IR **, IR *> m_save;
        set<NODETYPE> ss;
#ifdef WEAK_TYPE
#else
        ss.insert(__FUNCTION_ARGUMENT_UNIT__);
#endif

        split_to_basic_unit(function_arg_ir, q, m_save, ss);

        while (!q.empty())
        {
            auto cur = q.front();
            string var_type;
            string var_name;

            vector<IR *> ir_vec;
            vector<IR *> ir_vec_name;
            search_by_data_type(cur, kDataVarType, ir_vec);
            search_by_data_type(cur, kDataVarName, ir_vec_name);

            if (ir_vec_name.empty())
            {
                break;
            }
            // handle specially
            for (auto ir : ir_vec)
            {
                if(ir->op_ == NULL || ir->op_->prefix_.empty()){
                    auto tmpp = ir->to_string();
                    var_type += tmpp.substr(0, tmpp.size() - 1);
                }else{
                    var_type += ir->op_->prefix_;
                }
                var_type += " ";
            }
            var_type = var_type.substr(0, var_type.size() - 1);
            var_name = ir_vec_name[0]->to_string();
            auto idx = ir_vec_name[0]->id_;
            if (DBG)
                cout << "Type string: " << var_type << endl;
            if (DBG)
                cout << "Arg name: " << var_name << endl;
            if (!var_type.empty())
            {
                int type = get_basic_type_id_by_string(var_type);
                if (type == 0)
                {
                    #ifdef NONEXISTTOANYTYPE
                    type = ANYTYPE;
                    #else
                    arg_types.clear();
                    arg_names.clear();
                    arg_ids.clear();
                    break;
                    #endif
                }
                arg_types.push_back(type);
                arg_names.push_back(var_name);
                arg_ids.push_back(idx);
            }

            q.pop();
        }

        connect_back(m_save);
    }
    if (DBG)
        cout << "Function name: " << function_name_str << endl;
    if (DBG)
        cout << "return value type: " << return_value_type_str << "id:" << return_type << endl;
    if (DBG)
        cout << "Args type: " << endl;
    for (auto i : arg_types)
    {
        if (DBG)
            cout << "typeid" << endl;
        if (DBG)
            cout << get_type_by_type_id(i)->type_name_ << endl;
    }

    auto cur_scope = get_scope_by_id(cur->scope_id_);
    if (return_type)
    {
        auto function_ptr = make_function_type(function_name_str, return_type, arg_types);
        if (function_ptr == NULL || function_name_ir == NULL)
            return;

        cur_scope->add_definition(function_ptr->type_id_, function_ptr->type_name_, function_name_ir->id_);
    }

    auto function_body = search_by_data_type(cur, kDataFunctionBody);
    if (function_body)
    {
        cur_scope = get_scope_by_id(function_body->scope_id_);
        for (auto i = 0; i < arg_types.size(); i++)
        {
            cur_scope->add_definition(arg_types[i], arg_names[i], arg_ids[i]);
        }
        type_fix_framework(function_body);
    }
}
#endif

DATATYPE TypeSystem::find_define_type(IR *cur)
{
    if (cur->data_type_ == kDataVarType || cur->data_type_ == kDataClassType || cur->data_type_ == kDataFunctionType)
        return cur->data_type_;

    if (cur->left_)
    {
        auto res = find_define_type(cur->left_);
        if (res != kDataWhatever)
            return res;
    }

    if (cur->right_)
    {
        auto res = find_define_type(cur->right_);
        if (res != kDataWhatever)
            return res;
    }

    return kDataWhatever;
}

bool TypeSystem::collect_definition(IR *cur)
{
    bool res = false;
    if (cur->data_type_ == kDataVarDefine)
    {
        auto define_type = find_define_type(cur);

        switch (define_type)
        {
        case kDataVarType:
            if (DBG)
                cout << "kDataVarType" << endl;
#ifdef WEAK_TYPE
            collect_simple_variable_defintion_wt(cur);
#else
            collect_simple_variable_defintion(cur);
#endif
            return true;

        case kDataClassType:
            if (DBG)
                cout << "kDataClassType" << endl;
#ifdef WEAK_TYPE
            collect_structure_definition_wt(cur, cur);
#else
            collect_structure_definition(cur, cur);
#endif
            return true;

        case kDataFunctionType:
            if (DBG)
                cout << "kDataFunctionType" << endl;
#ifdef WEAK_TYPE
            collect_function_definition_wt(cur);
#else
            collect_function_definition(cur);
#endif
            return true;
        default:
            if (DBG)
                cout << "test default" << endl;
//handle structure and function ,array ,etc..
#ifdef WEAK_TYPE
            collect_simple_variable_defintion_wt(cur);
#else
            collect_simple_variable_defintion(cur);
#endif

            break;
        }
    }
    else
    {
        if (cur->left_)
            res = collect_definition(cur->left_) && res;
        if (cur->right_)
            res = collect_definition(cur->right_) && res;
    }

    return res;
}


bool TypeSystem::type_inference_new(IR *cur, int scope_type)
{
    auto cur_type = make_shared<map<TYPEID, vector<pair<TYPEID, TYPEID>>>>();
    int res_type = NOTEXIST;
    bool flag;
    if (DBG)
        cout << "Infering: " << cur->to_string() << endl;
    if (DBG)
        cout << "Scope type: " << scope_type << endl;
    switch (cur->type_)
    {
        __HANDLE_BASIC_TYPES_NEW__
    }

    if (cur->type_ == kIdentifier)
    {
        //handle here
        if (DBG)
            cout << "Reach here" << endl;
        if (cur->str_val_ == "FIXME")
        {
            if (DBG)
                cout << "See a fixme!" << endl;
            auto v_usable_type = collect_usable_type(cur);
           
            if (DBG)
                cout << "collect_usable_type.size(): " << v_usable_type.size() << endl;
            for (auto t : v_usable_type)
            {
                if (DBG)
                    cout << "Type: " << get_type_name_by_id(t) << endl;
                assert(t);
                (*cur_type)[t].push_back(make_pair(t, 0));
            }
            cache_inference_map_[cur] = cur_type;
            return !(cur_type->empty());
        }

        if (scope_type == NOTEXIST)
        {
            // match name in cur->scope_
            res_type = locate_defined_variable_by_name(cur->str_val_, cur->scope_id_);
            if (DBG)
                cout << "Name: " << cur->str_val_ << endl;
            if (DBG)
                cout << "Type: " << res_type << endl;
          
            if (!res_type)
                res_type = ANYTYPE; // should fix
            (*cur_type)[res_type].push_back(make_pair(res_type, 0));
            cache_inference_map_[cur] = cur_type;
            return true;

        }
        else
        {
            //match name in scope_type
            //currently only class/struct is possible
            if (DBG)
                cout << "Scope type: " << scope_type << endl;
            if (is_compound_type(scope_type) == false)
            {
                if (is_function_type(scope_type))
                {
                    auto ret_type = get_function_type_by_type_id(scope_type)->return_type_;
                    if (is_compound_type(ret_type))
                    {
                        scope_type = ret_type;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                    return false;
            }
           
            auto ct = get_compound_type_by_type_id(scope_type);
            for (auto &iter : ct->v_members_)
            {
                for (auto &member : iter.second)
                {
                    if (cur->str_val_ == member)
                    {
                        if (DBG)
                            cout << "Match member" << endl;
                       
                        assert(iter.first);
                        (*cur_type)[iter.first].push_back(make_pair(iter.first, 0));
                        cache_inference_map_[cur] = cur_type;
                        return true;
                    }
                }
            }
            if (DBG)
                cout << get_type_name_by_id(scope_type) << endl;
            if (DBG)
                cout << cur->str_val_ << endl;
            return false; // cannot find member
        }
    }

    if (is_op_null(cur->op_))
    {
        if (cur->left_ && cur->right_)
        {
            flag = type_inference_new(cur->left_, scope_type);
            if (!flag)
                return flag;
            flag = type_inference_new(cur->right_, scope_type);
            if (!flag)
                return flag;
            for (auto &left : *(cache_inference_map_[cur->left_]))
            {
                for (auto &right : *(cache_inference_map_[cur->right_]))
                {
                    auto res_type = least_upper_common_type(left.first, right.first);
                    (*cur_type)[res_type].push_back(make_pair(left.first, right.first));
                }
            }
            cache_inference_map_[cur] = cur_type;
        }
        else if (cur->left_)
        {
            flag = type_inference_new(cur->left_, scope_type);
            if (!flag || cache_inference_map_[cur->left_]->empty())
                return false;
            if (DBG)
                cout << "Left: " << cur->left_->to_string() << endl;
            assert(cache_inference_map_[cur->left_]->size());
            cache_inference_map_[cur] = cache_inference_map_[cur->left_];
        }
        else
        {
            if (DBG)
                cout << cur->to_string() << endl;
            if (DBG)
                cout << get_string_by_nodetype(cur->type_) << endl;
            return false;
            assert(0);
        }
        return true;
    }

    //handle by OP

    auto cur_op = get_op_value(cur->op_);
    if (cur_op == NOTEXIST)
    {
        if (DBG)
            cout << cur->to_string() << endl;
        if (DBG)
            cout << cur->op_->prefix_ << ", " << cur->op_->middle_ << ", " << cur->op_->suffix_ << endl;
        if (DBG)
            cout << "OP not exist!" << endl;
        return false;
        assert(0);
    }

    if (is_op1(cur_op))
    {
        if (cur->left_ == nullptr)
        {
            return false;
        }
        flag = type_inference_new(cur->left_, scope_type);
        if (!flag)
            return flag;
    
        for (auto &left : *(cache_inference_map_[cur->left_]))
        {
            auto left_type = left.first;
            if (DBG)
                cout << "Reaching op1" << endl;
            if (DBG)
                cout << cur->op_->prefix_ << ", " << cur->op_->middle_ << ", " << cur->op_->suffix_ << endl;
            res_type = query_result_type(cur_op, left_type);
            if (DBG)
                cout << "Result_type: " << res_type << endl;
            if (res_type != NOTEXIST)
            {
                (*cur_type)[res_type].push_back(make_pair(left_type, 0));
            }
        }
        cache_inference_map_[cur] = cur_type;
    }
    else if (is_op2(cur_op))
    {
        if (!(cur->left_ && cur->right_))
            return false;
       
        switch (get_fix_order(cur_op))
        {
        case LEFT_TO_RIGHT:
        {
            // this shouldn't contain "FIXME"
            if (DBG)
                cout << "Left to right" << endl;
            flag = type_inference_new(cur->left_, scope_type);
            if (!flag)
                return flag;
            if (cache_inference_map_[cur->left_]->empty())
                return false;
            auto left_type = cache_inference_map_[cur->left_]->begin()->first;
            auto new_left_type = left_type;
           
            if (get_op_property(cur_op) == OP_PROP_Dereference)
            {
                auto tmp_ptr = get_type_by_type_id(left_type);
                assert(tmp_ptr->is_pointer_type());
                if (DBG)
                    cout << "left type of -> is : " << left_type << endl;
                auto type_ptr = static_pointer_cast<PointerType>(tmp_ptr);
                if (DBG)
                    cout << "Type ptr: " << type_ptr->type_name_ << endl;
                assert(type_ptr);
                new_left_type = type_ptr->orig_type_;
            }
            flag = type_inference_new(cur->right_, new_left_type);
            if (!flag)
                return flag;
            auto right_type = cache_inference_map_[cur->right_]->begin()->first;
            res_type = right_type;
            (*cur_type)[res_type].push_back(make_pair(left_type, right_type));
            break;
        }
        case RIGHT_TO_LEFT:
        {
            assert(0);
            break;
        }
        default:
        {
            // handle a + b Here
            flag = type_inference_new(cur->left_, scope_type);
            if (!flag)
                return flag;
            flag = type_inference_new(cur->right_, scope_type);
            if (!flag)
                return flag;
            auto left_type = cache_inference_map_[cur->left_];
            auto right_type = cache_inference_map_[cur->right_];
            //handle function call case-by-case

            if (left_type->size() == 1 && is_function_type(left_type->begin()->first))
            {
                res_type = get_function_type_by_type_id(left_type->begin()->first)->return_type_;
                if (DBG)
                    cout << "get_function_type_by_type_id: " << res_type << endl;
                (*cur_type)[res_type].push_back(make_pair(0, 0)); // just 0, 0 is ok now, we won't fix it.
                break;
            }

           
            for (auto &left_cahce : *(cache_inference_map_[cur->left_]))
            {
                for (auto &right_cache : *(cache_inference_map_[cur->right_]))
                {
                    auto a_left_type = left_cahce.first;
                    auto a_right_type = right_cache.first;
                    if (DBG)
                        cout << "[a+b] left_type = " << a_left_type << ":" << get_type_name_by_id(a_left_type) << " ,right_type = " << a_right_type << ":" << get_type_name_by_id(a_right_type) << ", op = " << cur_op << endl;
                    if (DBG)
                        cout << "Left to alltype: " << is_derived_type(a_left_type, ALLTYPES) << ", right to alltype: " << is_derived_type(a_right_type, ALLTYPES) << endl;
                    auto t = query_result_type(cur_op, a_left_type, a_right_type);
                    if (t != NOTEXIST)
                    {
                        if (DBG)
                            cout << "Adding" << endl;
                        (*cur_type)[t].push_back(make_pair(a_left_type, a_right_type));
                    }
                }
            }
        }
        }
    }
    else
    {
        assert(0);
    }
    cache_inference_map_[cur] = cur_type;
    return !cur_type->empty();
}

int TypeSystem::locate_defined_variable_by_name(const string &var_name, int scope_id)
{
    int result = NOTEXIST;
    auto current_scope = get_scope_by_id(scope_id);
    while (current_scope)
    {
       
        if (current_scope->m_defined_variables_.size())
        {
            for (auto &iter : current_scope->m_defined_variables_)
            {
                for (auto &var : iter.second)
                {
                    if (var.first == var_name)
                    {
                        return iter.first;
                    }
                }
            }
        }
        current_scope = current_scope->parent_.lock();
    }

    return result;
}

set<int> TypeSystem::collect_usable_type(IR *cur)
{
    set<int> result;
    auto ir_id = cur->id_;
    auto current_scope = get_scope_by_id(cur->scope_id_);
    while (current_scope)
    {
        if (DBG)
            cout << "Collecting scope id: " << current_scope->scope_id_ << endl;
        if (current_scope->m_defined_variables_.size())
        {
            for (auto &iter : current_scope->m_defined_variables_)
            {
                auto tmp_type = iter.first;
                bool flag = false;
                
                auto type_ptr = get_type_by_type_id(tmp_type);
                if(type_ptr == NULL) continue;
                for (auto &kk : iter.second)
                {
                    if (ir_id > kk.second)
                    {
                        flag = true;
                        break;
                    }
                }

                if (flag == false)
                {
                    continue;
                }

                if (type_ptr->is_function_type())
                {
                    //whether to insert the function type it self
                    auto ptr = get_function_type_by_type_id(tmp_type);
                    result.insert(ptr->return_type_);
                }
                else if (type_ptr->is_compound_type())
                {
                    auto ptr = get_compound_type_by_type_id(tmp_type);
                    for (auto &iter : ptr->v_members_)
                    {
                        // haven't search deeper right now
                        result.insert(iter.first);
                    }
                    result.insert(tmp_type);
                }
                else
                {
                    result.insert(tmp_type);
                }
            }
        }
        current_scope = current_scope->parent_.lock();
    }

    return result;
}

vector<map<int, vector<string>>> TypeSystem::collect_all_var_definition_by_type(IR *cur)
{
    vector<map<int, vector<string>>> result;
    map<int, vector<string>> simple_var;
    map<int, vector<string>> functions;
    map<int, vector<string>> compound_types;
    map<int, vector<string>> pointer_types;
    auto cur_scope_id = cur->scope_id_;
    auto ir_id = cur->id_;
    auto current_scope = get_scope_by_id(cur_scope_id);
    while (current_scope)
    {
        
        if (current_scope->m_defined_variables_.size())
        {
            for (auto &iter : current_scope->m_defined_variables_)
            {
                auto tmp_type = iter.first;
                auto type_ptr = get_type_by_type_id(tmp_type);
                if(type_ptr == NULL) continue;
                if (type_ptr->is_function_type())
                {
                    for (auto &var : iter.second)
                    {
                        if (var.second < ir_id)
                        {
                            functions[tmp_type].push_back(var.first);
                        }
                    }
                }
                else if (type_ptr->is_compound_type())
                {
                    for (auto &var : iter.second)
                    {
                        if (var.second < ir_id)
                        {
                            if (DBG)
                                cout << "Collecting compound: " << var.first << endl;
                            compound_types[tmp_type].push_back(var.first);
                        }
                    }
                }
                else
                {
                    if (type_ptr->is_pointer_type())
                    {
                        for (auto &var : iter.second)
                        {
                            if (var.second < ir_id)
                            {
                                pointer_types[tmp_type].push_back(var.first);
                            }
                        }
                    }
                    for (auto &var : iter.second)
                    {
                        if (var.second < ir_id)
                        {
                            if (DBG)
                                cout << "Collecting simple var: " << var.first << endl;
                            simple_var[tmp_type].push_back(var.first);
                        }
                    }
                }
            }
        }
        current_scope = current_scope->parent_.lock();
    }

    // add builtin types

    auto &builtin_func = get_all_builtin_function_types();
    size_t add_prop = builtin_func.size();

    for (auto &k : builtin_func)
    {
        for (auto &n : k.second)
        {
            if (get_rand_int(add_prop) == 0)
                functions[k.first].push_back(n);
        }
    }

    if (builtin_func.size())
    {
        for (int ii = 0; ii < 2; ii++)
        {
            auto t1 = random_pick(builtin_func);
            if (t1->second.size())
                functions[t1->first].push_back(*random_pick(t1->second));
        }
    }

    auto &builtin_compounds = get_all_builtin_compound_types();
    add_prop = builtin_compounds.size();
 
    for (auto &k : builtin_compounds)
    {
        for (auto &n : k.second)
        {
            if (get_rand_int(add_prop) == 0)
                compound_types[k.first].push_back(n);
        }
    }

    if (builtin_compounds.size())
    {
        for (int ii = 0; ii < 2; ii++)
        {
            auto t1 = random_pick(builtin_compounds);
            if (t1->second.size())
                compound_types[t1->first].push_back(*random_pick(t1->second));
        }
    }

    auto &builtin_simple_var = get_all_builtin_simple_var_types();
    add_prop = builtin_simple_var.size();

    for (auto &k : builtin_simple_var)
    {
        for (auto &n : k.second)
        {
            if (get_rand_int(add_prop) == 0)
                simple_var[k.first].push_back(n);
        }
    }

    if (builtin_simple_var.size())
    {
        auto t1 = random_pick(builtin_simple_var);
        if (t1->second.size())
            simple_var[t1->first].push_back(*random_pick(t1->second));
    }

    result.push_back(move(simple_var));
    result.push_back(move(compound_types));
    result.push_back(move(functions));
    result.push_back(move(pointer_types));
    return result;
}

pair<OPTYPE, vector<int>> TypeSystem::collect_sat_op_by_result_type(int type, map<int, vector<set<int>>> &all_satisfiable_types, map<int, vector<string>> &function_map, map<int, vector<string>> &compound_var_map)
{
    static map<int, vector<vector<int>>> cache; // map<type, vector<pair<opid, int<operand_1, operand_2>>>
    auto res = make_pair(0, move(vector<int>(2, 0)));

    if (cache.empty())
    {
        for (auto &rule : op_rules_)
        {
            for (auto &r : rule.second)
            {
                if (r.operand_num_ == 2)
                {
                    cache[r.result_].push_back({r.op_id_, r.left_, r.right_});
                }
                else
                {
                    cache[r.result_].push_back({r.op_id_, r.left_});
                }
            }
        }
    }
    assert(cache.empty() == false);
    if (DBG)
        cout << "result type: " << type << endl;
    int counter = 0;
    for (auto &iter : cache)
    {
        if (DBG)
            cout << "OP result type: " << iter.first << endl;
        if (is_derived_type(type, iter.first))
        {
            for (auto &v : iter.second)
            {
                int left = 0, right = 0;
                if (v[1] > ALLUPPERBOUND && all_satisfiable_types.find(v[1]) == all_satisfiable_types.end())
                {
                    continue;
                }
                switch (v[1])
                {
                case ALLTYPES:
                    left = type;
                    break;
                case ALLFUNCTION:
                    if (function_map.empty())
                        continue;
                    left = random_pick(function_map)->first;
                    break;
                case ALLCOMPOUNDTYPE:
                default:
                    if (all_satisfiable_types.find(v[1]) == all_satisfiable_types.end())
                        continue;
                    left = v[1];
                    break;
                }

                if (v.size() == 3)
                {
                    if (v[2] > ALLUPPERBOUND && all_satisfiable_types.find(v[2]) == all_satisfiable_types.end())
                    {
                        continue;
                    }
                    right = v[2] < ALLUPPERBOUND ? type : v[2];
                }

                if (res.first == 0 || !(get_rand_int(counter + 1)))
                {
                    res.first = v[0];
                    res.second = {left, right};
                }
                counter++;
            }
        }
    }
    return res;
}

vector<string> TypeSystem::get_op_by_optype(OPTYPE op_type)
{

    for (auto &s1 : op_id_map_)
    {
        for (auto &s2 : s1.second)
        {
            for (auto &s3 : s2.second)
            {
                if (s3.second == op_type)
                    return {s1.first, s2.first, s3.first};
            }
        }
    }

    return {};
}

string TypeSystem::get_class_member_by_type_no_duplicate(int type, int target_type, set<int> &visit)
{
    string res;
    vector<string> all_sol;
    vector<shared_ptr<FunctionType>> func_sol;
    visit.insert(type);

    auto type_ptr = get_compound_type_by_type_id(type);
    for (auto &member : type_ptr->v_members_)
    {
        if (member.first == target_type)
        {
            res = "." + *random_pick(member.second);
            all_sol.push_back(res);
        }
    }

    string res1;
    for (auto &member : type_ptr->v_members_)
    {
        if (is_compound_type(member.first))
        {
            if (visit.find(member.first) != visit.end())
                continue;
            auto tmp_res = get_class_member_by_type_no_duplicate(member.first, target_type, visit);
            if (tmp_res.size())
            {
                res1 = "." + *random_pick(type_ptr->v_members_[member.first]) + tmp_res;
                all_sol.push_back(res1);
            }
        }
        else if (is_function_type(member.first))
        {
            if (member.first == target_type)
            {
                res1 = "." + *random_pick(type_ptr->v_members_[member.first]);
                all_sol.push_back(res1);
            }
            else
            {
                auto pfunc = get_function_type_by_type_id(member.first);
                if (pfunc->return_type_ != target_type)
                    continue;
                func_sol.push_back(pfunc);
            }
        }
    }

    if (all_sol.size() && !func_sol.size())
    {
        res = *random_pick(all_sol);
    }
    else if (!all_sol.size() && func_sol.size())
    {
        auto pfunc = *random_pick(func_sol);
        map<int, vector<string>> tmp_func_map;
        tmp_func_map[pfunc->type_id_] = {pfunc->type_name_};
        res = "." + function_call_gen_handler(tmp_func_map, NULL);
    }
    else if (all_sol.size() && func_sol.size())
    {
        if (get_rand_int(2))
        {
            auto pfunc = *random_pick(func_sol);
            map<int, vector<string>> tmp_func_map;
            tmp_func_map[pfunc->type_id_] = {pfunc->type_name_};
            res = "." + function_call_gen_handler(tmp_func_map, NULL);
        }
        else
        {
            res = *random_pick(all_sol);
        }
    }

    return res;
}

string TypeSystem::get_class_member_by_type(int type, int target_type)
{
    set<int> visit;
    return get_class_member_by_type_no_duplicate(type, target_type, visit);
}

bool TypeSystem::filter_compound_type(map<int, vector<string>> &compound_var_map, int type)
{
    bool res = false;
    auto tmp = compound_var_map;

    for (auto &each_compound_var : tmp)
    {
        auto compound_type = each_compound_var.first;
        auto member_name = get_class_member_by_type(compound_type, type);
        if (!member_name.empty())
        {
            res = true;
            break;
        }

        compound_var_map.erase(each_compound_var.first);
    }

    return res;
}

set<int> get_all_types_from_compound_type(int compound_type, set<int> &visit)
{
    set<int> res;
    if (visit.find(compound_type) != visit.end())
        return res;
    visit.insert(compound_type);

    auto compound_ptr = get_compound_type_by_type_id(compound_type);
    for (auto member : compound_ptr->v_members_)
    {
        auto member_type = member.first;
        if (is_compound_type(member_type))
        {
            auto sub_structure_res = get_all_types_from_compound_type(member_type, visit);
            res.insert(sub_structure_res.begin(), sub_structure_res.end());
            res.insert(member_type);
        }
        else if (is_function_type(member_type))
        {
#ifdef WEAK_TYPE
            auto pfunc = get_function_type_by_type_id(member_type);
            res.insert(pfunc->return_type_);
            res.insert(member_type);
#endif
        }
        else if (is_basic_type(member_type))
        {
            res.insert(member_type);
        }
#ifdef WEAK_TYPE
        else
        {
            res.insert(member_type);
        }
#endif
    }

    return res;
}
static map<int, set<int>> builtin_structure_type_cache;
set<int> TypeSystem::calc_possible_types_from_structure(int structure_type)
{
    set<int> visit;
    if (DBG)
    {
        auto type_ptr = get_compound_type_by_type_id(structure_type);
        cout << "[calc_possible_types_from_structure] " << type_ptr->type_name_ << endl;
    }
    if(builtin_structure_type_cache.count(structure_type)) 
        return builtin_structure_type_cache[structure_type];
    auto res = get_all_types_from_compound_type(structure_type, visit);
    if(is_builtin_type(structure_type)) 
        builtin_structure_type_cache[structure_type] = res;
    return res;
}

bool TypeSystem::filter_function_type(map<int, vector<string>> &function_map, const map<int, vector<string>> &compound_var_map,
                                      const map<int, vector<string>> &simple_type, int type)
{
    map<int, set<int>> func_map;
    set<int> current_types;

    //setup function graph into a map
    for (auto &func : function_map)
    {
        auto func_ptr = get_function_type_by_type_id(func.first);
        func_map[func.first] = set<int>(func_ptr->v_arg_types_.begin(), func_ptr->v_arg_types_.end());
    }

    //collect simple types into current_types
    for (auto &simple : simple_type)
        current_types.insert(simple.first);

    //collect all types from compound type into current_types
    set<int> visit;
    for (auto &compound : compound_var_map)
    {
        auto compound_res = get_all_types_from_compound_type(compound.first, visit);
        for (auto &i : compound_res)
            current_types.insert(i);
    }

    //traverse function graph
    //remove satisfied function type and add them into current_types
    bool is_change;
    do
    {
        is_change = false;
        for (auto func_itr = func_map.begin(); func_itr != func_map.end();)
        {
            auto func = *func_itr;
            auto func_ptr = get_function_type_by_type_id(func.first);
            for (auto &t : current_types)
            {
                func.second.erase(t);
            }
            if (func.second.size() == 0)
            {
                is_change = true;
                func_map.erase(func_itr++);
                current_types.insert(func_ptr->return_type_);
            }
            else
            {
                func_itr++;
            }
        }
    } while (is_change == true);

    //unsatisfied function remain in func_map
    for (auto &f : func_map)
    {
        if (DBG)
            cout << "remove function: " << f.first << endl;
        function_map.erase(f.first);
    }
    return true;
}

set<int> TypeSystem::calc_satisfiable_functions(const set<int> &function_type_set, const set<int> &available_types)
{
    map<int, set<int>> func_map;
    set<int> current_types = available_types;
    set<int> res = function_type_set;

    //setup function graph into a map
    for (auto &func : function_type_set)
    {
        auto func_ptr = get_function_type_by_type_id(func);
        func_map[func] = set<int>(func_ptr->v_arg_types_.begin(), func_ptr->v_arg_types_.end());
    }

    //traverse function graph
    //remove satisfied function type and add them into current_types
    bool is_change;
    do
    {
        is_change = false;
        for (auto func_itr = func_map.begin(); func_itr != func_map.end();)
        {
            auto func = *func_itr;
            auto func_ptr = get_function_type_by_type_id(func.first);
            for (auto &t : current_types)
            {
                func.second.erase(t);
            }
            if (func.second.size() == 0)
            {
                is_change = true;
                func_map.erase(func_itr++);
                current_types.insert(func_ptr->return_type_);
            }
            else
            {
                func_itr++;
            }
        }
    } while (is_change == true);

    //unsatisfied function remain in func_map
    for (auto &f : func_map)
    {
        if (DBG)
            cout << "remove function: " << f.first << endl;
        res.erase(f.first);
    }
    return res;
}

#define SIMPLE_VAR_IDX 0
#define STRUCTURE_IDX 1
#define FUNCTION_CALL_IDX 2
#define HANDLER_CASE_NUM 3

void filter_element(map<int, vector<string>> &vars, set<int> &satisfiable_types)
{
    map<int, vector<string>> filtered;
    for (auto t : satisfiable_types)
    {
        if (vars.find(t) != vars.end())
        {
            filtered[t] = move(vars[t]);
        }
    }
    vars = move(filtered);
}

map<int, vector<set<int>>> TypeSystem::collect_satisfiable_types(IR *ir, map<int, vector<string>> &simple_var_map, map<int, vector<string>> &compound_var_map, map<int, vector<string>> &function_map)
{

    map<int, vector<set<int>>> res;
    set<int> current_types;

    for (auto &t : simple_var_map)
    {
        auto type = t.first;
        if (res.find(type) == res.end())
        {
            res[type] = vector<set<int>>(HANDLER_CASE_NUM);
        }
        res[type][SIMPLE_VAR_IDX].insert(type);
        current_types.insert(type);
    }

    for (auto &t : compound_var_map)
    {
        auto type = t.first;
        if (res.find(type) == res.end())
        {
            res[type] = vector<set<int>>(HANDLER_CASE_NUM);
        }

        res[type][STRUCTURE_IDX].insert(type);
        auto member_types = calc_possible_types_from_structure(type);
        //assert(member_types.size());
        for (auto member_type : member_types)
        {
            current_types.insert(member_type);
            if (res.find(member_type) == res.end())
            {
                res[member_type] = vector<set<int>>(HANDLER_CASE_NUM);
            }
            res[member_type][STRUCTURE_IDX].insert(type);
           }
    }

    set<int> function_types;
    for (auto &t : function_map)
    {
        function_types.insert(t.first);
    }

    auto satisfiable_functions = calc_satisfiable_functions(function_types, current_types);
    for (auto type : satisfiable_functions)
    {

        auto func_ptr = get_function_type_by_type_id(type);
        auto return_type = func_ptr->return_type_;
        if (res.find(return_type) == res.end())
        {
            res[return_type] = vector<set<int>>(HANDLER_CASE_NUM);
        }
        res[return_type][FUNCTION_CALL_IDX].insert(type);
    }

    return res;
}

string TypeSystem::function_call_gen_handler(map<int, vector<string>> &function_map, IR *ir)
{
    string res;
    assert(function_map.size());
    if (DBG)
        cout << "function handler" << endl;
    auto pick_func = *random_pick(function_map);
    if (DBG)
        cout << "Function type: " << pick_func.first << endl;
    shared_ptr<FunctionType> choice_ptr = get_function_type_by_type_id(pick_func.first);

    assert(choice_ptr != nullptr);

    res = choice_ptr->type_name_;
    res += "(";
    function_gen_counter_ += choice_ptr->v_arg_types_.size();
    for (auto k : choice_ptr->v_arg_types_)
    {
        res += generate_expression_by_type_core(k, ir);
        res += ",";
    }
    if (res[res.size() - 1] == ',')
        res[res.size() - 1] = ')';
    else
        res += ")";
    return res;
}

string TypeSystem::structure_member_gen_handler(map<int, vector<string>> &compound_var_map, int member_type)
{
    if (DBG)
        cout << "Structure handler" << endl;

    string res;

    auto compound = *random_pick(compound_var_map);
    auto compound_type = compound.first;

    assert(compound.second.size());
    auto compound_var = *random_pick(compound.second);
    res = get_class_member_by_type(compound_type, member_type);
    if (res.empty())
    {
        assert(member_type == compound_type);
#ifdef WEAK_TYPE
        if (is_builtin_type(compound_type) && get_rand_int(4))
        {
            auto compound_ptr = get_type_by_type_id(compound_type);
            if (compound_ptr != nullptr && compound_var == compound_ptr->type_name_)
                return "(new " + compound_ptr->type_name_ + "())";
            else
            {
                return compound_var;
            }
        }
#endif
        return compound_var;
    }
    res = compound_var + res;

    return res;
}

string TypeSystem::get_class_member(TYPEID type_id)
{
    string res;
    auto compound_ptr = get_compound_type_by_type_id(type_id);

    if (compound_ptr == nullptr)
        return "";
    while (true)
    {
        auto var_info = *random_pick(compound_ptr->v_members_);
        auto var_type = var_info.first;
        auto var_name = *random_pick(var_info.second);
        if (is_compound_type(var_type))
        {
            res += "." + var_name;
            compound_ptr = get_compound_type_by_type_id(var_type);
        }
        else if (is_function_type(var_type))
        {
            map<int, vector<string>> tmp_func_map;
            tmp_func_map[var_type] = {var_name};
            res += "." + TypeSystem::function_call_gen_handler(tmp_func_map, NULL);
            break;
        }
        else
        {
            res += "." + var_name;
            break;
        }
    }

    return res;
}

void TypeSystem::update_pointer_var(map<int, vector<string>> &pointer_var_map,
                                    map<int, vector<string>> &simple_var_map, map<int, vector<string>> &compound_var_map)
{

    for (auto pointer_type : pointer_var_map)
    {
        if (pointer_type.second.size() == 0)
            break;
        auto pointer_id = pointer_type.first;
        auto pointer_ptr = get_pointer_type_by_type_id(pointer_id);
        if (is_basic_type(pointer_ptr->basic_type_))
        {
            for (auto var_name : pointer_type.second)
            {
                string target(pointer_ptr->reference_level_, '*');
                simple_var_map[pointer_ptr->basic_type_].push_back(target + var_name);
            }
        }
        else if (is_compound_type(pointer_ptr->basic_type_))
        {
            for (auto var_name : pointer_type.second)
            {
                string target(pointer_ptr->reference_level_, '*');
                compound_var_map[pointer_ptr->basic_type_].push_back(target + var_name);
            }
        }
    }

    return;
}

string TypeSystem::expression_gen_handler(int type, map<int, vector<set<int>>> &all_satisfiable_types, map<int, vector<string>> &function_map, map<int, vector<string>> &compound_var_map, IR *ir)
{
    string res;
    auto sat_op = collect_sat_op_by_result_type(type, all_satisfiable_types, function_map, compound_var_map); // map<OPTYPE, vector<typeid>>
    if (DBG)
        cout << "OP id: " << sat_op.first << endl;
    if (sat_op.first == 0)
    {
        return gen_random_num_string();
    }
    assert(sat_op.first);

    auto op = get_op_by_optype(sat_op.first); //vector<string> for prefix, middle, suffix
    assert(op.size());                        // should not be an empty operator
    if (is_op1(sat_op.first))
    {
        auto arg1_type = sat_op.second[0];
        auto arg1 = generate_expression_by_type_core(arg1_type, ir);
        assert(arg1.size());
        res = op[0] + " " + arg1 + " " + op[1] + op[2];
    }
    else
    {
        auto arg1_type = sat_op.second[0];
        auto arg2_type = sat_op.second[1];
        auto arg1 = generate_expression_by_type_core(arg1_type, ir);
        auto arg2 = generate_expression_by_type_core(arg2_type, ir);

        assert(arg1.size() && arg2.size());
        res = op[0] + " " + arg1 + " " + op[1] + " " + arg2 + " " + op[2];
    }
    return res;
}

string TypeSystem::generate_expression_by_type(int type, IR *ir)
{
    gen_counter_ = 0;
    function_gen_counter_ = 0;
    return generate_expression_by_type_core(type, ir);
}

string TypeSystem::generate_expression_by_type_core(int type, IR *ir)
{
    static vector<map<int, vector<string>>> var_maps;
    static map<int, vector<set<int>>> all_satisfiable_types;
    if (gen_counter_ == 0)
    {
        if(current_fix_scope_ != ir->scope_id_){
            current_fix_scope_ = ir->scope_id_;
            var_maps.clear();
            all_satisfiable_types.clear();
            var_maps = collect_all_var_definition_by_type(ir);
            all_satisfiable_types = collect_satisfiable_types(ir, var_maps[0], var_maps[1], var_maps[2]);
        }
    }

    if (gen_counter_ > 50)
        return gen_random_num_string();
    gen_counter_++;
    if (DBG)
        cout << "Generating type:" << get_type_name_by_id(type) << ", type id: " << type << endl;
    string res;

    // collect all possible types
    auto simple_var_map = var_maps[0];
    auto compound_var_map = var_maps[1];
    auto function_map = var_maps[2];
    auto pointer_var_map = var_maps[3];

    auto simple_var_size = simple_var_map.size();
    auto compound_var_size = compound_var_map.size();
    auto function_size = function_map.size();

#ifndef WEAK_TYPE
    //add pointer into *_var_map
    update_pointer_var(pointer_var_map, simple_var_map, compound_var_map);
#endif

    if (type == ALLTYPES && all_satisfiable_types.size())
    {
        type = random_pick(all_satisfiable_types)->first;
        if (DBG)
            cout << "Type becomes: " << get_type_name_by_id(type) << ", id: " << type << endl;
        if (is_function_type(type) && (get_rand_int(3)))
        {
            type = get_function_type_by_type_id(type)->return_type_;
            if (DBG)
                cout << "Type becomes: " << get_type_name_by_id(type) << ", id: " << type << endl;
        }

    }

    if (all_satisfiable_types.find(type) == all_satisfiable_types.end())
    {
        // should invoke insert_definition;
        return gen_random_num_string();
        assert(0);
    }

    int tmp_prob = 2;

#ifndef WEAK_TYPE
    while (0)
    {
        // can choose to generate its derived_type if possible
        auto type_ptr = get_type_by_type_id(type);
        if (type_ptr->derived_type_.empty())
        {
            break;
        }
        auto derived_type_ptr = (*random_pick(type_ptr->derived_type_)).lock();
        auto derived_type = derived_type_ptr->get_type_id();
        if (all_satisfiable_types.find(derived_type) != all_satisfiable_types.end())
        {
            if (get_rand_int(tmp_prob) == 0)
            {
                if (DBG)
                    cout << "Changing type from " << type << " to " << derived_type << endl;
                type = derived_type;
                tmp_prob <<= 1;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
#endif

    // Filter out unmatched types, update the sizes
    filter_element(simple_var_map, all_satisfiable_types[type][SIMPLE_VAR_IDX]);
    filter_element(compound_var_map, all_satisfiable_types[type][STRUCTURE_IDX]);
    filter_element(function_map, all_satisfiable_types[type][FUNCTION_CALL_IDX]);

    simple_var_size = simple_var_map.size();
    compound_var_size = compound_var_map.size();
    function_size = function_map.size();

// calculate probility
#define SIMPLE_VAR_WEIGHT 6
#define COMPOUND_VAR_WEIGHT 3
#define FUNCTION_WEIGHT 3
#define EXPRESSION_WEIGHT 1

    unsigned long expression_size = (function_size + compound_var_size + simple_var_size) >> 1;

    // when we use builtin types, we should not worry about this.
    if (!(function_size + compound_var_size + simple_var_size))
    {
        return gen_random_num_string();
    }

    if (expression_size == 0)
        expression_size = 1;
    if (is_function_type(type) || is_compound_type(type))
        expression_size = 0; // when meeting a function size, we do not use it in operation.

    if (DBG)
        cout << "Function size: " << function_size << endl;
    if (DBG)
        cout << "Compound size: " << compound_var_size << endl;
    if (DBG)
        cout << "Simple var size: " << simple_var_size << endl;
    expression_size <<= EXPRESSION_WEIGHT;
    function_size <<= FUNCTION_WEIGHT;
    compound_var_size <<= COMPOUND_VAR_WEIGHT;
    simple_var_size <<= SIMPLE_VAR_WEIGHT;
    if (gen_counter_ > 3)
        expression_size >>= 2;
    if (gen_counter_ > 15)
    {
        simple_var_size <<= 0x10;
        expression_size = 0;
    }
    if (function_gen_counter_ > 2)
        function_size >>= (function_gen_counter_ >> 2);
    unsigned long prob[] = {expression_size, function_size, compound_var_size, simple_var_size};

    auto total_size = simple_var_size + function_size + compound_var_size;
    if(total_size == 0) return gen_random_num_string();
    auto choice = get_rand_int(total_size);
    if (DBG)
        cout << "choice: " << choice << "/" << simple_var_size + function_size + compound_var_size + expression_size << endl;
    if (DBG)
        cout << expression_size << endl;
  
    if (0 <= choice && choice < prob[0])
    {
        if (DBG)
            cout << "exp op exp handler" << endl;
        res = expression_gen_handler(type, all_satisfiable_types, function_map, compound_var_map, ir);
        return res;
        //expr op expr
    }

    choice -= prob[0];
    if (0 <= choice && choice < prob[1])
    {
        return function_call_gen_handler(function_map, ir);
    }

    choice -= prob[1];
    if (0 <= choice && choice < prob[2])
    {
        //handle structure here
#ifdef WEAK_TYPE
        return structure_member_gen_handler(compound_var_map, type);
#else
        return structure_member_gen_handler(compound_var_map, type);
#endif
    }
    else
    {
        // handle simple var here
        if (DBG)
            cout << "simple_type handler" << endl;

        assert(!simple_var_map[type].empty());

        return *random_pick(simple_var_map[type]);
    }

    return gen_random_num_string();
}

IR *TypeSystem::locate_mutated_ir(IR *root)
{
    if (root->left_)
    {
        if (root->right_ == NULL)
        {
            return locate_mutated_ir(root->left_);
        }

        if (contain_fixme(root->right_) == false)
        {
            return locate_mutated_ir(root->left_);
        }
        if (contain_fixme(root->left_) == false)
        {
            return locate_mutated_ir(root->right_);
        }

        return root;
    }

    if (contain_fixme(root))
        return root;
    return NULL;
}

bool TypeSystem::simple_fix(IR *ir, int type)
{


    if (type == 0)
        return false;
    if (DBG)
        cout << "NodeType: " << get_string_by_nodetype(ir->type_) << endl;
    if (DBG)
        cout << "Type: " << type << endl;

    if (ir->str_val_.empty() == false && ir->str_val_ == "FIXME")
    {
        if (DBG)
            cout << "Reach here" << endl;
        ir->str_val_ = generate_expression_by_type(type, ir);
        return true;
    }

#ifndef WEAK_TYPE
    if ((*cache_inference_map_[ir]).find(type) == (*cache_inference_map_[ir]).end())
    {
        auto new_type = NOTEXIST;
        int counter = 0;
        for (auto &iter : (*cache_inference_map_[ir]))
        {
            if (is_derived_type(type, iter.first))
            {
                if (new_type == NOTEXIST || get_rand_int(counter) == 0)
                {
                    new_type = iter.first;
                }
                counter++;
            }
        }

        type = new_type;
        if (DBG)
            cout << "nothing in cache_inference_map_: " << ir->to_string() << endl;
        if (DBG)
            cout << "NodeType: " << get_string_by_nodetype(ir->type_) << endl;
    }
    if ((*cache_inference_map_[ir])[type].size() == 0)
        return false;
    assert((*cache_inference_map_[ir])[type].size());
    if (ir->left_)
    {
        auto iter = random_pick((*cache_inference_map_[ir])[type]);
        if (ir->right_)
        {
            simple_fix(ir->left_, iter->first);
            simple_fix(ir->right_, iter->second);
        }
        else
        {
            simple_fix(ir->left_, iter->first);
        }
    }
#else
    if (ir->left_)
        simple_fix(ir->left_, type);
    if (ir->right_)
        simple_fix(ir->right_, type);
#endif
    return true;
}

bool TypeSystem::top_fix(IR *root)
{
    stack<IR*> stk;

    stk.push(root);

    bool res = true;

    while (res && !stk.empty())
    {
        root = stk.top();
        stk.pop();
#ifdef WEAK_TYPE
        if (root->str_val_ == "FIXME")
        {
            int type = ALLTYPES;
            if (get_rand_int(3) != 0)
            {
                type = ANYTYPE;
            }

            res = simple_fix(root, type);
        }
#else
        if (root->type_ == __FIX_IR_TYPE__ || root->str_val_ == "FIXME")
        {
            if (contain_fixme(root) == false) continue;

            bool flag = type_inference_new(root, 0);
            if(!flag){
                res = false;
                break;
            }
            auto iter = random_pick(*(cache_inference_map_[root]));
            auto t = iter->first;
            res= simple_fix(root, t);
        }
#endif
        else
        {
            if (root->right_) stk.push(root->right_);
            if (root->left_) stk.push(root->left_);
        }
    }
    return res;
}

bool TypeSystem::validate_syntax_only(IR *root)
{
    auto ast = parser(root->to_string());
    if (ast == NULL)
    {
        return false;
    }
    ast->deep_delete();
    queue<IR *> q;
    map<IR **, IR *> m_save;
    int node_count = 0;
    q.push(root);
    split_to_basic_unit(root, q, m_save);

    if (q.size() > 15)
    {
        connect_back(m_save);
        return false;
    }

    while (!q.empty())
    {
        auto cur = q.front();
        q.pop();
        int tmp_count = calc_node(cur);
        node_count += tmp_count;
        if (tmp_count > 250 || node_count > 1500)
        {
            connect_back(m_save);
            return false;
        }
    }

    connect_back(m_save);

    return true;
}
bool TypeSystem::validate(IR *&root)
{
    bool res = false;
#ifdef SYNTAX_ONLY
    res = validate_syntax_only(root);
    if(res == false){
        type_fix_framework_fail_counter++;
    }else{
        top_fix_success_counter ++;
    }
    return res;
#else
    gen_counter_ = 0;
    current_fix_scope_ = -1;
    auto ast = parser(root->to_string());
    if (ast == NULL)
    {
        return false;
    }

    vector<IR *> ivec;

#ifndef WEAK_TYPE
    ast->translate(ivec);
    ast->deep_delete();
    deep_delete(root);
    root = ivec.back();
    extract_struct_after_mutation(root);

    ast = parser(root->to_string());
    if (ast == NULL)
    {
        return false;
    }
    ivec.clear();

    set_scope_translation_flag(true);

    ast->translate(ivec);
    ast->deep_delete();
    deep_delete(root);
    root = ivec.back();
#else
    set_scope_translation_flag(true);
    ast->translate(ivec);
    ast->deep_delete();
    deep_delete(root);
    root = ivec.back();
    extract_struct_after_mutation(root);
#endif
    res = type_fix_framework(root);
    if (res == false)
    {
        type_fix_framework_fail_counter ++;
        deep_delete(root);
        root = NULL;
    }
    else
    {

        res = top_fix(root);
        if (res == false)
        {
            top_fix_fail_counter ++;
            deep_delete(root);
            root = NULL;
        }
        top_fix_success_counter ++;
    }

    cache_inference_map_.clear();
    clear_definition_all();
    set_scope_translation_flag(false);

    return res;
#endif
}

string TypeSystem::generate_definition(string &var_name, int type)
{
    auto type_ptr = get_type_by_type_id(type);
    assert(type_ptr != nullptr);
    auto type_str = type_ptr->type_name_;
    string res = type_str + " " + var_name + ";";

    return res;
}

string TypeSystem::generate_definition(vector<string> &var_name, int type)
{
    if (DBG)
        cout << "Generating definitions for type: " << type << endl;
    auto type_ptr = get_type_by_type_id(type);
    assert(type_ptr != nullptr);
    assert(var_name.size());
    auto type_str = type_ptr->type_name_;
    string res = type_str + " ";
    string var_list = var_name[0];
    for (auto itr = var_name.begin() + 1; itr != var_name.end(); itr++)
        var_list += ", " + *itr;
    res += var_list + ";";

    return res;
}

bool OPRule::is_op1()
{
    return operand_num_ == 1;
}

bool OPRule::is_op2()
{
    return operand_num_ == 2;
}

int OPRule::apply(int arg1, int arg2)
{
    // can apply rule here.
    if (result_ != ALLTYPES && result_ != ALLCOMPOUNDTYPE && result_ != ALLFUNCTION)
    {
        return result_;
    }
    if (is_op1())
    {
        switch (property_)
        {
        case OP_PROP_Reference:
            return get_or_create_pointer_type(arg1);
        case OP_PROP_Dereference:
            if (is_pointer_type(arg1) == false)
                return NOTEXIST;
            return get_pointer_type_by_type_id(arg1)->orig_type_;
        case OP_PROP_FunctionCall:
            if (is_function_type(arg1) == false)
            {
                break;
            }

            return get_function_type_by_type_id(arg1)->return_type_;
        case OP_PROP_Default:
            return arg1;
        default:
            assert(0);
        }
    }
    else
    {
    }
    return least_upper_common_type(arg1, arg2);
}

int TypeSystem::query_result_type(int op, int arg1, int arg2)
{
    bool isop1 = arg2 == 0;

    if (op_rules_.find(op) == op_rules_.end())
    {
        dbg_cout << "Not exist op!" << endl;
        assert(0);
    }

    OPRule *rule = NULL;
    int left = 0, right = 0, result_type = 0;

    for (auto &r : op_rules_[op])
    { // exact match
        if (r.is_op1() != isop1)
            continue;

        if (isop1)
        {
            if (r.left_ == arg1)
                return r.apply(arg1);
        }
        else
        {
            if (r.left_ == arg1 && r.right_ == arg2)
                return r.apply(arg1, arg2);
        }
    }

    for (auto &r : op_rules_[op])
    { // derived type match
        if (r.is_op1() != isop1)
            continue;

        if (isop1)
        {
            if (is_derived_type(arg1, r.left_))
                return r.apply(arg1);
        }
        else
        {
            if (is_derived_type(arg1, r.left_) && is_derived_type(arg2, r.right_))
                return r.apply(arg1, arg2);
        }
    }

    if (DBG)
        cout << "Here , no exist" << endl;
    return NOTEXIST;
}

int TypeSystem::get_op_property(int op_id)
{
    assert(op_rules_.find(op_id) != op_rules_.end());
    assert(op_rules_[op_id].size());

    return op_rules_[op_id][0].property_;
}

bool TypeSystem::is_op1(int op_id)
{
    if (op_rules_.find(op_id) == op_rules_.end())
    {
        return false;
    }

    assert(op_rules_[op_id].size());

    return op_rules_[op_id][0].operand_num_ == 1;
}

bool TypeSystem::is_op2(int op_id)
{
    if (op_rules_.find(op_id) == op_rules_.end())
    {
        return false;
    }

    assert(op_rules_[op_id].size());

    return op_rules_[op_id][0].operand_num_ == 2;
}

void OPRule::add_property(const string &s)
{
    if (s == "FUNCTIONCALL")
    {
        property_ = OP_PROP_FunctionCall;
    }
    else if (s == "DEREFERENCE")
    {
        property_ = OP_PROP_Dereference;
    }
    else if (s == "REFERENCE")
    {
        property_ = OP_PROP_Reference;
    }
}

OPRule TypeSystem::parse_op_rule(string s)
{
    vector<string> v_strbuf;
    int pos = 0, last_pos = 0;
    while ((pos = s.find(" # ", last_pos)) != string::npos)
    {
        v_strbuf.push_back(s.substr(last_pos, pos - last_pos));
        last_pos = pos + 3;
    }

    v_strbuf.push_back(s.substr(last_pos, s.size() - last_pos));

    int cur_id = op_id_map_[v_strbuf[1]][v_strbuf[2]][v_strbuf[3]];
    if (cur_id == 0)
    {
        cur_id = op_id_map_[v_strbuf[1]][v_strbuf[2]][v_strbuf[3]] = gen_id();
        if (DBG)
            cout << cur_id << endl;
    }

    if (DBG)
        cout << s << endl;
    if (v_strbuf[0][0] == '2')
    {
        auto left = get_basic_type_id_by_string(v_strbuf[4]);
        auto right = get_basic_type_id_by_string(v_strbuf[5]);
        auto result = get_basic_type_id_by_string(v_strbuf[6]);

        assert(left && right && result);

        OPRule res(cur_id, result, left, right);

        if (v_strbuf[7] == "LEFT")
        {
            res.fix_order_ = LEFT_TO_RIGHT;
        }
        else if (v_strbuf[7] == "RIGHT")
        {
            res.fix_order_ = RIGHT_TO_LEFT;
        }
        else
        {
            res.fix_order_ = DEFAULT;
        }
        return move(res);
    }
    else
    {
        assert(v_strbuf[0][0] == '1');
        if (DBG)
            cout << "Here: " << v_strbuf[4] << endl;
        auto left = get_basic_type_id_by_string(v_strbuf[4]);
        auto result = get_basic_type_id_by_string(v_strbuf[5]);

        OPRule res(cur_id, result, left);
        res.add_property(v_strbuf.back());

        if (v_strbuf[6] == "LEFT")
        {
            res.fix_order_ = LEFT_TO_RIGHT;
        }
        else if (v_strbuf[6] == "RIGHT")
        {
            res.fix_order_ = RIGHT_TO_LEFT;
        }
        else
        {
            res.fix_order_ = DEFAULT;
        }
        return move(res);
    }
}

bool TypeSystem::insert_definition(int scope_id, int type_id, string var_name)
{
    auto scope_ptr = get_scope_by_id(scope_id);
    auto type_ptr = get_type_by_type_id(type_id);
    IR *insert_target = NULL;

    if (!scope_ptr || !type_ptr)
        return false;

    for (auto ir : scope_ptr->v_ir_set_)
    {
        if (isInsertable(ir->data_flag_))
        {
            insert_target = ir;
            break;
        }
    }

    if (!insert_target)
        return false;

    auto def_str = generate_definition(var_name, type_id);
    auto def_ir = new IR(kIdentifier, def_str, kDataWhatever);
    auto root_ir = new IR(kUnknown, OP0(), def_ir, insert_target->left_);

    root_ir->scope_id_ = def_ir->scope_id_ = scope_id;
    insert_target->left_ = root_ir;

    scope_ptr->add_definition(type_id, var_name, def_ir->id_);
    scope_ptr->v_ir_set_.push_back(root_ir);
    scope_ptr->v_ir_set_.push_back(def_ir);

    return true;
}
