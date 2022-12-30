#include<iostream>
#include<vector>
#include<algorithm>
#include<queue>
#include<cstdio>
#include<string>
using namespace std;

// 指令
struct instruction{
    string type,operand_target,operand_a,operand_b;
    int iss_time,exec_time,write_time;
    // 输出指令相关信息
    inline void print(){
        cout<<type<<" "<<operand_target<<" "<<operand_a<<" "<<operand_b<<" :";
        cout<<time2string(iss_time)<<","<<time2string(exec_time)<<","<<time2string(write_time)<<";\n";
    }
    // 将时间转换为字符串
    inline string time2string(int time){
        return time?to_string(time):"";
    }
    inline instruction(){
        iss_time = exec_time = write_time = 0;
    }
};
// 指令队列
vector<instruction> instructions;

// 表示引用的结构体
struct reference{
    string *check_value,*target_value;
    int *flag;
    reference(string *check_value,string *target_value,int *flag = nullptr):check_value(check_value),target_value(target_value),flag(flag){
        if(flag) *flag = 0;
    }

    // 如果*check_value与当前值相同，则对*target_value赋值并设置标志位
    void check_write(string &ck_value,string &tar_value){
        if(ck_value == *check_value)  *target_value = tar_value,flag?(*flag = 1):1;
    }
};

// 保留站/缓冲区
struct station{
    string name;
    bool busy;
    int inst_id;
    // 对保留站/缓冲区计算结果的引用数组
    vector<reference> references;

    // 获取保留站/缓冲区计算结果
    virtual string get_comp_value() = 0;

    // 获取保留站/缓冲区中指令执行时延
    virtual int get_exec_latency() = 0;

    // 将保留站/缓冲区相关信息转成字符串
    virtual operator string()const = 0;

    // 检查操作数是否准备完成，即指令是否可以执行
    virtual bool check()const = 0;

    station():busy(false){}

    // 保留站/缓冲区占用对应功能部件并执行指令,更新状态并返回执行完成时间
    int execute(int now_cycle){
        int exec_comp_cycle = now_cycle + get_exec_latency();
        instructions[inst_id].exec_time = exec_comp_cycle;
        return exec_comp_cycle;
    }

    // 保留站/缓冲区将计算结果写CDB,并通过references更新相关状态
    void write_cdb(int now_cycle){
        string comp_value = get_comp_value();
        for(int i = 0;i < references.size();++i)  references[i].check_write(name, comp_value);
        references.clear();
        instructions[inst_id].write_time = now_cycle;
        busy = false;
    }
};

// load指令对应缓冲区
struct load_station:public station{
    string address;

    virtual operator string()const{
        string tmp = name;
        if(busy) tmp+=":Yes,"+address+";\n";
        else tmp+=":No,;\n";
        return tmp;
    }

    virtual string get_comp_value(){
        return "M("+address+")";
    }

    // load指令执行时延为2
    virtual int get_exec_latency(){
        return 2;
    }

    // load指令不需要操作数
    virtual bool check()const{
        return true;
    }
}load_stations[3];// 3个load指令的缓冲区

// store指令对应缓冲区
struct store_station:public station{
    string address,value;
    int flag;

    virtual string get_comp_value(){
        return "";
    }
    // save指令执行时延为2
    virtual int get_exec_latency(){
        return 2;
    }

    // 使用标志位flag表示操作数是否准备完成
    bool check()const{
        return flag;
    }

    virtual operator string()const{
        string tmp = name;
        if(busy) tmp+=":Yes,"+address+","+value+";\n";
        else tmp+=":No,,;\n";
        return tmp;
    }
}store_stations[3];

// 通常的保留站
struct reservation_station:public station{
    string op,vi,vj,qi,qj;

    virtual operator string()const{
        string tmp = name;
        if(busy) {
            tmp+=":Yes,"+op+',';
            if(vi.size()) tmp+=vi;
            tmp+=",";
            if(vj.size()) tmp+=vj;
            tmp+=",";
            if(!vi.size()) tmp+=qi;
            tmp+=",";
            if(!vj.size()) tmp+=qj;
            tmp+=";\n";
        }
        else tmp+=":No,,,,,;\n";
        return tmp;
    }

    virtual string get_comp_value(){
        string comp_value;
        switch (op[0]){
            case 'A': comp_value = vi + "+" + vj;
            break;
            case 'S': comp_value = vi + "-(" + vj +")";
            break;
            case 'M': comp_value = "("+vi + ")*(" + vj +")";
            break;
            case 'D': comp_value = "("+vi + ")/(" + vj +")";
            break;
            default: 
            perror("Invalid operation name"),exit(-1);
        }
        return comp_value;
    }

    //获取对应时延
    virtual int get_exec_latency(){
        int latency = 0;
        switch (op[0]){
            case 'A': latency = 2;
            break;
            case 'S': latency = 2;
            break;
            case 'M': latency = 10;
            break;
            case 'D': latency = 20;
            break;
            default: 
            perror("Invalid operation name"),exit(-1);
        }
        return latency;
    }
    virtual bool check()const{
        return vi.size()&&vj.size();
    }
}reservation_stations[5];
// 5个保留站

// FU部件
struct Fu{
    string name,value;
    int flag;
    // value.size()为0表示FU为初始值
    // 否则flag为0表示value为计算结果,value已准备好
    // 为1表示value为生成计算结果的功能部件名，即value未准备好

    void print(){
        cout<<name+":";
        if(value.size()) cout<<value;
        cout<<";";
    }

    operator string()const{
        string tmp = name+":";
        if(value.size()) tmp+=value;
        tmp+=";";
        return tmp;
    }

    // 获取FU的值，如果FU的值未准备好则返回空串
    string get_value()const{
        if(value.empty()) return "R("+name+")";
        if(flag) return value;
        return "";
    }
    
}Fus[7];//FU部件有7个

// 待处理的提交项，即已经执行完毕等待写CDB的任务
struct submission{
    // 对应的保留站/缓冲区id,对应空闲队列及其自身指针
    queue<int> *target_free_queue;
    int id;
    station *task;

    submission(queue<int> *target_free_queue,int id,station *task):target_free_queue(target_free_queue),id(id),task(task){
    }

    //允许该项提交(写CDB并释放资源)
    void write_cdb(int now_cycle){
        task->write_cdb(now_cycle);
        target_free_queue->push(id);
    }
};

// 正在执行的任务
struct executing_task{

    // 对应的保留站/缓冲区id,信号量、指令执行完成的时间、空闲队列及其自身指针，
    queue<int> *target_free_queue;
    int id,exec_comp_cycle;
    station *task;
    int *semaphore;

    // 保留站/缓冲区占用功能部件，开始执行指令
    executing_task(queue<int> *target_free_queue,int id,station *task,int *semaphore,int now_cycle):target_free_queue(target_free_queue),
        id(id),task(task),semaphore(semaphore){
        exec_comp_cycle = task->execute(now_cycle);
        (*semaphore)--;
    }

    // 对应的指令执行完毕，释放功能部件并等待提交(写CDB)
    submission exec_complete(){
        ++*semaphore;
        return submission(target_free_queue,id,task);
    }

    // 重载<运算符，使得优先队列按照指令执行完成的时间从小到大排序
    bool operator <(const executing_task &o)const{
        return exec_comp_cycle > o.exec_comp_cycle;
    }
};

// 信号量，用于表示对应空闲功能部件的个数
int semaphore_load = 3,semaphore_store = 3,semaphore_mult = 2,semaphore_add = 3;

// 空闲保留站/缓冲区队列
queue<int> free_load_stations,free_store_stations,free_add_stations,free_mult_stations;

// 因等待操作数或功能部件而堵塞的保留站/缓冲区队列
vector<int> block_load_stations,block_store_stations,block_add_stations,block_mult_stations;

// 以指令完成时间为优先级的正在执行的任务的优先队列
// 即按时间推移，而先后执行完毕的任务队列
priority_queue<executing_task> exec_stations;

// 等待处理以写CDB的提交队列
queue<submission> exec_comp_stations;

// 完成各部件的初始化(命名及赋值),同时从输入中读入指令
void init(){
    for(int i = 0;i < 3;++i) free_load_stations.push(i),free_store_stations.push(i);
    for(int i = 0;i < 3;++i) load_stations[i].name = "Load" + to_string(i + 1),store_stations[i].name = "Store" + to_string(i + 1);
    for(int i = 0;i < 3;++i) free_add_stations.push(i),reservation_stations[i].name = "Add" + to_string(i + 1);
    for(int i = 3;i < 5;++i) free_mult_stations.push(i),reservation_stations[i].name = "Mult" + to_string(i - 2);
    for(int i = 0;i < 7;++i) Fus[i].name = "F"+ to_string(i<<1);

    // 读入指令
    while(1) {
        instruction tmp;
        cin>>tmp.type;
        if(!cin) break;
        cin>>tmp.operand_target>>tmp.operand_a>>tmp.operand_b;
        instructions.push_back(tmp);
    }
}

// 通过名称name，获取对应保留站/缓冲区的引用
station& get_station_ref(const string &name){
    if(name[0] == 'L') return load_stations[name[4] - '1'];
    else if(name[0] == 'S') return store_stations[name[4] - '1'];
    else if(name[0] == 'A') return reservation_stations[name[3] - '1'];
    else if(name[0] == 'M') return reservation_stations[name[4] - '1' + 3];
    else printf("Invalid station name : %s\n", name.c_str()),exit(-1);
}

// 通过名称name，获取对应fu的引用
Fu& get_Fu_ref(const string &name){
    if(name[0] == 'F') {
        int index = 0;
        for(int i = 1;i < name.size();++i) index = index * 10 + name[i] - '0';
        if(!(index&1)&&index<12) return Fus[index>>1];
    }
    printf("Invalid Fu name : %s\n", name.c_str()),exit(-1);
}

// 通过保留站/缓冲区from_station的计算结果对FU部件进行赋值，由于该值并未准备好
// 故在FU中保存from_station的名称、设置标志位并向from_station中插入一个对其计算结果的引用
void set_fu_value(const string &Fu_name,station& from_station){
    Fu& fu = get_Fu_ref(Fu_name);
    fu.value = from_station.name;
    from_station.references.push_back(reference(&fu.value,&fu.value,&fu.flag));
}

// 通过FU对保留站/缓冲区中一个具有标志位的值进行赋值，此时如果FU的值已经准备好则直接赋值
// 否则使用FU中值的来源保留站/缓冲区from_station的名称对齐赋值、设置标志位并向from_station中插入对应引用
void set_station_value(const string &Fu_name,string & target,int &flag){
    Fu& fu = get_Fu_ref(Fu_name);
    string value = fu.get_value();
    if(value.size()) target = value,flag = 1;
    else{
        station& from_station = get_station_ref(target = fu.value);
        from_station.references.push_back(reference(&target,&target,&flag));
    }
}

// 此函数与上一个函数的区别在于使用变量Q同时充当引用中标志位和校验值，目标值另设
// 而上一个函数target同时充当引用中校验值和目标值，标志位另设
void set_station_value(const string &Fu_name,string& V,string& Q){
    Fu& fu = get_Fu_ref(Fu_name);
    string value = fu.get_value();
    if(value.size()) V = value;
    else{
        V = "";
        station& from_station = get_station_ref(Q = fu.value);
        from_station.references.push_back(reference(&Q,&V));
    }
}

// 尝试发射指令，若成功发射则返回true，否则返回false
bool try_issue_instruction(int &id,int now_cycle){
    // 获取当前指令
    instruction& inst = instructions[id];
    int flag = 0;
    // 若当前指令有对应的空闲保留站/缓冲区，则发射指令
    if(inst.type == "LD"&&free_load_stations.size()){
        int station_id = free_load_stations.front();free_load_stations.pop();
        load_station& station = load_stations[station_id];
        station.busy = true;
        flag = true;
        station.inst_id = id++;
        inst.iss_time = now_cycle;
        string address = inst.operand_b;
        if(inst.operand_a != "0") address = inst.operand_a + address;
        station.address = address;
        set_fu_value(inst.operand_target,station);
        block_load_stations.push_back(station_id);
    }else if(inst.type == "SD"&&free_store_stations.size()){
        int station_id = free_store_stations.front();free_store_stations.pop();
        store_station& station = store_stations[station_id];
        station.busy = true;
        flag = true;
        station.inst_id = id++;
        inst.iss_time = now_cycle;
        string address = inst.operand_b;
        if(inst.operand_a != "0") address = inst.operand_a + address;
        station.address = address;
        set_station_value(inst.operand_target,station.value,station.flag);
        block_store_stations.push_back(station_id);
    }else if((inst.type == "ADDD"||inst.type == "SUBD")&&free_add_stations.size()){
        int station_id = free_add_stations.front();free_add_stations.pop();
        reservation_station& station = reservation_stations[station_id];
        station.busy = true;
        flag = true;
        station.inst_id = id++;
        station.op = inst.type;
        inst.iss_time = now_cycle;
        set_station_value(inst.operand_a,station.vi,station.qi);
        set_station_value(inst.operand_b,station.vj,station.qj);
        set_fu_value(inst.operand_target,station);
        block_add_stations.push_back(station_id);
    }else if((inst.type == "MULTD"||inst.type == "DIVD")&&free_mult_stations.size()){
        int station_id = free_mult_stations.front();free_mult_stations.pop();
        reservation_station& station = reservation_stations[station_id];
        station.busy = true;
        flag = true;
        station.inst_id = id++;
        station.op = inst.type;
        inst.iss_time = now_cycle;
        set_station_value(inst.operand_a,station.vi,station.qi);
        set_station_value(inst.operand_b,station.vj,station.qj);
        set_fu_value(inst.operand_target,station);
        block_mult_stations.push_back(station_id);
    }else if(inst.type == "BNEZ"){
        //如果指令为BNEZ，则直接发射，但直至其执行完毕(消耗一个周期)才能发射下一条指令
        //故就地执行该指令
        if(inst.iss_time) inst.exec_time = now_cycle,++id;
        else inst.iss_time = now_cycle;
        flag = true;
    }

    //返回指令是否发射成功
    return flag;
}

// 通过buf输出上一个本质不同的周期状态及其范围，同时将当前周期状态存入buf
void print_cycle(int now,int last_cycle,string &buf){

    // 若上一个周期状态存在，则输出
    if(buf.size()){
        cout<<"//Cycle_"+ to_string(last_cycle);

        //输出上一个周期状态的范围
        if(now>last_cycle+1) cout<<"-"<<to_string(now-1);
        cout<<endl<<buf;
    }

    // 将当前周期状态存入buf
    buf = "";
    for(int i = 0;i < 3;++i) buf+="//"+string(load_stations[i]);
    for(int i = 0;i < 3;++i) buf+="//"+string(store_stations[i]);
    for(int i = 0;i < 5;++i) buf+="//"+string(reservation_stations[i]);
    buf+="//";
    for(int i = 0;i < 7;++i) buf+=Fus[i];
    buf+="\n\n";
}

// 输出指令执行情况
void print_instructions(){
    for(auto &e:instructions) e.print();
}

int main(){
    // 重定向输入输出
    cout<<"Enter input file's name (1 - input1.txt, other - input2.txt)\n";
    int tmp = 1;
    cin>>tmp;
    if(tmp != 1) freopen("input2.txt","r",stdin),freopen("output2.txt","w",stdout);
    else freopen("input1.txt","r",stdin),freopen("output1.txt","w",stdout);
    
    // 初始化,其中next_cycle表示下一个可能导致系统状态发生变化的周期
    init();
    int last_cycle = 0,next_cycle = !instructions.empty(),instruction_id = 0;
    string buf = "";

    // 模拟发射执行指令
    while(1){
        // 跳过中间系统状态不变的周期，flag为当前周期处理完毕后周期状态是否发生变化
        int now_cycle = next_cycle,flag = 0;

        // 如果提交队列不为空，则允许队头提交项写CDB，并将其释放资源
        if(exec_comp_stations.size()){
            flag = 1;
            exec_comp_stations.front().write_cdb(now_cycle);
            exec_comp_stations.pop();
        }

        // 如果指令队列为全部发射完成，则尝试发射下一条指令
        if(instruction_id < instructions.size()) flag |= try_issue_instruction(instruction_id,now_cycle);

        // 处理优先队列中，于当前周期完成的任务，生成对应的提交项并加入队列以等待写CDB
        while(exec_stations.size()&&exec_stations.top().exec_comp_cycle == now_cycle){
            executing_task tmp = exec_stations.top(); exec_stations.pop();
            exec_comp_stations.push(tmp.exec_complete());
        }

        // 如果功能部件空闲，则从对应blocking队列中选取操作数准备完毕的保留站/缓冲区占用并执行指令
        while(semaphore_load&&block_load_stations.size()){
            int station_id = block_load_stations[0];block_load_stations.erase(block_load_stations.begin());
            exec_stations.push(executing_task(&free_load_stations,station_id,&load_stations[station_id],&semaphore_load,now_cycle));
        }
        for(int i = 0;i < block_store_stations.size();i++){
            if(!semaphore_store) break;
            if(store_stations[block_store_stations[i]].check()){
                int station_id = block_store_stations[i];block_store_stations.erase(block_store_stations.begin()+i),--i;
                exec_stations.push(executing_task(&free_store_stations,station_id,&store_stations[station_id],&semaphore_store,now_cycle));
            }
        }
        for(int i = 0;i < block_add_stations.size();i++){
            if(!semaphore_add) break;
            if(reservation_stations[block_add_stations[i]].check()){
                int station_id = block_add_stations[i];block_add_stations.erase(block_add_stations.begin()+i),--i;
                exec_stations.push(executing_task(&free_add_stations,station_id,&reservation_stations[station_id],&semaphore_add,now_cycle));
            }
        }
        for(int i = 0;i < block_mult_stations.size();i++){
            if(!semaphore_mult) break;
            if(reservation_stations[block_mult_stations[i]].check()){
                int station_id = block_mult_stations[i];block_mult_stations.erase(block_mult_stations.begin()+i),--i;
                exec_stations.push(executing_task(&free_mult_stations,station_id,&reservation_stations[station_id],&semaphore_mult,now_cycle));
            }
        }

        // 如果当前周期状态发生变化，则能确认并输出上一个不同的周期状态及其范围
        if(flag) print_cycle(now_cycle,last_cycle,buf),last_cycle = now_cycle;
        
        // 指令未发射完全或有保留项/缓冲区等待写CDB，则下个周期系统状态将可能发生变化
        // 否则，若仍有执行中的任务，则下个系统状态可能发生变化的周期为之后最早执行完成的任务的完成周期
        // 否则，系统状态不再发生变化，终止运行
        if(instruction_id < instructions.size()||exec_comp_stations.size())next_cycle = now_cycle + 1;
        else if(exec_stations.size()) next_cycle = exec_stations.top().exec_comp_cycle;
        else break;
    }

    // 输出最后一个不同的周期状态及其范围,及最终指令执行情况
    print_cycle(last_cycle,last_cycle,buf);
    print_instructions();
    return 0;
}