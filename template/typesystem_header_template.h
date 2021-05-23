#ifndef __TYPESYSTEM_H__
#define __TYPESYSTEM_H__

#include <queue>
#include <set>
#include <map>
#include <iterator>
#include <memory>
#include <vector>
#include <string>
#include <stack>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using std::set;
using std::map;
using std::vector;
using std::string;
#define NOTEXIST 0
typedef int VALUETYPE;
typedef int OPTYPE;
class Scope;

extern unsigned long type_fix_framework_fail_counter;
extern unsigned long top_fix_fail_counter;
extern unsigned long top_fix_success_counter;

__IS_WEAK_TYPE__
//#define WEAK_TYPE

enum FIXORDER{
    LEFT_TO_RIGHT = 0,
    RIGHT_TO_LEFT,
    DEFAULT,
};

enum OPRuleProperty{
    OP_PROP_Default,
    OP_PROP_FunctionCall,
    OP_PROP_Dereference,
    OP_PROP_Reference
};

class OPRule{
public:
    OPRule(int op_id, int result, int left): op_id_(op_id), result_(result), left_(left), right_(0), operand_num_(1){}
    OPRule(int op_id, int result, int left, int right): op_id_(op_id), result_(result),left_(left), right_(right), operand_num_(2){}
    int left_, right_, result_;
    int op_id_;
    int operand_num_;
    FIXORDER fix_order_;
    bool is_op1();
    bool is_op2();
    int apply(int, int = 0);
    void add_property(const string &s);
    // more properties to add;

    OPRuleProperty property_ = OP_PROP_Default ;
};

class TypeSystem{
private:
    static map<string, map<string, map<string, int>>> op_id_map_;
    static map<int, vector<OPRule>> op_rules_;
    static int current_scope_id_;
    static shared_ptr<Scope> current_scope_ptr_;
    static bool contain_used_;
    static set<IRTYPE> s_basic_unit_;

public:
    static map<IR*, shared_ptr<map<int, vector<pair<int,int>>>>> cache_inference_map_;


    static int gen_id();
    static void init_type_dict();


    static void split_to_basic_unit(IR * root, std::queue<IR *> &q, map<IR**, IR*> &m_save, set<IRTYPE>& s_basic_unit_ptr=s_basic_unit_);

    static void connect_back(map<IR**, IR*> &m_save);

    static FIXORDER get_fix_order(int type); //need to finish

    static bool type_fix_framework(IR* root);

    static int get_op_value(IROperator* op);

    static bool is_op_null(IROperator* op);

    // new 
    static bool type_inference_new(IR * cur, int scope_type=NOTEXIST);
    static set<int> collect_usable_type(IR* cur);
    static int locate_defined_variable_by_name(const string& var_name, int scope_id);
    static string get_class_member_by_type(int type, int target_type);
    static string get_class_member_by_type_no_duplicate(int type, int target_type, set<int> &visit);
    static string get_class_member(int type_id);
    static vector<string> get_op_by_optype(OPTYPE op_type);
    static pair<OPTYPE, vector<int>> collect_sat_op_by_result_type(int type, map<int, vector<set<int>>> &a, map<int, vector<string>> &function_map, map<int, vector<string>> &compound_var_map);

    static DATATYPE find_define_type(IR* cur);
    #ifndef WEAK_TYPE
    static void collect_structure_definition(IR * cur, IR * root);
    static void collect_function_definition(IR* cur);
    #else
    static void collect_simple_variable_defintion_wt(IR* cur);
    static void collect_function_definition_wt(IR* cur);
    static void collect_structure_definition_wt(IR * cur, IR * root);
    #endif

    static bool is_contain_definition(IR* cur);
    static bool collect_definition(IR* cur);
    static string generate_expression_by_type(int type, IR* ir);
    static string generate_expression_by_type_core(int type, IR* ir);
    static vector<map<int, vector<string>>> collect_all_var_definition_by_type(IR* cur);

    static bool simple_fix(IR* ir, int type);
    static bool validate(IR* &root);
    static bool validate_syntax_only(IR *root);
    static bool top_fix(IR* root);
    IR* locate_mutated_ir(IR* root);

    static string generate_definition(string &var_name, int type);
    static string generate_definition(vector<string> &var_name, int type);

    static bool filter_compound_type(map<int, vector<string>> &compound_var_map, int type);
    static bool filter_function_type(map<int, vector<string>> &function_map, const map<int, vector<string>> &compound_var_map, 
    const map<int, vector<string>> &simple_type, int type);
    static set<int> calc_satisfiable_functions(const set<int> &function_type_set, const set<int> &available_types);
    static map<int, vector<set<int>>> collect_satisfiable_types(IR* ir, map<int, vector<string>> &simple_var_map, map<int, vector<string>> &compound_var_map, map<int, vector<string>> &function_map);
    static set<int> calc_possible_types_from_structure(int structure_type);
    static string function_call_gen_handler(map<int, vector<string>> &function_map, IR* ir);
    static string structure_member_gen_handler(map<int, vector<string>> &compound_var_map, int member_type);
    static void update_pointer_var(map<int, vector<string>> &pointer_var_map, 
                        map<int, vector<string>> &simple_var_map, map<int, vector<string>> &compound_var_map);

    static string expression_gen_handler(int type, map<int, vector<set<int>>> &all_satisfiable_types, map<int, vector<string>> &function_map, map<int, vector<string>> &compound_var_map, IR* ir);
    static OPRule parse_op_rule(string s);
    static bool is_op1(int);
    static bool is_op2(int);
    static int query_result_type(int op, int, int = 0);
    static int get_op_property(int op_id);
    static int gen_counter_, function_gen_counter_, current_fix_scope_;

    static bool insert_definition(int scope_id, int type_id, string var_name);

    // set up internal object
    static void init_internal_obj(string dir_name);
    static void init_one_internal_obj(string filename);
    static void init();
    static void debug();
};


#endif
