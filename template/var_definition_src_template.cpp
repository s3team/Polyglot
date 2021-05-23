#include "../include/ast.h"
#include "../include/var_definition.h"

//#define WEAK_TYPE
__IS_WEAK_TYPE__
using namespace std;
#define NOTEXISTS 0
#define DBG 0
shared_ptr<Scope> g_scope_current;
static shared_ptr<Scope> g_scope_root;
static map<int, shared_ptr<Scope>> scope_id_map;
static int g_scope;
static set<int> all_functions;
set<int> all_compound_types;
set<int> all_internal_compound_types;
set<int> all_internal_functions;
set<int> all_internal_class_methods;
map<TYPEID, shared_ptr<VarType>> internal_type_map;

map<TYPEID, map<int, TYPEID>> pointer_map; //original_type:<level: typeid>
static map<TYPEID, shared_ptr<VarType>> type_map;
static map<string, shared_ptr<VarType>> basic_types;
set<TYPEID> basic_types_set; // For faster search

bool is_internal_obj_setup = false;
bool is_in_class = false;


bool is_builtin_type(TYPEID type_id){
    for(auto i: internal_type_map){
        if(i.first == type_id) return true;
    }
    return false;
}

map<TYPEID, vector<string>> & get_all_builtin_simple_var_types(){
    static map<TYPEID, vector<string>> res;
    if(res.size() > 0) return res;


    return res;
}
map<TYPEID, vector<string>> & get_all_builtin_compound_types(){
    static map<TYPEID, vector<string>> res;
    if(res.size() > 0) return res;

    for(auto type_id: all_internal_compound_types){
        auto ptype = internal_type_map[type_id];
        res[type_id].push_back(ptype->type_name_);
    }

    return res;
}

map<TYPEID, vector<string>> & get_all_builtin_function_types(){
    static map<TYPEID, vector<string>> res;
    if(res.size() > 0) return res;

    for(auto type_id: all_internal_functions){
        auto ptype = internal_type_map[type_id];
        res[type_id].push_back(ptype->type_name_);
    }

    return res;
}

TYPEID VarType::get_type_id() const{
    return type_id_;
}


shared_ptr<Scope> get_scope_root(){
    return g_scope_root;
}

void init_convert_chain(){
    //init instance for ALLTYPES, ALLCLASS, ALLFUNCTION ...

    static vector<pair<string, string>> v_convert;
    static bool has_init = false;

    if(!has_init){
        __SEMANTIC_CONVERT_CHAIN__
        has_init = true;
    }

    for(auto &rule: v_convert){
        auto base_var = get_type_by_type_id(get_basic_type_id_by_string(rule.second));
        auto derived_var = get_type_by_type_id(get_basic_type_id_by_string(rule.first));
        if(base_var != NULL && derived_var != NULL){
            derived_var->base_type_.push_back(base_var);
            base_var->derived_type_.push_back(derived_var);
        }else{
            assert(0);
        }
    }
}

bool is_derived_type(TYPEID dtype, TYPEID btype){
    auto derived_type = get_type_by_type_id(dtype);
    auto base_type = get_type_by_type_id(btype);

    if(base_type == derived_type) return true;

    assert(derived_type && base_type);    
    
    bool res = false;

    for(auto &t: derived_type->base_type_){
        assert(t->type_id_ != dtype);
        res = res || is_derived_type(t->type_id_, btype);
    }

    return res;
}

void init_basic_types(){

    vector<string> v_basic_types = {__SEMANTIC_BASIC_TYPES__};
    for(auto &line: v_basic_types){
        if(line.empty()) continue;
        auto new_id = gen_type_id();
        auto ptr = make_shared<VarType>();
        ptr->type_id_ = new_id;
        ptr->type_name_ = line;
        basic_types[line] = ptr;
        basic_types_set.insert(new_id);
        type_map[new_id] = ptr;
        if(DBG) cout << "Basic types: " << line << ", type id: " << new_id << endl;
    }

    make_basic_type_add_map(ALLTYPES, "ALLTYPES");
    make_basic_type_add_map(ALLCOMPOUNDTYPE, "ALLCOMPOUNDTYPE");
    make_basic_type_add_map(ALLFUNCTION, "ALLFUNCTION");
    make_basic_type_add_map(ANYTYPE, "ANYTYPE");
    basic_types["ALLTYPES"] = get_type_by_type_id(ALLTYPES);
    basic_types["ALLCOMPOUNDTYPE"] = get_type_by_type_id(ALLCOMPOUNDTYPE);
    basic_types["ALLFUNCTION"] = get_type_by_type_id(ALLFUNCTION);
    basic_types["ANYTYPE"] = get_type_by_type_id(ANYTYPE);
}


TYPEID get_basic_typeid_by_string(const string & s){
    if(basic_types.find(s) != basic_types.end()){
        return basic_types[s]->get_type_id();
    }

    return NOTEXISTS;
}

int gen_type_id(){
    static int id = 10;
    return id++;
}

void init_internal_type(){
    for(auto i: all_internal_functions){
        auto ptr = get_function_type_by_type_id(i);
        g_scope_root->add_definition(ptr->type_id_, ptr->type_name_, 0);
    }


}

void clear_type_map(){
    // clear type_map
    // copy basic type back to type_map
    pointer_map.clear();
    type_map.clear();
    all_compound_types.clear();


    for(auto &i: basic_types){
        i.second->base_type_.clear();
        i.second->derived_type_.clear();
        type_map[i.second->get_type_id()] = i.second;
    }
    
    init_convert_chain();

}

set<int> get_all_class(){
    set<int> res(all_compound_types);
    res.insert(all_internal_compound_types.begin(), all_internal_compound_types.end());

    return res;
}

shared_ptr<Scope> get_scope_by_id(int scope_id){
    assert(scope_id_map.find(scope_id) != scope_id_map.end());
    return scope_id_map[scope_id];
}

shared_ptr<VarType> get_type_by_type_id(TYPEID type_id){
    if(type_map.find(type_id) != type_map.end())
        return type_map[type_id];

    if(internal_type_map.find(type_id) != internal_type_map.end())
        return internal_type_map[type_id];

    return nullptr;
}

shared_ptr<CompoundType> get_compound_type_by_type_id(TYPEID type_id){
    if(type_map.find(type_id) != type_map.end()) 
        return static_pointer_cast<CompoundType>(type_map[type_id]);
    if(internal_type_map.find(type_id) != internal_type_map.end()) 
        return static_pointer_cast<CompoundType>(internal_type_map[type_id]);

    return nullptr;
}

shared_ptr<FunctionType> get_function_type_by_type_id(TYPEID type_id){
    if(type_map.find(type_id) != type_map.end()) 
        return static_pointer_cast<FunctionType>(type_map[type_id]);
    if(internal_type_map.find(type_id) != internal_type_map.end())
        return static_pointer_cast<FunctionType>(internal_type_map[type_id]);

    return nullptr;
}

bool is_compound_type(TYPEID type_id){
    return all_compound_types.find(type_id) != all_compound_types.end() 
            || all_internal_compound_types.find(type_id) != all_internal_compound_types.end();
}

TYPEID get_compound_type_id_by_string(const string &s){
    for(auto k: all_compound_types){
        if(type_map[k]->type_name_ == s) return k;
    }

    for(auto k: all_internal_compound_types){
        if(internal_type_map[k]->type_name_ == s) return k;
    }

    return NOTEXISTS;
}

bool is_function_type(TYPEID type_id){
    return all_functions.find(type_id) != all_functions.end()
            || all_internal_functions.find(type_id) != all_internal_functions.end() 
            || all_internal_class_methods.find(type_id) != all_internal_class_methods.end();
}

bool is_basic_type(TYPEID type_id){
    return basic_types_set.find(type_id) != basic_types_set.end();
}

bool is_basic_type(const string &s){
    return basic_types.find(s) != basic_types.end();
}

TYPEID get_basic_type_id_by_string(const string& s){
    if(s == "ALLTYPES") return ALLTYPES;
    if(s == "ALLCOMPOUNDTYPE") return ALLCOMPOUNDTYPE;
    if(s == "ALLFUNCTION") return ALLFUNCTION;
    if(s == "ANYTYPE") return ANYTYPE;
    if(is_basic_type(s) == false) return NOTEXISTS;
    return basic_types[s]->get_type_id();
}

TYPEID get_type_id_by_string(const string &s){
    for(auto iter: type_map){
        if(iter.second->type_name_ == s) return iter.first;
    }

    for(auto iter: internal_type_map){
        if(iter.second->type_name_ == s) return iter.first;
    }
    return NOTEXISTS;
}

shared_ptr<FunctionType> get_function_type_by_return_type_id(TYPEID type_id){
    int counter = 0;
    shared_ptr<FunctionType> result = nullptr;
    for(auto t: all_functions){
        shared_ptr<FunctionType> tmp_ptr = static_pointer_cast<FunctionType>(type_map[t]);
        if(tmp_ptr->return_type_ == type_id){
            if(result == nullptr){
                result = tmp_ptr;
            }else{
                if(rand() % (counter + 1) < 1){
                    result = tmp_ptr;
                }
            }
            counter ++;
        }
    }
    return nullptr;
}

void make_basic_type_add_map(TYPEID id, const string &s){
    auto res = make_basic_type(id, s);
    type_map[id] = res;
    if(id == ALLCOMPOUNDTYPE){
        for(auto internal_compound: all_internal_compound_types){
            auto compound_ptr = get_type_by_type_id(internal_compound);
            res->derived_type_.push_back(compound_ptr);
        }
    }
    else if(id == ALLFUNCTION){
        // handler function here
        for(auto internal_func: all_internal_functions){
            auto func_ptr = get_type_by_type_id(internal_func);
            res->derived_type_.push_back(func_ptr);
        }
    }
    else if(id == ANYTYPE){
        for(auto &type: internal_type_map){
            res->derived_type_.push_back(type.second);
        }
    }
}

shared_ptr<VarType> make_basic_type(TYPEID id, const string &s){
    auto res = make_shared<VarType>();
    res->type_id_ = id;
    res->type_name_ = s;
     
    return res;
}

void forward_add_compound_type(string &structure_name){
    if(DBG) cout << "pre define compound type" << endl;
    auto res = make_shared<CompoundType>();
    res->type_id_ = gen_type_id();
    res->type_name_ = structure_name;
    type_map[res->type_id_] = res;
    all_compound_types.insert(res->type_id_);
    auto all_compound_type = get_type_by_type_id(ALLCOMPOUNDTYPE);  
    assert(all_compound_type);
    all_compound_type->derived_type_.push_back(res);
    res->base_type_.push_back(all_compound_type);
}

shared_ptr<CompoundType> make_compound_type_by_scope(shared_ptr<Scope> scope, string& structure_name){
    shared_ptr<CompoundType> res = nullptr;

    auto tid = get_compound_type_id_by_string(structure_name);
    if(tid != NOTEXISTS) res = get_compound_type_by_type_id(tid);
    if(res == nullptr) {
        res = make_shared<CompoundType>();
        res->type_id_ = gen_type_id();
        res->type_name_ = structure_name;
        if(is_internal_obj_setup == true){
            all_internal_compound_types.insert(res->type_id_);
            internal_type_map[res->type_id_] = res;
            auto all_compound_type = get_type_by_type_id(ALLCOMPOUNDTYPE);  
            assert(all_compound_type);
            all_compound_type->derived_type_.push_back(res);
            res->base_type_.push_back(all_compound_type);
        }
        else{
            all_compound_types.insert(res->type_id_); //here
            type_map[res->type_id_] = res; //here
            auto all_compound_type = get_type_by_type_id(ALLCOMPOUNDTYPE);  
            assert(all_compound_type);
            all_compound_type->derived_type_.push_back(res);
            res->base_type_.push_back(all_compound_type);
        }
        if(structure_name.size()){
            vector<TYPEID> tmp;
            auto pfunc = make_function_type(structure_name, res->type_id_, tmp);
            if(DBG) cout << "add definition: " << pfunc->type_id_ << ", " << structure_name << " to scope " << g_scope_root->scope_id_ << endl;
            g_scope_root->add_definition(pfunc->type_id_, structure_name, 0);
            if(DBG){
                for(auto i: g_scope_root->m_defined_variables_){
                    for(auto j: i.second)
                        cout << "member: " << j.first << endl;
                }
            }
        }
    }

    res->define_root_ = scope->v_ir_set_.back();
    if(DBG) cout << "Make compound typeid:"<< res->type_id_ <<" Scope id:" << scope->scope_id_ << endl;

    for(auto &defined_var: scope->m_defined_variables_){
        auto &var_type = defined_var.first;
        auto &var_names = defined_var.second;
        for(auto &var: var_names){
            auto &var_name = var.first;
            res->v_members_[var_type].push_back(var_name);
            if(DBG) cout << "[struct member] add member: " << var_name << " type: " << var_type << " to structure " << res->type_name_ << endl;
        }
    }
    
    return res;
}

shared_ptr<FunctionType> make_function_type(string &function_name, TYPEID return_type, vector<TYPEID> &args){
    auto res = make_shared<FunctionType>();
    res->type_id_ = gen_type_id();
    res->type_name_ = function_name;

    res->return_type_ = return_type;
    res->v_arg_types_ = args;
    if(is_internal_obj_setup == true){
        internal_type_map[res->type_id_] = res;
        if(!is_in_class)
            all_internal_functions.insert(res->type_id_);
        else
            all_internal_class_methods.insert(res->type_id_);
        auto all_function_type = get_type_by_type_id(ALLFUNCTION); 
        all_function_type->derived_type_.push_back(res);
        res->base_type_.push_back(all_function_type);
    }
    else{
        type_map[res->type_id_] = res; //here
        all_functions.insert(res->type_id_); //here
        auto all_function_type = get_type_by_type_id(ALLFUNCTION); 
        all_function_type->derived_type_.push_back(res);
        res->base_type_.push_back(all_function_type);
    }


    return res;
}

shared_ptr<FunctionType> make_function_type_by_scope(shared_ptr<Scope> scope){
    return nullptr; //may be useless
}

void Scope::add_definition(int type, IR* ir){
        if(type == 0) return ;
        m_define_ir_[type].push_back(ir);
}


void Scope::add_definition(int type, const string &var_name, unsigned long id){
        if(type == 0) return ;
        m_defined_variables_[type].push_back(make_pair(var_name, id));
}

void Scope::add_definition(int type, const string &var_name, unsigned long id, ScopeType stype){
        if(type == 0) return ;

#ifdef WEAK_TYPE
        if(stype != kScopeStatement){
            auto p = this;
            while(p != NULL && p->scope_type_ != stype) p = p->parent_.lock().get();
            if(p == NULL) p = this;
            if(p->s_defined_variable_names_.find(var_name) != p->s_defined_variable_names_.end()) return;

            p->s_defined_variable_names_.insert(var_name);
            p->m_defined_variables_[type].push_back(make_pair(var_name, id));
            
            return;
        }
        else{
            m_defined_variables_[type].push_back(make_pair(var_name, id));
            return;
        }
#endif

        m_defined_variables_[type].push_back(make_pair(var_name, id));
}

void mytest_debug(){
    if(DBG) cout << "g_scope_root: " << g_scope_root << endl;
    debug_scope_tree(g_scope_root);
}

TYPEID least_upper_common_type(TYPEID type1, TYPEID type2){

    if(type1 == type2){
        return type1;
    }

    if(is_derived_type(type1, type2)) return type2;
    else if(is_derived_type(type2, type1)) return type1;
    return NOTEXISTS;
}

TYPEID convert_to_real_type_id(TYPEID type1, TYPEID type2){

    if(type1 > ALLUPPERBOUND){
        return type1;
    }
    
    switch(type1){
        case ALLTYPES: { 
            if(type2 == ALLTYPES || type2 == NOTEXISTS){
 
                auto pick_ptr = random_pick(basic_types_set);
                return *pick_ptr;
            }
            else if(type2 == ALLCOMPOUNDTYPE){
                if(all_compound_types.empty()){
                    if(DBG) cout << "empty me" << endl;
                    break;
                }
                auto res = random_pick(all_compound_types);
                return *res;
            }
            else if(is_basic_type(type2)){
                return type2;
            }
            else{
                auto parent_type_ptr = static_pointer_cast<CompoundType>(type_map[type2]);
                if(parent_type_ptr->v_members_.empty()){
                    break;
                }
                auto res = random_pick(parent_type_ptr->v_members_);
                return res->first;
            }
        }
        case ALLFUNCTION: {
            auto res = *random_pick(all_functions);
            return res;
        }

        case ALLCOMPOUNDTYPE: {
            if(type2 == ALLTYPES || type2 == ALLCOMPOUNDTYPE || is_basic_type(type2)|| type2 == NOTEXISTS){
                if(all_compound_types.empty()){
                    if(DBG) cout << "empty me" << endl;
                    break;
                }
                auto res = random_pick(all_compound_types);
                return *res;
            }
            else{
                assert(is_compound_type(type2));
                auto parent_type_ptr = static_pointer_cast<CompoundType>(type_map[type2]);
                if(parent_type_ptr->v_members_.empty()){
                    break;
                }

                int counter = 0;
                auto res = NOTEXISTS;
                for(auto &iter: parent_type_ptr->v_members_){
                    if(is_compound_type(iter.first)){
                        if(rand() % (counter + 1) < 1){
                            res = iter.first;
                        }
                        counter ++;
                    }
                }
                return res;
            }
        }
    }
    return NOTEXISTS;
}

string CompoundType::get_member_by_type(TYPEID type){
    if(v_members_.find(type) == v_members_.end()){
        return "";
    }
    return *random_pick(v_members_[type]);
}

void debug_scope_tree(shared_ptr<Scope> cur){

    if(cur == nullptr) return;
    for(auto &child: cur->children_){
        debug_scope_tree(child.second);
    }

    if(DBG) cout << cur->scope_id_ << ":" << endl;
 
    if(DBG) cout << "Definition set: " << endl;
    for(auto &iter :cur->m_defined_variables_){
        if(DBG) cout << "Type id:" << iter.first << endl;
        auto tt = get_type_by_type_id(iter.first);
        if(tt == nullptr) continue;
        if(DBG) cout << "Type name: " << tt->type_name_ << endl;
        for(auto &name: iter.second){
            if(DBG) cout << "\t" << name.first << endl;
        }
    }
    if(DBG) cout << "------------------------" << endl;
}

void enter_scope(ScopeType scope_type){
    if(get_scope_translation_flag() == false) return;
    auto new_scope = gen_scope(scope_type);
    if(g_scope_root == NULL){
        if(DBG) cout << "set g_scope_root, g_scope_current: " <<  new_scope << endl;
        g_scope_root = g_scope_current = new_scope;
    }else{
        new_scope->parent_ = g_scope_current;
        if(DBG) cout << "use g_scope_current: " << g_scope_current << endl;
        g_scope_current->children_[new_scope->scope_id_] = new_scope;
        g_scope_current = new_scope;
        if(DBG) cout << "[enter]change g_scope_current: " << g_scope_current << endl;
    }
    if(DBG) cout << "Entering new scope: " << g_scope << endl;
}

void exit_scope(){
    if(get_scope_translation_flag() == false) return;
    if(DBG) cout << "Exit scope: " << g_scope_current->scope_id_ << endl;
    g_scope_current = g_scope_current->parent_.lock();
    if(DBG) cout << "[exit]change g_scope_current: " << g_scope_current << endl;
}

shared_ptr<Scope> gen_scope(ScopeType scope_type){
    g_scope ++ ;
    auto res = make_shared<Scope>(g_scope, scope_type);
    scope_id_map[g_scope] = res;
    return res;
}

void CompoundType::remove_unfix(IR* ir){
    can_be_fixed_ir_.erase(ir);
    v_members_[ir->value_type_].push_back(ir->str_val_);
}

string get_type_name_by_id(TYPEID type_id){
    if(type_id == 0) return "NOEXISTS";
    if(DBG) cout << "get_type name: Type: " << type_id << endl;
    if(DBG) cout << "get_type name: Str: " << type_map[type_id]->type_name_ << endl;

    if(type_map.count(type_id) > 0){
        return type_map[type_id]->type_name_;
    }
    if(internal_type_map.count(type_id) > 0){
        return internal_type_map[type_id]->type_name_;
    }

    return "NOEXIST";
}


int generate_pointer_type(int original_type, int pointer_level){

    //must be a positive level
    assert(pointer_level);
    if(pointer_map[original_type].count(pointer_level)) return pointer_map[original_type][pointer_level];

    auto cur_type = make_shared<PointerType>();
    cur_type->type_id_ = gen_type_id();
    cur_type->type_name_ = "pointer_typeid_" + std::to_string(cur_type->type_id_);
    
    int base_type = -1;
    if(pointer_level == 1)
        base_type = original_type;
    else
        base_type = generate_pointer_type(original_type, pointer_level-1);

    cur_type->orig_type_ = base_type;
    cur_type->reference_level_ = pointer_level;
    cur_type->basic_type_ = original_type;

    auto alltype_ptr = get_type_by_type_id(get_basic_type_id_by_string("ALLTYPES"));
    cur_type->base_type_.push_back(alltype_ptr);
    alltype_ptr->derived_type_.push_back(cur_type);

    type_map[cur_type->type_id_] = cur_type;
    pointer_map[original_type][pointer_level] = cur_type->type_id_;
    debug_pointer_type(cur_type);
    return cur_type->type_id_;
}

int get_or_create_pointer_type(int type){
    bool is_found = false;
    int orig_type = -1;
    int level = -1;
    int res = -1;
    bool outter_flag = false;
    for(auto &each_orig: pointer_map){
        //if type is a basic type
        if(each_orig.first == type){
            is_found = true;
            level = 0;
            orig_type = type;
            break;
        }

        //not a basic type
        for(auto &each_level: each_orig.second){          
            if(each_level.second == type){
                is_found = true;
                level = each_level.first;
                orig_type = each_orig.first;
                outter_flag = true;
                break;
            }
        }
        if(outter_flag) break;
    }

    if(is_found == true){
        assert(level != -1 && orig_type != -1);

        //found target pointer type
        if(pointer_map[orig_type].count(level+1)) return pointer_map[orig_type][level+1];

        //found original type but no this level reference
        auto cur_type = make_shared<PointerType>();
        cur_type->type_id_ = gen_type_id();
        cur_type->type_name_ = "pointer_typeid_" + std::to_string(cur_type->type_id_);
        cur_type->orig_type_ = type;
        cur_type->reference_level_ = level + 1;
        cur_type->basic_type_ = orig_type;
        auto alltype_ptr = get_type_by_type_id(get_basic_type_id_by_string("ALLTYPES"));
        alltype_ptr->derived_type_.push_back(cur_type);
        cur_type->base_type_.push_back(alltype_ptr);

        type_map[cur_type->type_id_] = cur_type;
        pointer_map[orig_type][level+1] = cur_type->type_id_;
        res = cur_type->type_id_;
        debug_pointer_type(cur_type);
    }
    else{
        //new original type
        auto cur_type = make_shared<PointerType>();
        cur_type->type_id_ = gen_type_id();
        cur_type->type_name_ = "pointer_typeid_" + std::to_string(cur_type->type_id_);
        cur_type->orig_type_ = type;
        cur_type->reference_level_ = 1;
        cur_type->basic_type_ = type;
        auto alltype_ptr = get_type_by_type_id(get_basic_type_id_by_string("ALLTYPES"));
        alltype_ptr->derived_type_.push_back(cur_type);
        cur_type->base_type_.push_back(alltype_ptr);

        type_map[cur_type->type_id_] = cur_type;
        pointer_map[type][1] = cur_type->type_id_;
        res = cur_type->type_id_;
        debug_pointer_type(cur_type);
    }

    return res;
    
}

bool is_pointer_type(int type){
    if(DBG) cout << "is_pointer_type: " << type << endl;
    auto type_ptr = get_type_by_type_id(type);
    return type_ptr->is_pointer_type();
}

shared_ptr<PointerType> get_pointer_type_by_type_id(TYPEID type_id){
    if(type_map.find(type_id) == type_map.end()) return nullptr;
    
    auto res = static_pointer_cast<PointerType>(type_map[type_id]);
    if(res->is_pointer_type() == false) return nullptr;

    return res;
}

void debug_pointer_type(shared_ptr<PointerType> &p){
    if(DBG) cout << "------new_pointer_type-------" << endl;
    if(DBG) cout << "type id: " << p->type_id_ << endl;
    if(DBG) cout << "basic_type: " << p->basic_type_ << endl;
    if(DBG) cout << "reference_level: " << p->reference_level_ << endl;
    if(DBG) cout << "-----------------------------" << endl;
}

void reset_scope(){
    if(DBG) cout << "call reset_scope" << endl;
    g_scope_current = NULL;
    g_scope_root = NULL;
    scope_id_map.clear();
    g_scope = 0;
}

void clear_definition_all(){
    clear_type_map();
    reset_scope();
}
