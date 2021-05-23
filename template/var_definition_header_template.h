#ifndef __VAR_DEFINITION_H__
#define __VAR_DEFINITION_H__

#include "define.h"
#include "utils.h"
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <cassert>

typedef int TYPEID;

#define ALLTYPES 1
#define ALLCOMPOUNDTYPE 2
#define ALLFUNCTION 3
#define ALLUPPERBOUND 5
#define ANYTYPE 6

enum ScopeType{
    kScopeGlobal,
    kScopeFunction,
    kScopeStatement,
    kScopeClass,
};

class Scope{
public:
    Scope(int scope_id, ScopeType scope_type):scope_id_(scope_id), scope_type_(scope_type){}
    ~Scope(){}
    vector<IR*> v_ir_set_; //all the irs in this scope;
    map<int, vector<IR*>> m_define_ir_;
    int scope_id_;
    map<int, shared_ptr<Scope>> children_;
    weak_ptr<Scope> parent_;
    ScopeType scope_type_; // for what type of scope
    
    map<int, vector<pair<string, unsigned long>>> m_defined_variables_;
    set<string> s_defined_variable_names_;

    void add_definition(int type, const string &var_name, unsigned long id, ScopeType stype);
    void add_definition(int type, const string &var_name, unsigned long id);
    void add_definition(int type, IR* ir);
};

class VarType{
public:
    TYPEID type_id_;
    string type_name_;
    vector<shared_ptr<VarType>> base_type_;
    vector<weak_ptr<VarType>> derived_type_;
    TYPEID get_type_id() const;
    virtual bool is_pointer_type(){return false;}
    virtual bool is_compound_type(){ return false;};
    virtual bool is_function_type(){ return false;};
};

class FunctionType: public VarType{
public:
    
    vector<IR *> return_value_ir_;
    IR* return_definition_ir_;
    TYPEID return_type_;
  
    vector<IR *> v_arguments_;
    vector<TYPEID> v_arg_types_;
    int arg_num();
    string get_arg_by_order(int);
    string get_arg_by_type(TYPEID);
    virtual bool is_pointer_type(){
        return false;
    }
    virtual bool is_compound_type(){ return false; };
    virtual bool is_function_type(){ return true; };
};

class CompoundType: public VarType{
public:
    vector<TYPEID> parent_class_;
    map<TYPEID, vector<string>> v_members_;
    set<IR *> can_be_fixed_ir_;
    IR * define_root_;

    string get_member_by_type(TYPEID type);
    void remove_unfix(IR*);
    virtual bool is_pointer_type(){ return false;}
    virtual bool is_compound_type(){ return true; };
    virtual bool is_function_type(){ return false; };
};


class PointerType: public VarType{
public:
    int orig_type_;
    int basic_type_;
    int reference_level_;
    virtual bool is_pointer_type(){return true; }
    virtual bool is_compound_type(){ return false; };
    virtual bool is_function_type(){ return false; };
};



shared_ptr<Scope> get_scope_root();
extern shared_ptr<Scope> g_scope_current;
extern set<int> all_compound_types;
extern bool is_internal_obj_setup;
extern bool is_in_class;

//for internal type
extern set<int> all_internal_compound_types;
extern set<int> all_internal_functions;
extern map<TYPEID, shared_ptr<VarType>> internal_type_map;
map<TYPEID, vector<string>> &get_all_builtin_simple_var_types();
map<TYPEID, vector<string>> &get_all_builtin_compound_types();
map<TYPEID, vector<string>> &get_all_builtin_function_types();
bool is_builtin_type(TYPEID type_id);

void init_convert_chain();
bool is_derived_type(TYPEID dtype, TYPEID btype);

void init_basic_types();
void forward_add_compound_type(string &structure_name);
shared_ptr<Scope> get_scope_by_id(int);
shared_ptr<CompoundType> make_compound_type_by_scope(shared_ptr<Scope> scope, string& structure_name);
shared_ptr<FunctionType> make_function_type_by_scope(shared_ptr<Scope> scope);
shared_ptr<FunctionType> make_function_type(string &function_name, TYPEID return_type, vector<TYPEID> &args);
shared_ptr<VarType> get_type_by_type_id(TYPEID type_id);
shared_ptr<CompoundType> get_compound_type_by_type_id(TYPEID type_id);
shared_ptr<FunctionType> get_function_type_by_type_id(TYPEID type_id);
shared_ptr<FunctionType> get_function_type_by_return_type_id(TYPEID type_id);
void make_basic_type_add_map(TYPEID id, const string &s);
shared_ptr<VarType> make_basic_type(TYPEID id, const string &s);
bool is_basic_type(TYPEID type_id);
bool is_compound_type(TYPEID type_id);
bool is_function_type(TYPEID type_id);
bool is_basic_type(const string &s);
TYPEID get_basic_type_id_by_string(const string &s);
TYPEID get_type_id_by_string(const string &s);
TYPEID get_compound_type_id_by_string(const string &s);
void mytest_debug();
int gen_type_id();
string get_type_name_by_id(TYPEID type_id);
void debug_scope_tree(shared_ptr<Scope> cur);
void enter_scope(ScopeType scope_type);
void exit_scope();
set<int> get_all_class();
TYPEID convert_to_real_type_id(TYPEID, TYPEID);
TYPEID least_upper_common_type(TYPEID, TYPEID);
shared_ptr<Scope> gen_scope(ScopeType scope_type);

//for pointer
int generate_pointer_type(int original_type, int pointer_level);
int get_or_create_pointer_type(int type);
void debug_pointer_type(shared_ptr<PointerType> &p);
bool is_pointer_type(int type);
shared_ptr<PointerType> get_pointer_type_by_type_id(TYPEID type_id);

void reset_scope();
void clear_definition_all();
void init_internal_type();
#endif