#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dirent.h"
#include "QFileDialog"
#include "iostream"
#include "fstream"
#include "QVector"
#include "QTimer"
#include "unistd.h"



using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Init();
}

MainWindow::~MainWindow()
{
    delete ui,timer;
}

void MainWindow::Init(){
    //绘图
    PaintGraph(ui->memCustomPlot);
    PaintGraph(ui->cpuCustomPlot);
    PaintGraph(ui->diskCustomPlot);
    PaintGraph(ui->netCustomPlot);

    //安装定时器
    connect(&procTimer,SIGNAL(timeout()),this,SLOT(DispProc()));
    connect(&timer,SIGNAL(timeout()),this,SLOT(DispMemStat()));//mem
    connect(&timer,SIGNAL(timeout()),this,SLOT(DispCpuStat()));//cpu
    connect(&timer,SIGNAL(timeout()),this,SLOT(DispDiskStat()));//disk
    connect(&timer,SIGNAL(timeout()),this,SLOT(DispNetStat()));//net

    //启动定时器
    procTimer.start(1000);
    timer.start(1000);
}

//传入一个目录结构体，取出目录名查看是否为数字
int MainWindow::IsPid(const struct dirent *entry){
    const char *p;
    for(p=entry->d_name;*p;p++){   //d_name是一个字符数组
        if(!isdigit(*p))
            return 0;
    }
    return 1;
}


void MainWindow::DispProc(){
    DIR *procDir;
    QFile file;
    QString str;
    int cRow,par1,par2,procTime,preProcTime;
    string comm;
    QString qComm;
    double usage;
    char task_stat;
    int pid,ppid,pgid,sid,tty_nr,tty_pgrp,task_flag,min_flt,cmin_flt,maj_flt,cmaj_flt,utime,stime,
            cutime,cstime,priority,nice,num_threads,it_real_value,start_time,vsize,rss;
    int curProcTime;
    struct dirent *entry;
    char path[5+256+5]; // /proc + d_name + /stat
    QMap<char,QString> statMap;

    //init map
    statMap.insert('R',"running");
    statMap.insert('S',"sleeping");
    statMap.insert('D',"disk sleep");
    statMap.insert('T',"stopped");
    statMap.insert('Z',"zombie");
    statMap.insert('X',"dead");
    statMap.insert('I',"idle");

    ui->listWidget->clear();
    QListWidgetItem *title = new QListWidgetItem("PID\t"+QString::fromUtf8("状态")+'\t'+QString::fromUtf8("CPU")
                                                 +'\t'+QString::fromUtf8("内存")+'\t'+QString::fromUtf8("优先级")
                                                 +'\t'+QString::fromUtf8("名称"));
    QFont font;
    font.setBold(true);
    title->setFont(font);
    ui->listWidget->addItem(title);

    //打开/proc目录
    procDir = opendir("/proc");  
    while(entry=readdir(procDir)){
        if(!IsPid(entry))
            continue;
        snprintf(path,sizeof(path),"/proc/%s/stat",entry->d_name);

        //打开对应pid目录的stat文件
        ifstream fin(path,ios::in);
        fin >> pid;
        preProcTime = procPreTimeMap.value(pid);//先从Map中取出上一次cpu时间
        fin>>comm>>task_stat>>ppid>>pgid>>sid>>tty_nr>>tty_pgrp>>task_flag>>min_flt>>cmin_flt>>maj_flt>>cmaj_flt>>utime>>stime>>
                cutime>>cstime>>priority>>nice>>num_threads>>it_real_value>>start_time>>vsize>>rss;

        //删除括号
        qComm = QString::fromStdString(comm);
        par1 = qComm.indexOf('(');
        par2 = qComm.indexOf(')');
        qComm = qComm.mid(par1+1,par2-par1-1);
        qComm.trimmed();

        //计算cpu使用率
        //将当前进程pid和time存入Map中，下次执行根据pid找到上次时间
        curProcTime = utime + stime + cutime + cstime;
        procPreTimeMap.insert(pid,curProcTime);
        procTime = curProcTime - preProcTime;//当前进程使用cpu总时间
        usage = procTime*1.0 / curTime * 100; //进程使用时间/cpu总时间
        QListWidgetItem *item = new QListWidgetItem(QString::number(pid)+"\t"+statMap.value(task_stat)
                                                    +'\t'+QString::number(usage,'f',1)+'%'+'\t'
                                                    +QString::number(rss*4/1024)+'M'+'\t'
                                                    +QString::number(priority)+"\t"+qComm);
        if(pid % 2){
            item->setBackgroundColor(QColor(248,248,255));
        }
        //将正在运行的进程放在前面
        if(task_stat == 'R'){
            ui->listWidget->insertItem(1,item);
        }else{
            ui->listWidget->addItem(item);
        }
        fin.close();
    }
}


void MainWindow::PaintGraph(QCustomPlot *customPlot){
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(QColor(0,0,255)));   //曲线颜色
    customPlot->legend->setVisible(false);  //不显示图例
    customPlot->axisRect()->setupFullAxesBox();//四边轴显示
    customPlot->xAxis->setTickLabels(false);
    customPlot->graph(0)->setBrush(QBrush(QColor(0,0,255,50)));//填充
    customPlot->yAxis->setRange(0,100);
}

void MainWindow::DispCpuStat(){
    string tmp;
    double usage;
    int preTotal, user, nice, system, preIdle,iowait, irq, softirq, stealstolen, guest, guest_nice;
    QString curStr,cpuInfo,processes,procRun,procBlock;
    QFile file;
    int pos,runTime;
    //将上一次的CPU时间拷贝
    preTotal = curTotal;
    preIdle = curIdle;
    //cpu信息
    file.setFileName("/proc/cpuinfo");
    if(!file.open(QIODevice::ReadOnly)){
        cout << "cpuinfo can not open!" << endl;
        return;
    }
    while(1){
        curStr = file.readLine();
        pos = curStr.indexOf("model name");
        if(pos != -1){
            cpuInfo = curStr.mid(pos+12,40);
            break;
        }
    }
    file.close();
    file.setFileName("/proc/stat");
    if(!file.open(QIODevice::ReadOnly)){
        cout << "stat can not open!" << endl;
        return;
    }
    while(1){
        curStr = file.readLine();
        pos = curStr.indexOf("processes");
        if(pos != -1){
            processes = curStr.mid(pos+10,curStr.length()-10);
            processes.trimmed();
        }
        else if(pos = curStr.indexOf("procs_running"),pos != -1){
            procRun = curStr.mid(pos+13,5);
            procRun.trimmed();
        }
        else if(pos = curStr.indexOf("procs_blocked"),pos != -1){
            procBlock = curStr.mid(pos+14,curStr.length()-14);
            procBlock.trimmed();
            break;
        }
    }
    file.close();

    //计算CPU利用率
    ifstream fin("/proc/stat", ios::in);
    fin >> tmp >> user >> nice >> system >> curIdle >> iowait >> irq >> softirq >> stealstolen >> guest >> guest_nice;
    fin.close();
    //重新赋值给全局变量
    curTotal = user + nice + system + curIdle + iowait + irq + softirq + stealstolen + guest + guest_nice;
    curTime = curTotal - preTotal;
    usage = (curTotal + preIdle - preTotal - curIdle) * 1.0 / curTime *100;
    runTime = (user + nice +system +curIdle + iowait + irq + softirq)/100;
    //second to std
    int H = runTime / (60*60);
    int M = (runTime- (H * 60 * 60)) / 60;
    int S = (runTime - (H * 60 * 60)) - M * 60;
    QString hour = QString::number(H);
    if (hour.length() == 1) hour = "0" + hour;
    QString min = QString::number(M);
    if (min.length() == 1) min = "0" + min;
    QString sec = QString::number(S);
    if (sec.length() == 1) sec = "0" + sec;
    QString qTime = hour + ":" + min + ":" + sec;

    //UI
    ui->cpuName->setText(cpuInfo);
    ui->cpuUse->setText(QString::number(usage,'f',1)+'%');
    ui->cpuProcRun->setText(procRun);
    ui->cpuProcCreate->setText(processes);
    ui->cpuProcBlock->setText(procBlock);
    ui->cpuTime->setText(qTime);
    ui->cpuCustomPlot->graph(0)->addData(key,usage);
    ui->cpuCustomPlot->graph(0)->removeDataBefore(key-20);//显示20s内
    ui->cpuCustomPlot->xAxis->setRange(key,20,Qt::AlignRight);
    ui->cpuCustomPlot->replot();
}


void MainWindow::DispMemStat(){
    QString curStr; //当前读取字符串
    QFile file;
    int pos;
    QString memTotal,memFree;
    float nMemTotal,nMemFree,nMemUsed;
    float value;

    file.setFileName("/proc/meminfo");
    if(!file.open(QIODevice::ReadOnly)){
        cout << "meminfo can not open!" << endl;
        return;
    }
    while(1){       
        curStr = file.readLine();
        pos = curStr.indexOf("MemTotal");
        if(pos != -1){
            memTotal = curStr.mid(pos+10,curStr.length()-13);    //mid(start pos,num)
            memTotal = memTotal.trimmed();                      //清除空格
            nMemTotal = memTotal.toFloat() / 1024 / 1024;       //M
        }
        else if(pos = curStr.indexOf("MemFree") , pos != -1){
            memFree = curStr.mid(pos+9,curStr.length()-12);
            memFree = memFree.trimmed();
            nMemFree = memFree.toFloat() / 1024 /1024;
            break;
        }

     }
    nMemUsed = nMemTotal - nMemFree;
    value = nMemUsed*100 / nMemTotal;

    //update UI
    key++;
    ui->memCustomPlot->graph(0)->addData(key,value);
    ui->memCustomPlot->graph(0)->removeDataBefore(key-20);//显示20s内
    ui->memCustomPlot->xAxis->setRange(key,20,Qt::AlignRight);
    ui->memCustomPlot->replot();
    ui->memProgressBar->setValue((int)value);
    ui->memProgressBar->setMaximum(100);
    ui->memAll->setText(QString::number(nMemTotal,'f',2)+'G');
    ui->memAvailable->setText(QString::number(nMemFree,'f',2)+'G');
    ui->memUsing->setText(QString::number(nMemUsed,'f',2)+'G');
    ui->memUse->setText(QString::number(nMemUsed,'f',2)+'G');
}


//loop:挂载镜像     sr0光驱       sda:磁盘      sda5:第一逻辑分区
void MainWindow::DispDiskStat(){
    string temp;
    int preTranTime;
    float usage;
    preTranTime = curTranTime;
    //设备号 编号 设备 读完成次数 合并完成次数 读扇区次数 读操作花费毫秒数 写完成次数 合并写完成次数 写扇区次数
    //写操作花费的毫秒数 正在处理的输入/输出请求数 输入/输出操作花费的毫秒数 输入/输出操作花费的加权毫秒数。
    ifstream fin("/proc/diskstats", ios::in);
    while(!fin.eof()){
        fin >> temp >> temp >> temp;
        if(temp.size()==3 && temp == "sda"){
            fin >> temp >> temp >> temp >> temp >> temp >> temp >> temp
                    >> temp >> temp >> curTranTime;
            getline(fin,temp);//将当前字符串赋值给temp
        }else{
            getline(fin,temp);
        }
    }
    fin.close();
    usage = (curTranTime - preTranTime) *1.0 / 1 / 1000 * 100;

    //UI
    ui->diskCustomPlot->graph(0)->addData(key,usage);
    ui->diskCustomPlot->graph(0)->removeDataBefore(key-20);//显示20s内
    ui->diskCustomPlot->xAxis->setRange(key,20,Qt::AlignRight);
    ui->diskCustomPlot->replot();
}


void MainWindow::DispNetStat(){
    string temp;
    int preRecvByte,preSendByte;
    float recvRate,sendRate,throughput;
    preRecvByte = curRecvByte;//曾经的当前就是以前
    preSendByte = curSendByte;
    ifstream fin("/proc/net/dev", ios::in);
    while(1){
        fin >> temp;
        if(temp == "enp0s3:"){
            fin >> curRecvByte >> temp >> temp >> temp >> temp >> temp >> temp >> temp >> curSendByte;
            break;
        }else{
            getline(fin,temp);
        }
    }
    fin.close();
    recvRate = (curRecvByte - preRecvByte)*1.0 / 1024 * 8; //   kbps
    sendRate = (curSendByte - preSendByte)*1.0 / 1024 * 8;
    throughput = recvRate + sendRate;
    string ipAddr,device,type;
    fin.open("/proc/net/arp",ios::in);
    getline(fin,temp);
    fin >> ipAddr >> type >> temp >> temp >> temp >> device;
    fin.close();
    if(type == "0x1"){
        type = "Etherney";
    }else if(type == "0x17"){
        type = "Metricom starmode IP";
    }
    else{
        type = "Frame Relay DLCI";
    }

    //UI
    ui->netRecv->setText(QString::number(recvRate,'f',1)+"Kbps");
    ui->netSend->setText(QString::number(sendRate,'f',1)+"Kbps");
    ui->netDev->setText(QString::fromStdString(device));
    ui->netIP->setText(QString::fromStdString(ipAddr));
    ui->netType->setText(QString::fromStdString(type));
    ui->netCustomPlot->graph(0)->addData(key,throughput);
    ui->netCustomPlot->graph(0)->removeDataBefore(key-20);//显示20s内
    ui->netCustomPlot->xAxis->setRange(key,20,Qt::AlignRight);
    ui->netCustomPlot->yAxis->setRange(0,500);
    ui->netCustomPlot->replot();


}

//事件处理
void MainWindow::on_listWidget_itemClicked(QListWidgetItem *item){
    procTimer.stop();
}

void MainWindow::on_pushButton_clicked(){
    QListWidgetItem *item = ui->listWidget->currentItem();
    QString proMsg = item->text();
    proMsg = proMsg.section("\t",0,0);//切割出PID
    system("kill " + proMsg.toLatin1());
    QMessageBox::warning(this,tr("kill"),QString::fromUtf8("该进程已被结束！"),QMessageBox::Yes);
    procTimer.start(1000);
}

void MainWindow::on_tabWidget_currentChanged(int index){
    procTimer.start(1000);
}

void MainWindow::on_poweroffButton_clicked(){
    system("poweroff");
}

void MainWindow::on_restartButton_clicked(){
    system("restart");
}
