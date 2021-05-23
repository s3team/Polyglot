import re

def hump_to_underline(input_str):
    '''
        input: a str represents classname in hump style
        output: underline style of that classname
        e.g. SelectStatement -> select_statement
    '''
    size = len(input_str)
    idx = 0
    while(idx < len(input_str)-1):
        if(input_str[idx+1].isupper()):
            input_str = input_str[:idx] + input_str[idx].lower() + "_" + input_str[idx+1].lower() + input_str[idx+2:]
            idx += 1
        idx += 1
    return input_str.lower()

def underline_to_hump(input_str):
    '''
        input: a str represents classname in underline style
        output: hump style of that classname
        e.g. select_statement -> SelectStatement
    '''
    #print(input_str)
    output = re.sub(r'(_\w)',lambda x:x.group(1)[1].upper(),input_str)
    #print("Out put:" + output)
    return output[0].upper() + output[1:]


class ClassGraph:
    def __init__(self, allClass):
        self.node_map= dict() #{a:[b, c], d:[c, e]}
        self.all_circles = list()
        self.circle_hash = list()
        self.all_class = dict()

        for each_class in allClass:
            self.node_map[hump_to_underline(each_class.name)] = list()
            self.all_class[hump_to_underline(each_class.name)] = each_class
            self.setup_one_class(each_class)

        for each_class in allClass:
            cur_name = hump_to_underline(each_class.name)
            if(cur_name in self.node_map.keys()):
                self.find_circle(cur_name, cur_name, list())

        self.merge_circle()
        self.build_id_classset_map()
        self.build_circle_id()
        self.build_graph_for_circle()
        self.build_topo()


    def setup_one_class(self, each_class):
        class_name = hump_to_underline(each_class.name)
        for each_case in each_class.caseList:
            for symbol in each_case.symbolList:
                if(symbol.isTerminator):
                    continue
                self.add_edge(class_name, symbol.name)

    def find_circle(self, current_name, entry_name, res):
        if(current_name == entry_name and current_name in res):
            res.sort()
            cur_hash = "".join(res)
            if(cur_hash not in self.circle_hash):
                self.all_circles.append([i for i in res])
                self.circle_hash.append(cur_hash)
            return
        elif(current_name in res):
            return

        res.append(current_name)
        for child_name in self.node_map[current_name]:
            if(child_name in self.node_map):
                self.find_circle(child_name, entry_name, [i for i in res])

    def merge_circle(self):

        no_change = False
        while(True):
            size = len(self.all_circles)

            if(no_change == True):
                break

            no_change = True
            for i in range(0, size):
                for j in range(i+1, size):
                    set_a = set(self.all_circles[i])
                    set_b = set(self.all_circles[j])
                    inter_set = set_a.intersection(set_b)
                    if(len(inter_set) != 0):
                        self.all_circles[i] = list(set_a.union(set_b))
                        self.all_circles.remove(self.all_circles[j])
                        no_change = False
                        break

                if(no_change == False):
                    break

    def build_circle_id(self):
        res = dict()
        for node_name in self.node_map.keys():
            cur_id = -1
            for each_circle in self.all_circles:
                if(node_name in each_circle):
                    cur_id = self.all_circles.index(each_circle)
                    break
            res[node_name] = cur_id

        self.node_to_circle_id = res


    def build_id_classset_map(self):
        res = dict()
        for i in range(len(self.all_circles)):
            res[i] = self.all_circles[i]

        self.id_classset_map = res

    def rely_on(self, cn, set_id, visited = None):
        if(visited == None):
            visited = set()

        if(cn in visited):
            return False

        if(self.node_to_circle_id[cn] == set_id):
            return True

        visited.add(cn)
        cur_class = self.all_class[cn]
        for case in cur_class.caseList:
            for symbol in case.symbolList:
                if(symbol.isTerminator):
                    continue

                sym_name = symbol.name
                if(sym_name in visited):
                    continue

                if(self.rely_on(sym_name, set_id, visited) == True):
                    return True

        return False


    def has_edge(self, set_a, set_b):
        set_id = self.node_to_circle_id[set_b[0]]

        for i in set_a:
            if(self.rely_on(i, set_id)):
                return True

        return False


    def build_graph_for_circle(self):
        result = dict()
        for (key1, value1) in self.id_classset_map.items():
            result[key1] = set()

        for (key1, value1) in self.id_classset_map.items():
            for (key2, value2) in self.id_classset_map.items():
                if(key1 == key2):
                    continue

                if(self.has_edge(value1, value2)):
                    result[key1].add(key2)

        self.circle_graph = result

    def topo_add(self, node_id, visited, result):
        if(node_id in visited):
            return

        visited.add(node_id)
        for i in self.circle_graph[node_id]:
            self.topo_add(i, visited, result)

        result.append(node_id)

    def build_topo(self):

        visited = set()

        result = []

        for (node_id, value) in self.circle_graph.items():
            self.topo_add(node_id, visited, result)

        self.topo_list = result

    def debug(self):
        #print "node_map: ", self.node_map
        #print "all_circles: ", self.all_circles

        print "=============after merge================\n"
        print self.all_circles
        print "=============id_set_map=================\n"
        for (key, value) in  self.id_classset_map.items():
            print "ID: %d" % key
            print value
        print "=============circle_graph================\n"
        print self.circle_graph

        print "=============topo_list================\n"
        print self.topo_list
        """
        for k, v in self.circle_graph.items():
            print "="*30
            print self.all_circles[k]
            for i in v:
                print "-"*20
                print self.all_circles[i]
        """

    def add_edge(self, parent_name, child_name):
        if(child_name not in self.node_map[parent_name]):
            self.node_map[parent_name].append(child_name)

    def remove_edge(self, parent_name, child_name):
        if(child_name in self.node_map[parent_name]):
            self.node_map[parent_name].remove(child_name)

