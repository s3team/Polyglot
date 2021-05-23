SRC_BEGIN

static bool scope_tranlation = false;

static unsigned long id_counter;
#define GEN_NAME() \
    id_ = id_counter ++;

#define STORE_IR_SCOPE() \
    if(scope_tranlation) { \
        if(g_scope_current == NULL) return; \
        g_scope_current->v_ir_set_.push_back(this); \
        this->scope_id_ = g_scope_current->scope_id_; \
    }

IR::IR(IRTYPE type,  IROperator * op, IR * left, IR* right, DATATYPE data_type):
    type_(type), op_(op), left_(left), right_(right), operand_num_((!!right)+(!!left)), data_type_(data_type){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, string str_val, DATATYPE data_type, int scope, DATAFLAG flag):
    type_(type), str_val_(str_val), op_(NULL), left_(NULL), right_(NULL), operand_num_
(0), data_type_(data_type), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, const char* str_val, DATATYPE data_type, int scope, DATAFLAG flag):
    type_(type), str_val_(str_val), op_(NULL), left_(NULL), right_(NULL), operand_num_
(0), data_type_(data_type), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, bool b_val, DATATYPE data_type, int scope, DATAFLAG flag):
    type_(type), bool_val_(b_val),left_(NULL), op_(NULL), right_(NULL), operand_num_(0), data_type_(kDataWhatever), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, unsigned long long_val, DATATYPE data_type, int scope, DATAFLAG flag):
    type_(type), long_val_(long_val),left_(NULL), op_(NULL), right_(NULL), operand_num_(0), data_type_(kDataWhatever), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, int int_val, DATATYPE data_type, int scope, DATAFLAG flag):
    type_(type), int_val_(int_val),left_(NULL), op_(NULL), right_(NULL), operand_num_(0), data_type_(kDataWhatever), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
}

IR::IR(IRTYPE type, double f_val, DATATYPE data_type, int scope, DATAFLAG flag ):
    type_(type), float_val_(f_val),left_(NULL), op_(NULL), right_(NULL), operand_num_(0), data_type_(kDataWhatever), scope_(scope) , data_flag_(flag){
        GEN_NAME();
        STORE_IR_SCOPE();
    }

IR::IR(IRTYPE type, IROperator * op, IR * left, IR* right, double f_val, string str_val, string name, unsigned int mutated_times, int scope, DATAFLAG flag):
    type_(type), op_(op), left_(left), right_(right), operand_num_((!!right)+(!!left)), name_(name), str_val_(str_val),
    float_val_(f_val), mutated_times_(mutated_times), data_type_(kDataWhatever), scope_(scope), data_flag_(flag){
        STORE_IR_SCOPE();
    }

IR::IR(const IR* ir, IR* left, IR* right){
    //STORE_IR_SCOPE();
    this->type_ = ir->type_;
    if(ir->op_ != NULL)
        this->op_ = OP3(ir->op_->prefix_, ir->op_->middle_, ir->op_->suffix_);
    else{
        this->op_ = OP0();
    }
    this->left_ = left;
    this->right_ = right;
    this->str_val_ = ir->str_val_;
    this->long_val_ = ir->long_val_;
    this->data_type_ = ir->data_type_;
    this->scope_ = ir->scope_;
    this->data_flag_ = ir->data_flag_;
    this->name_ = ir->name_;
    this->operand_num_ = ir->operand_num_;
    this->mutated_times_ = ir->mutated_times_;
}

Node* generate_ast_node_by_type(IRTYPE type){
    #define DECLARE_CASE(classname) \
    if (type == k##classname) return new classname();

    ALLCLASS(DECLARE_CASE);
    #undef DECLARE_CASE
    return NULL;
}

NODETYPE get_nodetype_by_string(string s){
    #define DECLARE_CASE(datatypename) \
    if(s == #datatypename) return k##datatypename;

    ALLCLASS(DECLARE_CASE);

    #undef DECLARE_CASE
    return kUnknown;
}

string get_string_by_nodetype(NODETYPE tt){
    #define DECLARE_CASE(datatypename) \
    if(tt == k##datatypename) return string(#datatypename);

    ALLCLASS(DECLARE_CASE);

    #undef DECLARE_CASE
    return string("");
}

string get_string_by_datatype(DATATYPE tt){
    #define DECLARE_CASE(datatypename) \
    if(tt == k##datatypename) return string(#datatypename);

    ALLDATATYPE(DECLARE_CASE);

    #undef DECLARE_CASE
    return string("");
}

DATATYPE get_datatype_by_string(string s){
    #define DECLARE_CASE(datatypename) \
    if(s == #datatypename) return k##datatypename;

    ALLDATATYPE(DECLARE_CASE);

    #undef DECLARE_CASE
    return kDataWhatever;
}

void deep_delete(IR * root){
    if(root == NULL) return;
    if(root->left_) deep_delete(root->left_);
    if(root->right_) deep_delete(root->right_);
    
    if(root->op_) delete root->op_;

    delete root;
}

IR * deep_copy(const IR * root){
    IR * left = NULL, * right = NULL, * copy_res;

    if(root->left_) left = deep_copy(root->left_);                                                
    if(root->right_) right = deep_copy(root->right_);

    copy_res = new IR(root, left, right);

    return copy_res;

}

string IR::print(){
    string res;
    res = this->name_ + " = ";
    if(!this->str_val_.empty()){
        res += "str(" + this->str_val_ + ")";
        return res;
    }
    else if(this->int_val_){
        res += "int(" + std::to_string(this->int_val_) + ")";
        return res;
    }
    if(this->op_) res += this->op_->prefix_;
    if(this->left_) res += this->left_->name_;
    if(this->op_) res += this->op_->middle_;
    if(this->right_) res += this->right_->name_;
    if(this->op_) res += this->op_->suffix_;

    return res;
}

string IR::to_string(){
    auto res = to_string_core();
    trim_string(res);
    return res;
}

string IR::to_string_core(){
    __TOSTRINGCASE__

    string res;
    
    if( op_!= NULL ){
        res += op_->prefix_ + " ";
    }
    
    if(left_ != NULL)
        res += left_->to_string_core() + " ";
    if( op_!= NULL)
        res += op_->middle_ + " ";
     
    if(right_ != NULL)
        res += right_->to_string_core() + " ";

    if(op_!= NULL)
        res += op_->suffix_;
    
    return res;
}


IR* Node::translate(vector<IR *> &v_ir_collector){
    return NULL;
}

static int cal_list_num_dfs(IR* ir, IRTYPE type){
    int res = 0;

    if(ir->type_ == type) res ++;

    if(ir->left_) res += cal_list_num_dfs(ir->left_, type);
    if(ir->right_) res += cal_list_num_dfs(ir->right_, type);

    return res;
}

void trim_list_to_num(IR* ir, int num){
    return;
}

int cal_list_num(IR* ir){
    return cal_list_num_dfs(ir, ir->type_);
}

IR* locate_parent(IR * root ,IR * old_ir) {
    if(root->left_ == old_ir || root->right_ == old_ir) return root;

    if(root->left_ != NULL) 
        if(auto res = locate_parent(root->left_, old_ir))  return res;
    if(root->right_ != NULL)
        if(auto res = locate_parent(root->right_, old_ir)) return res;
    
    return NULL;
}

IR* locate_define_top_ir(IR* root, IR* ir){
    static set<IRTYPE> define_top_set;
    static bool is_init = false;

    while(auto parent = locate_parent(root, ir)){
        if(define_top_set.find(parent->type_) != define_top_set.end()){
            return parent;
        }
        ir = parent;
    }
    cout << "root: " << root->to_string() << endl;
    cout << "find: " << ir->to_string() << endl;
    assert(0);
    return NULL;
}

void set_scope_translation_flag(bool flag){
    scope_tranlation = flag;
    if(flag == false){
        id_counter = 0;
    }
}
bool get_scope_translation_flag(){
    return scope_tranlation;
}
